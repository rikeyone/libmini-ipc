#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "debug.h"
#include "siglib.h"

static sigfunc sigaction_func;

static void *thread_sigfun(void *arg)
{
	int err, signo;
	sigset_t	waitset,mask;

	/*
	 * Two signals can't be blocked:
	 * SIGKILL,SIGSTOP - can't block
	 *
	 * Good applications should not block these signals:
	 * SIGFPE,SIGILL,SIGSEGV,SIGBUS - shouldn't block
	 *
	 * */
	sigfillset(&mask);
	sigdelset(&mask, SIGKILL);
	sigdelset(&mask, SIGSTOP);
	sigdelset(&mask, SIGFPE);
	sigdelset(&mask, SIGILL);
	sigdelset(&mask, SIGSEGV);
	sigdelset(&mask, SIGBUS);
	sigdelset(&mask, SIGABRT); /* NOTE: Shouldn't block, this is for watchdog */
	if ((err = pthread_sigmask(SIG_SETMASK, &mask, NULL)) != 0)
		err_exit("SIG_BLOCK error\n");

	/*
	 * sigwait for other signals
	 * */
	sigfillset(&waitset);
	sigdelset(&waitset, SIGKILL);
	sigdelset(&waitset, SIGSTOP);
	sigdelset(&waitset, SIGFPE);
	sigdelset(&waitset, SIGILL);
	sigdelset(&waitset, SIGSEGV);
	sigdelset(&waitset, SIGBUS);
	sigdelset(&waitset, SIGABRT); /* NOTE: Shouldn't block, this is for watchdog */
	for (;;) {
		err = sigwait(&waitset, &signo);
		if (err != 0) {
			err_exit("sigwait failed\n");
		}
		sigaction_func(signo);
		pr_debug("sigwait received signal:%d\n", signo);
	}
	return(0);
}

void set_signal_thread(sigfunc func)
{
	int err;
	sigset_t	mask;
	pthread_t	tid;

	sigaction_func = func;
	/*
	 * Block all signals in main thread
	 */
	sigfillset(&mask);
	if ((err = pthread_sigmask(SIG_SETMASK, &mask, NULL)) != 0)
		err_exit("SIG_BLOCK error\n");

	/*
	 * Create a child thread to handle SIGHUP and SIGTERM.
	 */
	err = pthread_create(&tid, NULL, thread_sigfun, 0);
	if (err != 0)
		err_exit("can't create thread\n");
}

int set_signal(int signo, sigfunc func)
{
	struct sigaction act, oact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (signo == SIGALRM) {
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;
#endif
	} else {
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;
#endif
	}
	if (sigaction(signo, &act, &oact) < 0) {
		printf("sigaction error,%s\n",strerror(errno));
		return 1;
	}
	return 0;
}
