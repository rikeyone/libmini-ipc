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

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "looper.h"
#include "debug.h"

static void *looper_loop(void *private)
{
	struct looper *looper = (struct looper *)private;
	struct list_node *head = &looper->head;
	struct list_node *node;
	struct msg_entity *msg;

	pr_info("looper start, name: %s\n", looper->name);
	while(looper->running){
		pthread_mutex_lock(&looper->lock);
		if (list_is_empty(head)){
			pthread_cond_wait(&looper->condition, &looper->lock);
			pthread_mutex_unlock(&looper->lock);
			/*
			* There are two conditions to get here:
			* first, list state changed from empty to nonempty
			* second, someone calls looper_stop to stop the looper
			* So, needs to re-check "running" state when thread is
			* waken up, "continue" will do this job.
			*/
			continue;
		}
		node = head->next;
		msg = list_node_entry(node, struct msg_entity, node);
		list_node_del(node);
		pr_debug("handler, msg id = %d\n", msg->msg_id);
		pthread_mutex_unlock(&looper->lock);
		if (looper->loop_cb)
			looper->loop_cb(msg->data);
		if (looper->free_cb)
			looper->free_cb(msg->data);
		free(msg);
	}

	return NULL;
}

static int looper_start(struct looper *looper)
{
	int ret;

	pthread_mutex_lock(&looper->lock);

	if(looper->running){
		ret = -1;
		pr_err("looper is already running, name =%s!\n", looper->name);
		goto out;
	}

	looper->running = true;
	ret = pthread_create(&looper->tid, NULL, looper_loop, (void *)looper);
	if(ret < 0){
		looper->running = false;
		pr_err("pthread create fail!, %s\n", strerror(errno));
	}

out:
	pthread_mutex_unlock(&looper->lock);
	return ret;
}

static int looper_stop(struct looper *looper)
{
	if(!looper->running){
		pr_err("looper is already stoped, name = %s!\n", looper->name);
		return -1;
	}

	looper->running = false;
	pthread_cond_signal(&looper->condition);
	pthread_join(looper->tid, NULL);

	/*
	* After the looper is stoped, messages in the list should
	* be cleaned carefully.
	*/
	pthread_mutex_lock(&looper->lock);
	if(list_is_empty(&looper->head) == false){
		struct list_node *node;
		struct msg_entity *msg;
		list_for_each_node(node, &looper->head) {
			msg = list_node_entry(node, struct msg_entity, node);
			list_node_del(node);
			if (looper->free_cb)
				looper->free_cb(msg->data);
			free(msg);
			msg = NULL;
		}
	}
	pthread_mutex_unlock(&looper->lock);
	return 0;
}

static void looper_dispatch(struct looper *looper, void *data)
{
	struct msg_entity *msg;

	if(NULL == looper){
		return;
	}

	pthread_mutex_lock(&looper->lock);
	msg = (struct msg_entity *)malloc(sizeof(struct msg_entity));
	if (msg == NULL){
		pr_err("malloc failed, %s!\n", strerror(errno));
		if(data && looper->free_cb)
		    looper->free_cb(data);
		pthread_mutex_unlock(&looper->lock);
		return;
	}
	msg->msg_id = looper->msg_id++;
	msg->data = data;
	INIT_LIST_NODE(&msg->node);
	/*
	* If list is empty, looper thread is sleeping wait for signal,
	* so should wake it up first and don't be worried for the next
	* while check for list state, because looper won't get the lock
	* which is held by dispatch thread now.
	*/
	if(list_is_empty(&looper->head))
		pthread_cond_signal(&looper->condition);
	list_node_add_tail(&msg->node, &looper->head);
	pr_debug("dispatch, msg id = %d\n", msg->msg_id);
	pthread_mutex_unlock(&looper->lock);


}

struct looper *looper_create(msg_handler loop_cb, msg_free free_cb, const char *name)
{
	struct looper *looper;

	looper = malloc(sizeof(struct looper));
	if(NULL == looper){
		pr_err("create looper fail, %s\n", strerror(errno));
		return NULL;
	}

	snprintf(looper->name, sizeof(looper->name), "%s", (name ? name : "default"));
	pthread_mutex_init(&looper->lock, NULL);
	pthread_cond_init(&looper->condition, NULL);
	INIT_LIST_NODE(&looper->head);
	looper->loop_cb = loop_cb;
	looper->free_cb = free_cb;
	looper->start = looper_start;
	looper->stop = looper_stop;
	looper->dispatch = looper_dispatch;
	looper->running = false;
	looper->msg_id = 0;

	return looper;
}

void looper_destory(struct looper *looper)
{
	if (NULL == looper)
		return;

	looper_stop(looper);
	pthread_mutex_destroy(&looper->lock);
	pthread_cond_destroy(&looper->condition);
	free(looper);
}

