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
#ifdef __cplusplus
export "C" {
#endif

#ifndef __IPCLIB_H__
#define __IPCLIB_H__

#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <stdint.h>
#include "looper.h"

#define MSG_QUEUE_NAME_SIZE 64
#define MSG_QUEUE_MAX_SIZE 4096
#define MSG_CONTENT_SIZE 256

struct ipc_msg {
	int type;
	/*
	* below members are for message request
	*/
	char source[MSG_QUEUE_NAME_SIZE];
	char content[MSG_CONTENT_SIZE];
};

struct ipc_reply {
	int type;
	/*
	* below members are for message reply
	*/
	int result;
	char content[MSG_CONTENT_SIZE];
};

/*
 * ipclib_watchdog_init - init watchdog settings
 * @second: init watchdog timeout time
 * */
int ipclib_watchdog_init(int second);

/*
 * ipclib_watchdog_feed - feed watchdog when receive watchdog message
 *
 * This function should be called from APP MSG HANDLER
 * */
int ipclib_watchdog_feed(void);

/*
* ipclib_send_msg_async - send a async message
* @name: app name
* @msg: request message
*/
int ipclib_send_msg_async(char *name, struct ipc_msg *msg);

/*
* ipclib_send_msg_sync - send a sync message and will wait for reply
* @name: app name
* @msg: request message
* @reply: reply which need to send
*/
int ipclib_send_msg_sync(char *name, struct ipc_msg *msg, struct ipc_reply *reply);

/*
* ipclib_send_reply - send a reply for a sync message
* @msg: request message
* @reply: reply which need to send
*
* This function will set reply->type equal (msg->type + MSG_TYPE_REPLY_BASE)
*/
int ipclib_send_reply(struct ipc_msg *msg, struct ipc_reply *reply);

/**
* ipclib_main_loop - application wait and dispatcher/handle messages
*
* This function should never return unless exceptions occur.
*/
void ipclib_main_loop(void);

/*
* ipclib_stop_loop - stop main loop and exit the application
*/
void ipclib_stop_loop(void);

/*
* ipclib_init - ipclib initialize
* Applications should call this function before using ipclib_mainloop and ipclib_deinit
*
* @name: app name
* @handler: data handle callback in looper thread
*/
int ipclib_init(char *name, msg_handler handler);

/*
* ipclib_deinit - ipclib de-initialize
* Applications should call this function when exit
*/
void ipclib_deinit(void);


/*
 * MSG TYPE DEFINITIONS
 * */

/*
 * > 10000: reserved for replied message
 *
 * REPLY_TYPE=BASE+MSG_TYPE
 *
 * */
#define MSG_TYPE_REPLY_BASE 10000

/* 9000-9999: common use */
#define MSG_TYPE_WATCHDOG 9000


/* 0-8999: applications use */

#endif //__IPCLIB_H__

#ifdef __cplusplus
}
#endif
