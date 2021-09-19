#include "dat.h"
#include "fns.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

int
populatesaddr(char *addr, struct sockaddr_in *saddr)
{
	saddr->sin_family = AF_INET;
	if(inet_pton(AF_INET, addr, &saddr->sin_addr) != 1) return -1;
	return 0;
}

int
iptou32(char *ip, unsigned int *u32)
{
	struct sockaddr_in s;
	if(populatesaddr(ip, &s) < 0) return -1;
	*u32 = s.sin_addr.s_addr;
	return 0;
}

int
u32toip(unsigned int u32, char *ipbuf)
{
	struct in_addr ia;

	ia.s_addr = u32;
	if(!inet_ntop(AF_INET, &ia, ipbuf, INET_ADDRSTRLEN)){
		*ipbuf = '\0';
		return -1;
	}
	return 0;
}

int
attemptinternetcon(void)
{
	int fd, err;
	struct sockaddr_in sa;

	err = 0;
	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(fd < 0) FAILASSERT("socket", err, fail);

	if(populatesaddr(TESTADDR, &sa) < 0)
		FAILASSERT("populatesaddr", err, sockfail);

	sa.sin_port = htons(TESTPORT);
	if(connect(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0)
		err = -1;

sockfail:
	close(fd);
fail:
	return err;
}
