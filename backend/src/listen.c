#include "dat.h"
#include "fns.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int sfd = -1;
struct ifaddrs *ifas = NULL;
struct ht *addrtodev = NULL;

#define CMBUFSZ 4096

int
startlisten(void)
{
	struct sockaddr_in sa;
	struct ifaddrs *ifa;
	int err, opt;

	err = 0;
	memset(&sa, 0, sizeof(struct sockaddr_in));
	sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sfd < 0) FAILASSERT("socket", err, fail);

	opt = 1;
	if(setsockopt(sfd, IPPROTO_IP, IP_PKTINFO, &opt, sizeof(int)) < 0)
		FAILASSERT("setsockopt IP_PKTINFO", err, sfail);

	if(getifaddrs(&ifas) < 0) FAILASSERT("getifaddrs", err, sfail);

	addrtodev = initht(FNV1AHF);
	if(!addrtodev) FAILASSERT("initht", err, ifafail);

	for(ifa = ifas; ifa; ifa = ifa->ifa_next){
		struct sockaddr_in *ifcaddr;
		unsigned int ta;

		ifcaddr = (struct sockaddr_in *)ifa->ifa_addr;
		if(!ifcaddr || ifcaddr->sin_family != AF_INET) continue;
		else if(memcmp(ifa->ifa_name, "lo", 3) == 0)
			continue;

		if(iptou32(DEFTUNADDR, &ta) < 0)
			FAILASSERT("iptou32", err, hfail);
		else if(ta == ifcaddr->sin_addr.s_addr)
			continue;

		if(insertht(addrtodev, ifcaddr->sin_addr.s_addr,
			    ifa->ifa_name) < 0)
			FAILASSERT("insertht", err, hfail);
	}

	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port = htons(RECVPORT);

	if(bind(sfd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0)
		FAILASSERT("bind", err, sfail);

	return 0;

hfail:
	deinitht(addrtodev);
	addrtodev = NULL;
ifafail:
	freeifaddrs(ifas);
	ifas = NULL;
sfail:
	close(sfd);
	sfd = -1;
fail:
	return err;
}

int
rdlisten(char *pktbuf, unsigned int pktsz, char **dev, unsigned int *devip,
	 unsigned int *from)
{
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct sockaddr_in sa;
	char cmbuf[CMBUFSZ];
	int out;

	out = 0;
	memset(&iov, 0, sizeof(struct iovec));
	memset(&msg, 0, sizeof(struct msghdr));
	memset(&sa, 0, sizeof(struct sockaddr_in));
	memset(pktbuf, 0, sizeof(char) * pktsz);

	iov.iov_base = pktbuf;
	iov.iov_len = pktsz;
	msg.msg_name = &sa;
	msg.msg_namelen = sizeof(struct sockaddr_in);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmbuf;
	msg.msg_controllen = CMBUFSZ;

	if((out = recvmsg(sfd, &msg, 0)) < 0) FAILASSERT("recvmsg", out, fail);
	else if(out == (int)pktsz)
		FAILASSERT("packet too large", out, fail);
	else
		((char *)msg.msg_iov->iov_base)[out] = '\0';

	for(cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)){
		struct in_pktinfo *pi;
		struct in_addr addr;
		char tip[INET_ADDRSTRLEN];

		if(cmsg->cmsg_level != IPPROTO_IP ||
		   cmsg->cmsg_type != IP_PKTINFO)
			continue;
		pi = (struct in_pktinfo *)CMSG_DATA(cmsg);
		addr = pi->ipi_spec_dst;

		if(u32toip(addr.s_addr, tip) < 0)
			FAILASSERT("unintelligble ip address on reception", out,
				   fail);
		printf("Received data over %s\n", tip);

		*dev = getht(addrtodev, addr.s_addr);
		if(!(*dev))
			FAILASSERT("received on non-existent local addr", out,
				   fail);
		*devip = addr.s_addr;
		*from = sa.sin_addr.s_addr;
		return out;
	}

	FAILASSERT("didn't get ip_pktinfo control msg", out, fail);
fail:
	return out;
}

void
stoplisten(void)
{
	deinitht(addrtodev);
	addrtodev = NULL;
	freeifaddrs(ifas);
	ifas = NULL;
	close(sfd);
	sfd = -1;
}
