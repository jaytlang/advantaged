#include "dat.h"
#include "fns.h"

#include <arpa/inet.h>
#include <linux/route.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#define WCIP "0.0.0.0"
#define WHIP "255.255.255.255"

#define RTDST(rt)     (struct sockaddr_in *)&((rt)->rt_dst)
#define RTGATEWAY(rt) (struct sockaddr_in *)&((rt)->rt_gateway)
#define RTGENMASK(rt) (struct sockaddr_in *)&((rt)->rt_genmask)
#define DEFMETRIC     10

struct ht *rc = NULL;
struct rcentry *defroute = NULL;

int
mkrtentry(struct rcentry *ent, struct rtentry *rtent)
{
	char *dst, *gateway, *genmask;
	unsigned short flags;
	int err;

	err = 0;
	flags = RTF_UP;
	memset(rtent, 0, sizeof(struct rtentry));

	switch(ent->t){
	case DEFAULT:
		dst = WCIP;
		gateway = ent->next;
		genmask = WCIP;
		flags |= RTF_GATEWAY;
		break;
	case DIRECT:
		dst = ent->dst;
		gateway = WCIP;
		genmask = WHIP;
		break;
	case INDIRECT:
		dst = ent->dst;
		gateway = ent->next;
		genmask = WHIP;
		flags |= RTF_GATEWAY;
		break;
	default: FAILASSERT("invalid ent->t", err, fail);
	}

	if(populatesaddr(dst, RTDST(rtent)) < 0 ||
	   populatesaddr(gateway, RTGATEWAY(rtent)) < 0 ||
	   populatesaddr(genmask, RTGENMASK(rtent)) < 0)
		FAILASSERT("populatesaddr", err, fail);

	rtent->rt_flags = flags;
	rtent->rt_dev = ent->dev;
	rtent->rt_metric = DEFMETRIC;

fail:
	return err;
}

int
syncrcentry(struct rcentry *ent, struct rcentry *replace)
{
	int fd, err;
	struct rtentry rtent;

	memset(&rtent, 0, sizeof(struct rtentry));
	err = 0;
	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if(fd < 0) FAILASSERT("socket", err, fail);

	if(replace){
		if(mkrtentry(replace, &rtent) < 0)
			FAILASSERT("mkrtentry replace", err, fail);
		if(ioctl(fd, SIOCDELRT, &rtent) < 0)
			FAILASSERT("ioctl replace", err, fail);
	}

	if(ent){
		if(mkrtentry(ent, &rtent) < 0)
			FAILASSERT("mkrtentry", err, recover);
		if(ioctl(fd, SIOCADDRT, &rtent) < 0)
			FAILASSERT("ioctl", err, recover);
	}

	return err;

recover:
	if(replace){
		if(mkrtentry(replace, &rtent) < 0)
			LITERALLYDIE("mkrtentry recover");
		if(ioctl(fd, SIOCADDRT, &rtent) < 0)
			LITERALLYDIE("ioctl recover");
	}

fail:
	return err;
}

struct rcentry *
rdrc(char *dst)
{
	unsigned int k;
	struct rcentry *ent;

	ent = NULL;
	if(!rc) FAILASSERTNE("rc not initialized", fail);
	if(iptou32(dst, &k) < 0) FAILASSERTNE("iptou32", fail);
	ent = getht(rc, k);
fail:
	return ent;
}

int
writerc(struct rcentry *r)
{
	int err;
	unsigned int k;
	struct rcentry *old;

	err = 0;
	if(!rc) FAILASSERT("rc not initialized", err, fail);
	if(iptou32(r->dst, &k) < 0) FAILASSERT("iptou32", err, fail);
	old = getht(rc, k);

	if(insertht(rc, k, r) < 0) FAILASSERT("insertht", err, fail);
	if(syncrcentry(r, old) < 0) FAILASSERT("syncrcentry", err, recht);

	return err;

recht:
	if(!delht(rc, k)) LITERALLYDIE("delht");
fail:
	return err;
}

struct rcentry *
delrc(char *dst)
{
	struct rcentry *ent;
	unsigned int k;

	ent = NULL;
	if(!rc) FAILASSERTNE("rc not initialized", fail);
	if(iptou32(dst, &k) < 0) FAILASSERTNE("iptou32", fail);

	ent = delht(rc, k);
	if(!ent) FAILASSERTNE("delht", fail);
	if(syncrcentry(NULL, ent) < 0) FAILASSERTNE("syncrcentry", recht);

	return ent;
recht:
	if(insertht(rc, k, ent) < 0) LITERALLYDIE("insertht");
	ent = NULL;
fail:
	return ent;
}

int
initrc(void)
{
	int err;

	err = 0;
	rc = initht(FNV1AHF);
	if(!rc) FAILASSERT("initht", err, fail);

	defroute = calloc(1, sizeof(struct rcentry));
	if(!defroute) FAILASSERT("calloc", err, htfail);

	defroute->t = DEFAULT;
	defroute->dst = WCIP;
	defroute->dev = tunname;
	defroute->next = DEFTUNADDR;

	if(writerc(defroute) < 0) FAILASSERT("writerc", err, drfail);

	return err;

drfail:
	free(defroute);
	defroute = NULL;
htfail:
	deinitht(rc);
	rc = NULL;
fail:
	return err;
}

void
deinitrc(void)
{
	int i, fc;

	fc = 0;
	for(i = 0; i < (int)rc->capacity; i++){
		if(rc->reslist[i] == RESIDENT){
			struct rcentry *old;
			old = rc->vals[i];
			syncrcentry(NULL, old);
			free(old);
			fc++;
		}
	}
	defroute = NULL;
	deinitht(rc);
	rc = NULL;
}
