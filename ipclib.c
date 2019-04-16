/*
 * Copyright (C) 2019 xiehaocheng <xiehaocheng127@163.com>
 *
 * All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <time.h>
#include <sys/time.h>
#include "debug.h"
#include "looper.h"
#include "ipclib.h"
#include "watchdog.h"
#include "timer.h"

struct ipc_lib {
	char name[MSG_QUEUE_NAME_SIZE+1];
	char buf[MSG_QUEUE_MAX_SIZE];
	mqd_t mqd;
	pthread_t tid;
	pthread_mutex_t lock;
    pthread_cond_t condition;
	struct looper *looper;
	struct timer_wrapper timer;
	struct ipc_reply reply;
	struct watchdog_timer wdt;
	int wdt_timeout;
	int exit;
	uint32_t send_sync_count;
	uint32_t send_async_count;
	uint32_t recv_reply_count;
	uint32_t recv_request_count;
};

static struct ipc_lib *ipclib;

mqd_t mq_rw_create(char *name, int maxsize)
{
	mqd_t mq;
	struct mq_attr attr;

	attr.mq_flags = 0;       /* BLOCK */
	attr.mq_maxmsg = 10;    /* msg count */
	attr.mq_msgsize = maxsize;  /* msg queue size in bytes */
	attr.mq_curmsgs = 0; /* current msg count in queue */

	mq = mq_open(name, O_CREAT | O_RDWR, 0644, &attr);
	if (mq == (mqd_t) -1) {
		pr_err("mq_open failed, %s\n", strerror(errno));
	}
	pr_info("msg queue mqd:%d\n", mq);
	return mq;
}

mqd_t mq_rd_open(char *name)
{
	mqd_t mq;

	mq = mq_open(name, O_RDONLY);
	if (mq == (mqd_t) -1) {
		pr_err("mq_open failed, %s\n", strerror(errno));
	}
	return mq;
}

int mq_recv_msg(mqd_t mq, char *buf, int maxsize)
{
	int bytes_read;

again:
	bytes_read = mq_receive(mq, buf, maxsize, NULL);
    if (bytes_read < 0) {
		if (errno == EINTR)
			goto again;
		else {
			pr_err("mq_receive failed, %s\n", strerror(errno));
			return -1;
		}
	}
	return bytes_read;
}


int mq_send_msg(mqd_t mq, char *buf, int length)
{
	int bytes_read;

again:
	bytes_read = mq_send(mq, buf, length, 0);
	if (bytes_read < 0) {
		if (errno == EINTR)
			goto again;
		else {
			pr_err("mq_send failed, %s\n", strerror(errno));
			return -1;
		}
	}
	return bytes_read;
}

int mq_send_msg_timeout(mqd_t mqd, void *buf, int length)
{
	int bytes_read;
	int count = 0;
	struct timespec expire_time;

	do {
		clock_gettime(CLOCK_REALTIME, &expire_time);
		expire_time.tv_nsec += 300000000; //300ms
		expire_time.tv_sec += expire_time.tv_nsec / 1000000000;
		expire_time.tv_nsec = expire_time.tv_nsec % 1000000000;
		bytes_read = mq_timedsend(mqd, (void *)buf, length, 0, &expire_time);
		if (bytes_read < 0) {
			if (errno == EINTR || errno == ETIMEDOUT)
				continue;
			else {
				pr_err("mq_timedsend failed, %s\n", strerror(errno));
				return -1;
			}
		} else if (bytes_read == 0) /* success return 0 */
			break;
	} while (count++ < 10);

	return bytes_read;
}

/****************************************************************/

/*
 * timer_callback - timer callback
 *
 * Send a MSG_WATCHDOG message to APP MSG QUEUE
 * */
static void timer_callback(void *data)
{
	struct ipc_lib *ipc = (struct ipc_lib *)data;
	struct ipc_msg msg = {0};
	struct timespec expire_time;

	clock_gettime(CLOCK_REALTIME, &expire_time);
	pr_debug("tv_sec:%ld, tv_nsec:%ld\n", expire_time.tv_sec, expire_time.tv_nsec);
	if (ipc) {
		msg.type = MSG_TYPE_WATCHDOG;
		if (mq_send_msg_timeout(ipc->mqd, (void *)&msg, sizeof(struct ipc_msg)) < 0)
			pr_err("watchdog message send fail\n");
	}
}

