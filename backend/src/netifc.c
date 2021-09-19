#include "dat.h"
#include "fns.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BCASTIP "255.255.255.255"

int *wsocks = NULL;
int nwsocks = -1;
int rsock = -1;
struct ht *addrtosock = NULL;

int
bcastnetifc(char *msg, unsigned int msgsz)
{
	int err, i;
	struct sockaddr_in sa;

	err = 0;
	memset(&sa, 0, sizeof(struct sockaddr_in));

	/* the only time this ever gets called is during
	 * indiscriminate broadcasting, searching if a
	 * given a
	 */
	for(i = 0; i < nwsocks; i++){
		if(populatesaddr(BCASTIP, &sa) < 0)
			FAILASSERT("populatesaddr", err, fail);
		sa.sin_port = htons(RECVPORT);

		if(sendto(wsocks[i], msg, msgsz, 0, (struct sockaddr *)&sa,
			  sizeof(struct sockaddr_in)) < 0)
			FAILASSERT("sendto", err, fail);
	}

fail:
	return err;
}

int
fwdpktnetifc(char *pkt, unsigned int pktsz)
{
	int err;
	struct sockaddr_in sa;

	err = 0;
	memset(&sa, 0, sizeof(struct sockaddr_in));

	sa.sin_family = AF_INET;
	sa.sin_port = 0;
	sa.sin_addr.s_addr = ((struct iphdr *)pkt)->daddr;

	if(sendto(rsock, pkt, pktsz, 0, (struct sockaddr *)&sa,
		  sizeof(struct sockaddr_in)) < 0)
		FAILASSERT("sendto", err, fail);

fail:
	return err;
}

int
fwdudpnetifc(char *pkt, unsigned int pktsz, char *from, char *to)
{
	int fd, *fdp, err;
	unsigned int fromu32;
	struct sockaddr_in sa;

	err = 0;
	memset(&sa, 0, sizeof(struct sockaddr_in));

	if(iptou32(from, &fromu32) < 0) FAILASSERT("iptou32 from", err, fail);

	fdp = getht(addrtosock, fromu32);
	if(!fdp)
		FAILASSERT("received packet on non-existent ip address", err,
			   fail);
	fd = *fdp;

	/* the only time this ever gets called is during
	 * indiscriminate broadcasting, searching if a
	 * given a
	 */
	if(populatesaddr(to, &sa) < 0) FAILASSERT("populatesaddr", err, fail);
	sa.sin_port = htons(RECVPORT);

	printf("Sending to %s from %s\n", to, from);

	if(sendto(fd, pkt, pktsz, 0, (struct sockaddr *)&sa,
		  sizeof(struct sockaddr_in)) < 0)
		FAILASSERT("sendto", err, fail);
fail:
	return err;
}

int
initnetifc(void)
{
	int err, i;
	struct ifaddrs *ifas, *ifa;

	addrtosock = initht(FNV1AHF);
	if(!addrtosock) FAILASSERT("addrtosock", err, fail);

	err = 0;
	rsock = socket(PF_INET, SOCK_RAW, IPPROTO_RAW);
	if(rsock < 0) FAILASSERT("rsock socket", err, hfail);

	if(getifaddrs(&ifas) < 0) FAILASSERT("getifaddrs", err, rsfail);

	wsocks = calloc(MAXNETIFCS, sizeof(int));
	if(!wsocks) FAILASSERT("calloc", err, ifafail);
	nwsocks = 0;

	for(ifa = ifas; ifa; ifa = ifa->ifa_next){
		int fd;
		struct sockaddr_in *ifcaddr;

		ifcaddr = (struct sockaddr_in *)ifa->ifa_addr;
		if(!ifcaddr || ifcaddr->sin_family != AF_INET) continue;
		else if(memcmp(ifa->ifa_name, "lo", 2) == 0)
			continue;

		{
			unsigned int ta;

			if(iptou32(DEFTUNADDR, &ta) < 0) continue;
			else if(ta == ifcaddr->sin_addr.s_addr)
				continue;
		}

		{
			int dontdoit;
			struct sockaddr_in sa;

			dontdoit = 1;
			memset(&sa, 0, sizeof(struct sockaddr_in));

			fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if(fd < 0) FAILASSERT("socket", err, wsockfail);

			if(setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &dontdoit,
				      sizeof(int)) < 0)
				FAILASSERT("setsockopt -> so_broadcast", err,
					   wsockfail);

			sa.sin_family = AF_INET;
			sa.sin_addr.s_addr = ifcaddr->sin_addr.s_addr;
			if(bind(fd, (struct sockaddr *)&sa,
				sizeof(struct sockaddr_in)) < 0)
				FAILASSERT("bind", err, wsockfail);
		}

		wsocks[nwsocks++] = fd;
		if(insertht(addrtosock, ifcaddr->sin_addr.s_addr,
			    wsocks + nwsocks - 1) < 0)
			FAILASSERT("insertht", err, wsockfail);
		else if(nwsocks > MAXNETIFCS)
			FAILASSERT("too many interfaces set up", err,
				   wsockfail);
	}

	freeifaddrs(ifas);
	return err;

wsockfail:
	for(i = 0; i < nwsocks; i++) close(wsocks[i]);
	free(wsocks);
	wsocks = NULL;
	nwsocks = 0;
ifafail:
	freeifaddrs(ifas);
rsfail:
	close(rsock);
	rsock = -1;

hfail:
	deinitht(addrtosock);
	addrtosock = NULL;
fail:
	return err;
}

void
deinitnetifc(void)
{
	int i;

	for(i = 0; i < nwsocks; i++) close(wsocks[i]);
	free(wsocks);
	wsocks = NULL;
	nwsocks = 0;

	close(rsock);
	rsock = -1;
	deinitht(addrtosock);
	addrtosock = NULL;
}
