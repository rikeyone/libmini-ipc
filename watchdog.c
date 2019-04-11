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
#include "watchdog.h"

#define DEBUG_TAG_NAME "wdt"
#define DEBUG_PRINT_ENABLE 1

int software_watchdog_init(struct watchdog_timer *t)
{
    /* Create the timer */
	memset(&t->sev, 0, sizeof(struct sigevent));
	t->sev.sigev_notify = SIGEV_SIGNAL;
	t->sev.sigev_signo = SIGABRT;
	t->sev.sigev_value.sival_ptr = &t->timerid;

    if (timer_create(CLOCK_REALTIME, &t->sev, &t->timerid) == -1) {
        pr_err("timer_create fail\n");
		return -1;
	}
	t->created = 1;

    pr_info("timer ID is 0x%lx\n", (long) t->timerid);
	return 0;
}

int software_watchdog_reset(struct watchdog_timer *t, int second)
{
    /* Start the timer */
	t->interval = second;
    t->its.it_value.tv_sec = second;
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

int software_watchdog_start(struct watchdog_timer *t, int second)
{
	return software_watchdog_reset(t, second);
}

int software_watchdog_feed(struct watchdog_timer *t)
{
	return software_watchdog_reset(t, t->interval);
}

void software_watchdog_remove(struct watchdog_timer *t)
{
	if (t->created)
		timer_delete(t->timerid);
	memset(t, 0, sizeof(struct watchdog_timer));
}