/*
 * ipclib_watchdog_init - init watchdog settings
 * @second: init watchdog timeout time
 * */
int ipclib_watchdog_init(int second)
{
	struct ipc_lib *ipc = ipclib;

	if (!ipc) {
		pr_info("ipclib didn't init\n");
		return 0;
	}

	ipc->wdt_timeout = second;
	if (timer_init(&ipc->timer, timer_callback, (void *)ipc) < 0) {
		pr_err("timer_init fail\n");
		return -1;
	}
	if(software_watchdog_init(&ipc->wdt) < 0) {
		pr_err("watchdog timer init fail\n");
		return -1;
	}
	return 0;
}

/*
 * ipclib_watchdog_start - start watchdog timer
 *
 * Used internally by ipclib_main_loop
 * */
static int ipclib_watchdog_start(void)
{
	struct ipc_lib *ipc = ipclib;
	uint64_t usec;

	if (!ipc) {
		pr_info("ipclib didn't init\n");
		return 0;
	}

	if (ipc->wdt_timeout) {
		usec = ipc->wdt_timeout * MAX_USEC_PER_SECOND;
		usec = usec / 4 * 3;
		pr_info("watchdog feed interval usec:%ld\n", usec);
		if (timer_start(&ipc->timer, usec, PERIODIC_TIMER) < 0)
			return -1;

		if (software_watchdog_start(&ipc->wdt, ipc->wdt_timeout) < 0)
			return -1;
	}
	return 0;
}

/*
 * ipclib_watchdog_feed - feed watchdog when receive watchdog message
 *
 * This function should be called from APP MSG HANDLER
 *
 * */
int ipclib_watchdog_feed(void)
{
	struct ipc_lib *ipc = ipclib;

	if (!ipc) {
		pr_info("ipclib didn't init\n");
		return 0;
	}
	software_watchdog_feed(&ipc->wdt);

	return 0;
}


/*
 * ipclib_watchdog_remove - remvoe watchdog timer
 * */
static int ipclib_watchdog_remove(void)
{
	struct ipc_lib *ipc = ipclib;

	if (!ipc) {
		pr_err("ipclib didn't init\n");
		return -1;
	}
	software_watchdog_remove(&ipc->wdt);
	timer_remove(&ipc->timer);
	return 0;
}


/*
* ipclib_send_msg_async - send a async message
* @name: app name
* @msg: request message
*/
int ipclib_send_msg_async(char *name, struct ipc_msg *msg)
{
	char path[MSG_QUEUE_NAME_SIZE+1] = {0};
	mqd_t mqd;

	snprintf(path, MSG_QUEUE_NAME_SIZE, "/%s", name);
	mqd = mq_rd_open(path);
	if (mqd == (mqd_t)(-1)) {
		pr_err("mq_rd_open fail\n");
		return -1;
	}
	return mq_send_msg_timeout(mqd, (void *)msg, sizeof(*msg));
}

