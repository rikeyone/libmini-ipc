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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include "debug.h"
#include "timer.h"

typedef void (*sigev_function) (union sigval);

int timer_init(struct timer_wrapper *t, timer_cb func, void *data)
{

    /* Create the timer */
	memset(&t->sev, 0, sizeof(struct sigevent));
    t->sev.sigev_notify = SIGEV_THREAD;
	t->sev.sigev_notify_function = (sigev_function) func;
	t->sev.sigev_value.sival_ptr = data;
	t->sev.sigev_notify_attributes = NULL;//&attr;

    if (timer_create(CLOCK_REALTIME, &t->sev, &t->timerid) == -1) {
        pr_err("timer_create fail\n");
		return -1;
	}
	t->created = 1;

    pr_info("timer ID is 0x%lx\n", (long) t->timerid);
	return 0;
}

int timer_start(struct timer_wrapper *t, uint64_t usec, uint32_t oneshot)
{
    /* Start the timer */
    t->its.it_value.tv_sec = usec / 1000000;
    t->its.it_value.tv_nsec = (usec % 1000000) * 1000;
	/*
	 * timer only run once
	 */
	if (oneshot) {
    	t->its.it_interval.tv_sec = 0;
    	t->its.it_interval.tv_nsec = 0;
	} else {
	/*
	 * periodic timer
	 */
		t->its.it_interval.tv_sec = t->its.it_value.tv_sec;
		t->its.it_interval.tv_nsec = t->its.it_value.tv_nsec;
	}

	/*
	 * second arg: 0 means relative timeout
	 * second arg: TIMER_ABSTIME absolute timeout
	 *
	 * Here use relative time
	 */
	if (timer_settime(t->timerid, 0, &t->its /*new value*/, NULL /*old value*/) == -1) {
		pr_err("timer_settime fail\n");
		return -1;
	}

	return 0;
}

int timer_stop(struct timer_wrapper *t)
{
    t->its.it_value.tv_sec = 0;
    t->its.it_value.tv_nsec = 0;
    t->its.it_interval.tv_sec = 0;
    t->its.it_interval.tv_nsec = 0;

	/*
	 * second arg: 0 means relative timeout
	 * second arg: TIMER_ABSTIME absolute timeout
	 *
	 * Here use relative time
	 */
	if (timer_settime(t->timerid, 0, &t->its /*new value*/, NULL /*old value*/) == -1) {
		pr_err("timer_settime fail\n");
		return -1;
	}

	return 0;
}

void timer_remove(struct timer_wrapper *t)
{
	if (t->created)
		timer_delete(t->timerid);
	memset(t, 0, sizeof(struct timer_wrapper));
}
