#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "debug.h"
#include "ipclib.h"


void app_msg_handler(void *data)
{
	struct ipc_msg *msg = (struct ipc_msg *)data;

	switch(msg->type) {
	case MSG_TYPE_WATCHDOG:
		pr_info("watchdog message received!\n");
		ipclib_watchdog_feed();
		break;
	default:
		pr_info("unexpected message received! type:%d\n", msg->type);
		ipclib_stop_loop();
		break;
	}
}

int main(int argc, char *argv[])
{
	int ret;

	ret = ipclib_init(argv[1], app_msg_handler);
	if (ret < 0)
		err_exit("ipclib_init error\n");

	ipclib_watchdog_init(20);
	ipclib_main_loop();
	ipclib_deinit();
	return 0;
}