/*
* ipclib_send_msg_sync - send a sync message and will wait for reply
* @msg: request message
* @reply: reply which need to send
*/
int ipclib_send_msg_sync(char *name, struct ipc_msg *msg, struct ipc_reply *reply)
{
	int bytes_read;
	struct ipc_lib *ipc = ipclib;
	struct timespec expire_time;
	char path[MSG_QUEUE_NAME_SIZE+1] = {0};
	mqd_t mqd;

	/*
	* reset ipclib reply structure to zero
	*/
	memset(&ipc->reply, 0, sizeof(struct ipc_reply));

	/*
	* fill message source mq name
	*/
	snprintf(msg->source, MSG_QUEUE_NAME_SIZE, "%s", ipc->name);

	/*
	* open target application message queue
	*/
	snprintf(path, MSG_QUEUE_NAME_SIZE, "/%s", name);
	mqd = mq_rd_open(path);
	if (mqd == (mqd_t)(-1)) {
		pr_err("mq_rd_open fail\n");
		return -1;
	}

	bytes_read = mq_send_msg_timeout(mqd, (void *)msg, sizeof(*msg));
	if (bytes_read < 0) {
		pr_err("ipclib_send_msg failed, %s\n", strerror(errno));
		return -1;
	}

	/*
	* block wait for reply signal from receive thread
	* wait reply 3 second as timeout
	*/
	pthread_mutex_lock(&ipc->lock);
	while(ipc->reply.type != msg->type + MSG_TYPE_REPLY_BASE){
		clock_gettime(CLOCK_REALTIME, &expire_time);
		expire_time.tv_sec += 3;
		if (pthread_cond_timedwait(&ipc->condition,
					&ipc->lock, &expire_time) < 0) {
			/*
			 * Handle errno as a fault except EINTR
			 */
			if (errno == EINTR) {
				continue;
			} else {
				pr_err("pthread_cond_timedwait fail, %s\n",
						strerror(errno));
				pthread_mutex_unlock(&ipc->lock);
				return -1;
			}
		}
	}

	/*
	 * copy reply to the argument pointed address
	*/
	memcpy(reply, &ipc->reply, sizeof(struct ipc_reply));
	pthread_mutex_unlock(&ipc->lock);
	return bytes_read;
}

/*
* ipclib_send_reply - send a reply for a sync message
* @msg: request message
* @reply: reply which need to send
*
* This function will set reply->type equal (msg->type + MSG_TYPE_REPLY_BASE)
*/
int ipclib_send_reply(struct ipc_msg *msg, struct ipc_reply *reply)
{
	mqd_t mqd;

	/**
	* set reply->type, should start from MSG_TYPE_REPLY_BASE
	*/
	reply->type = msg->type + MSG_TYPE_REPLY_BASE;

	/*
	* get source mq name from request message
	*/
	mqd = mq_rd_open(msg->source);
	if (mqd == (mqd_t)(-1)) {
		pr_err("mq_rd_open fail\n");
		return -1;
	}


	return mq_send_msg_timeout(mqd, (void *)reply, sizeof(*reply));
}

/**
* ipclib_receive_msg - receive messages.
* @ipc: ipclib structure point
* @msg: note that it is a point to a point (struct ipc_msg **)
*/
static int ipclib_receive_msg(struct ipc_lib *ipc, struct ipc_msg **msg)
{
	int bytes_read = -1;
	struct timespec expire_time;

	while(!ipc->exit) {
		clock_gettime(CLOCK_REALTIME, &expire_time);
		expire_time.tv_nsec += 500000000; //500ms
		expire_time.tv_sec += expire_time.tv_nsec / 1000000000;
		expire_time.tv_nsec = expire_time.tv_nsec % 1000000000;
		//pr_debug("ipc->mqd:%d, tv_sec:%ld, tv_nsec:%ld\n", ipc->mqd, expire_time.tv_sec, expire_time.tv_nsec);
		bytes_read = mq_timedreceive(ipc->mqd, (char *)ipc->buf,
				MSG_QUEUE_MAX_SIZE, NULL, &expire_time);
		if (bytes_read < 0) {
			if (errno == EINTR || errno == ETIMEDOUT)
				continue;
			else {
				pr_err("mq_timedreceive failed, %s\n", strerror(errno));
				return -1;
			}
		} else if (bytes_read > 0)
			break;
	}
	*msg = (struct ipc_msg *)ipc->buf;
	return bytes_read;
}

/**
* ipclib_handle_reply - handle message reply.
* @ipc: ipclib structure point
* @reply: ipc message reply point
*/
static void ipclib_handle_reply(struct ipc_lib *ipc, struct ipc_reply *reply)
{
	pthread_mutex_lock(&ipc->lock);
	memcpy(&ipc->reply, reply, sizeof(struct ipc_reply));
	pthread_cond_signal(&ipc->condition);
	pthread_mutex_unlock(&ipc->lock);
}

