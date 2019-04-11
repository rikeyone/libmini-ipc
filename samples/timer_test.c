#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "timer.h"
#include "debug.h"

#define ONE_SECOND_US 1000000
struct timer_wrapper timer;

void timer_callback(void *data)
{
	pr_info("timeout: %ld\n", (long) time(NULL));
}

int main()
{
	char buf[10];
	timer_init(&timer, timer_callback, NULL);
	timer_start(&timer, ONE_SECOND_US, 0);

	read(STDIN_FILENO, buf, 10);
	pr_info("read returns\n");

	return 0;
}
