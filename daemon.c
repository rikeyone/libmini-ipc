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
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <pthread.h>
#include "debug.h"

#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

/*
 * lockfile - /tmp/{daemonname}.pid
 */
static char lockfile[128];

/*
 * logfile - /tmp/{daemonname}.log
 */
static char logfile[128];

/*
 * lock status would be unlock if process exit
 */
static int file_lock(int fd)
{
	struct flock fl;

	/*
	 * Get lock for the whole file
	 */
	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	return(fcntl(fd, F_SETLK, &fl));
}

static int is_daemon_running(char *name)
{
	int		fd;
	char	buf[16];

	snprintf(lockfile, 128, "/tmp/%s.pid", name);
	fd = open(lockfile, O_RDWR|O_CREAT, LOCKMODE);
	if (fd < 0) {
		err_exit("can't open %s: %s\n", lockfile, strerror(errno));
	}
	/*
	 * Get the file lock
	 */
	if (file_lock(fd) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			close(fd);
			return 1;
		}
		err_exit("can't lock %s: %s\n", lockfile, strerror(errno));
	}
	/*
	 * Cut the file to offset 0
	 */
	if (ftruncate(fd, 0) < 0)
		err_exit("ftruncate error\n");

	sprintf(buf, "%ld", (long)getpid());
	/*
	 * Update pid to the lock file
	 */
	if (write(fd, buf, strlen(buf)+1) < 0)
		err_exit("write error\n");
	return 0;
}

int daemon_init(char *appname)
{
	int					i, fd0, fd1, fd2;
	pid_t				pid;
	struct rlimit		rl;
	struct sigaction	sa;

	umask(0);

	/*
	 * Fork() and setsid()
	 * Make child process become a session leader
	 * to lose controlling tty.
	 */
	if ((pid = fork()) < 0)
		err_exit("fork failed, can't fork\n");
	else if (pid != 0)
		exit(0);

	setsid();

	/*
	 * Ensure future opens won't allocate controlling TTYs.
	 */
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		err_exit("sigaction can't ignore SIGHUP\n");

	/* Fork again */
	if ((pid = fork()) < 0)
		err_exit("fork failed, can't fork\n");
	else if (pid != 0)
		exit(0);

	/*
	 * Change the current working directory to the root so
	 * we won't prevent file systems from being unmounted.
	 */
	if (chdir("/") < 0)
		err_exit("can't change directory to /\n");

	/*
	 * Close all opened fd.
	 */
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
		err_exit("can't get file limit\n");

	if (rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;
	for (i = 0; i < rl.rlim_max; i++)
		close(i);

	/*
	 * Normally, we should attach file descriptors 0, 1, and 2 to /dev/null.
	 * But here, we just redirect stdout and stderr to logfile for log print
	 */
	snprintf(logfile, 128, "/tmp/%s.log", appname);
	fd0 = open("/dev/null", O_RDWR);
	fd1 = open(logfile, O_RDWR | O_APPEND | O_CREAT, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH));
	fd2 = dup(1);
	if (fd0 != 0 || fd1 != 1 || fd2 != 2)
		err_exit("open stdin/stdout/stderr error!\n");

	/*
	 * Make sure only one daemon process is running.
	 */
	if (is_daemon_running(appname))
		err_exit("daemon already running, just exit\n");

	pr_info("Daemon start!\n");

	return 0;
}
