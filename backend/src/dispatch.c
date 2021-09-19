#include "dat.h"
#include "fns.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int rpipe = -1;
int wpipe = -1;
int killed = 0;
int mode = -1;
struct darray *children = NULL;
struct darray *dystrs = NULL;

#define LOGSIGNAL(SIGNO) \
	fprintf(stderr, "[CAUGHT SIGNAL %s]\n", sys_siglist[SIGNO])

#define LAUNCHJOB(JOB)                                          \
	do{                                                    \
		pid = fork();                                   \
		if(pid < 0) FAILASSERT("fork", err, pfail);     \
		else if(pid == 0)                               \
			JOB();                                  \
		else if(appenddarray(children, &pid) < 0){     \
			kill(pid, SIGTERM);                     \
			FAILASSERT("appenddarray", err, dfail); \
		}                                              \
	}while(0)

/* Parent deinitialization */

void
deinitdispatch(void)
{
	int i;

	if(killed == 0){
		killed = 1;
		for(i = 0; i < (int)children->size; i++){
			int pid;

			pid = *(int *)getdarray(children, i);
			if(!pid)
				fprintf(stderr,
					"Failed to get valid child to kill, "
					"idx %d\n",
					i);
			kill(pid, SIGTERM);
		}

		freedarray(children);
		children = NULL;

		close(rpipe);
		close(wpipe);
		rpipe = wpipe = -1;

		if(mode == OFFLINEMODE){
			deinitpktq();
			deinitrc();
			deinittun();
		}
		stoplisten();
		deinitnetifc();
	}
}

void
catchsig(int signo)
{
	LOGSIGNAL(signo);
	if(signal(SIGCHLD, SIG_DFL) == SIG_ERR)
		fprintf(stderr, "Failed to unregister SIGCHLD\n");

	if(signal(SIGINT, SIG_DFL) == SIG_ERR)
		fprintf(stderr, "Failed to unregister SIGINT\n");

	printf("Unregistered signals, doing deinit\n");
	deinitdispatch();
	printf("Deinit complete, bye!\n");
	exit(-1);
}

int
wrcmd(struct dispatchcmdplus *c)
{
	if(write(wpipe, c, sizeof(struct dispatchcmdplus)) ==
	   sizeof(struct dispatchcmdplus))
		return 0;
	return -1;
}

int
rdcmd(struct dispatchcmdplus *c)
{
	if(read(rpipe, c, sizeof(struct dispatchcmdplus)) ==
	   sizeof(struct dispatchcmdplus))
		return 0;
	return -1;
}

/* Various children */

void
listendispatch(void)
{
	int err;
	err = 0;

	for(;;){
		int qt;
		char pkt[MTU], *dev;
		unsigned int devip, from;
		struct dispatchcmdplus c;
		struct dispatchcmd *cmini;

		qt = rdlisten(pkt, MTU, &dev, &devip, &from);
		if(qt < 0) FAILASSERT("rdlisten", err, fail);

		memset(&c, 0, sizeof(struct dispatchcmdplus));
		cmini = (struct dispatchcmd *)pkt;
		memcpy(&c, cmini, sizeof(struct dispatchcmd));
		c.dev = dev;
		c.devip = devip;
		c.from = from;
		if(wrcmd(&c) < 0) FAILASSERT("wrcmd", err, fail);
	}

fail:
	exit(err);
}

void
tundispatch(void)
{
	int err;

	for(;;){
		int qt;
		struct dispatchcmdplus c;
		char pkt[MTU];

		qt = readtun(pkt, MTU);
		if(qt < 0) FAILASSERT("readtun", err, fail);

		memset(&c, 0, sizeof(struct dispatchcmdplus));
		c.d = NEWRT;
		memcpy(c.rp.pkt, pkt, qt);
		c.rp.pktsz = qt;
		printf("DEBUG: Enqueueing packet of size %u into queue\n",
		       c.rp.pktsz);
		if(wrcmd(&c) < 0) FAILASSERT("wrcmd", err, fail);
	}

fail:
	exit(err);
}

void
pipedispatch(void)
{
	char c;
	int fd, err;

	err = 0;
	fd = open(FRONTENDPIPE, O_RDONLY);
	if(fd < 0) FAILASSERT("open", err, fail);

	if(read(fd, &c, sizeof(char)) != 1) FAILASSERT("read", err, fdfail);

fdfail:
	close(fd);
fail:
	exit(err);
}

int
readysetdispatch(void)
{
	int err, pid, tp[2];

	mode = (attemptinternetcon() < 0) ? OFFLINEMODE : INTERNETMODE;

	if(mode == OFFLINEMODE){
		printf("MODE: Offline mode\n");
		if(inittun(DEFTUNADDR) < 0) FAILASSERT("inittun", err, fail);
		if(initrc() < 0) FAILASSERT("initrc", err, tunfail);
		if(initpktq() < 0) FAILASSERT("initpktq", err, rcfail);
	}else
		printf("MODE: Online mode\n");

	if(initnetifc() < 0) FAILASSERT("initnetifc", err, pqfail);
	if(startlisten() < 0) FAILASSERT("startlisten", err, nifail);

	if(pipe(tp) < 0) FAILASSERT("pipe", err, lisfail);
	rpipe = tp[0];
	wpipe = tp[1];

	children = mkdarray();
	if(!children) FAILASSERT("mkdarray", err, pfail);
	dystrs = mkdarray();
	if(!dystrs) FAILASSERT("mkdarray", err, chldfail);

	if(mode == OFFLINEMODE) LAUNCHJOB(tundispatch);
	LAUNCHJOB(listendispatch);
	LAUNCHJOB(pipedispatch);

	if(signal(SIGCHLD, catchsig) == SIG_ERR)
		FAILASSERT("signal", err, dfail);
	if(signal(SIGINT, catchsig) == SIG_ERR)
		FAILASSERT("signal", err, dfail);

	doroute();

dfail:
	deinitdispatch();
	return err;
chldfail:
	freedarray(children);
	children = NULL;
pfail:
	close(rpipe);
	close(wpipe);
	rpipe = wpipe = -1;
lisfail:
	stoplisten();
nifail:
	deinitnetifc();
pqfail:
	if(mode == INTERNETMODE) goto fail;
	deinitpktq();
rcfail:
	deinitrc();
tunfail:
	deinittun();
fail:
	return err;
}