/**
* ipclib_dispatcher - handle and post message.
* @ipc: ipclib structure point
* @msg: ipc message point
*/
static void ipclib_dispatcher(struct ipc_lib *ipc, struct ipc_msg *msg)
{
	struct ipc_msg *data;

	/*
	* reply message don't need to post, handle it here.
	*/
	if (msg->type >= MSG_TYPE_REPLY_BASE) {
		ipclib_handle_reply(ipc, (struct ipc_reply *)msg);
		return;
	}

	/**
	* messages except IPC_MSG_REPLY should be posted to
	* looper thread to handle.
	*/
	data = (struct ipc_msg *)malloc(sizeof(*data));
	if (!data) {
		pr_err("ipclib: ipc msg malloc fail\n");
		return;
	}
	memcpy(data, (void *)msg, sizeof(struct ipc_msg));
	ipc->looper->dispatch(ipc->looper, (void *)data);
}

/**
* ipclib_free_msg_cb - message free callback used by looper.
* @data: message point which malloced in ipclib_dispatcher()
*
* This callback should set to looper, so looper will free message
* memory after using it.
*/
static void ipclib_free_msg_cb(void *data)
{
	if (data)
		free(data);
}

/**
* ipclib_main_loop - application wait and dispatcher/handle messages
*
* This function should never return unless exceptions occur.
*/
void ipclib_main_loop(void)
{
	struct ipc_msg *msg;

	if (!ipclib) {
		pr_err("ipclib: should init first!\n");
		return;
	}
	if (ipclib_watchdog_start() < 0) {
		pr_err("ipclib: watchdog start fail!\n");
		return;
	}

	while (ipclib_receive_msg(ipclib, &msg) > 0){
		ipclib_dispatcher(ipclib, msg);
	}

	/**
	* out of main loop, application is going to exit
	*/
}

/*
* ipclib_stop_loop - stop main loop and exit the application
*/
void ipclib_stop_loop(void)
{
	if (!ipclib) {
		pr_err("ipclib: should init first!\n");
		return;
	}
	ipclib->exit = 1;
}


/*
* ipclib_init - ipclib initialize
* Applications should call this function before using ipclib_mainloop and ipclib_deinit
*
* @name: app name
* @handler: data handle callback in looper thread
*/
int ipclib_init(char *name, msg_handler handler)
{
	struct ipc_lib *ipc;

	if (ipclib) {
		pr_info("ipclib already inited\n");
		return 0;
	}

	ipc = (struct ipc_lib *) malloc(sizeof(struct ipc_lib));
	if (!ipc)
		err_exit("ipclib: malloc fail!\n");

	memset(ipc, 0, sizeof(struct ipc_lib));
	pthread_mutex_init(&ipc->lock, NULL);
	pthread_cond_init(&ipc->condition, NULL);

	/* fill msg queue path*/
	snprintf(ipc->name, MSG_QUEUE_NAME_SIZE, "/%s", name);
	pr_info("ipclib: create posix message queue at:%s\n", ipc->name);

	/* create msg queue */
	ipc->mqd = mq_rw_create(ipc->name, MSG_QUEUE_MAX_SIZE);
	if (ipc->mqd < 0)
		err_exit("ipclib: create message queue fail!\n");

	/* create looper */
	ipc->looper = looper_create(handler, ipclib_free_msg_cb, name);
	if (ipc->looper < 0)
		err_exit("ipclib: create looper fail!\n");

	/* start looper to handle message in looper thread */
	if (ipc->looper->start(ipc->looper) < 0)
		err_exit("ipclib: looper start fail!\n");

	/* set ipc to global point variable ipclib */
	ipclib = ipc;
	return 0;
}

/*
* ipclib_deinit - ipclib de-initialize
* Applications should call this function when exit
*/
void ipclib_deinit(void)
{
	if (!ipclib) {
		pr_info("ipclib doesn't need to deinit\n");
	}
	/* remove timers */
	ipclib_watchdog_remove();
	/* destory looper */
	looper_destory(ipclib->looper);
	/* delete msg queue */
	mq_unlink(ipclib->name);
	/* free ipclib  */
	free(ipclib);
	ipclib = NULL;
}
