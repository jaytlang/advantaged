#include "dat.h"
#include "fns.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEVNETTUN "/dev/net/tun"

char *tunname = NULL;
int tunfd = -1;

int
bringuptun(char *ifname, char *addr)
{
	int fd, err;
	struct ifreq ifr;
	struct sockaddr_in *saddr;

	err = 0;
	saddr = (struct sockaddr_in *)&ifr.ifr_addr;
	memset(&ifr, 0, sizeof(struct ifreq));
	memcpy(ifr.ifr_name, ifname, IFNAMSIZ);

	if(populatesaddr(addr, saddr) < 0)
		FAILASSERT("populatesaddr", err, fail);

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if(fd < 0) FAILASSERT("socket", err, fail);

	if(ioctl(fd, SIOCSIFADDR, &ifr) < 0)
		FAILASSERT("SIOCSIFADDR", err, sockfail);

	if(inet_pton(AF_INET, "255.255.255.255", &saddr->sin_addr) != 1)
		FAILASSERT("inet_pton for SIOCSIFNETMASK", err, sockfail);

	if(ioctl(fd, SIOCSIFNETMASK, &ifr) < 0)
		FAILASSERT("SIOCSIFNETMASK", err, sockfail);

	if(ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)
		FAILASSERT("SIOCGIFFLAGS", err, sockfail);
	ifr.ifr_flags |= IFF_UP;
	if(ioctl(fd, SIOCSIFFLAGS, &ifr) < 0)
		FAILASSERT("SIOCSIFFLAGS", err, sockfail);

sockfail:
	close(fd);
fail:
	return err;
}

int
inittun(char *addr)
{
	struct ifreq ifr;
	int fd, err;

	err = 0;
	if((fd = open(DEVNETTUN, O_RDWR)) < 0) FAILASSERT("open", err, fail);

	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	*ifr.ifr_name = '\0';

	if((ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0)
		FAILASSERT("ioctl", err, badioctl);

	if(bringuptun(ifr.ifr_name, addr) < 0)
		FAILASSERT("bringuptun", err, badioctl);

	tunname = calloc(1, IFNAMSIZ * sizeof(char));
	if(!tunname) FAILASSERT("calloc", err, badioctl);
	memcpy(tunname, ifr.ifr_name, IFNAMSIZ);

	tunfd = fd;

badioctl:
	if(err < 0) close(fd);
fail:
	return err;
}

int
readtun(char *pkt, unsigned int pktsz)
{
	struct iphdr *pkthdr;
	int cnt;

	memset(pkt, 0, sizeof(char) * pktsz);

doread:
	cnt = read(tunfd, pkt, pktsz);
	if(cnt < 0) FAILASSERT("read", cnt, fail);

	pkthdr = (struct iphdr *)pkt;
	if(pkthdr->version != 4) goto doread;

fail:
	return cnt;
}

void
deinittun(void)
{
	free(tunname);
	close(tunfd);
	tunname = NULL;
	tunfd = -1;
}
