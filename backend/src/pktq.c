#include "dat.h"
#include "fns.h"

#include <stdio.h>
#include <stdlib.h>

struct ht *pktq = NULL;

int
initpktq(void)
{
	int err;

	err = 0;
	pktq = initht(FNV1AHF);
	if(!pktq) FAILASSERT("initht", err, fail);

fail:
	return err;
}

void
deinitpktq(void)
{
	deinitht(pktq);
	pktq = NULL;
}

int
insertpktq(void *itemref, char *whom)
{
	int err;
	struct darray *dr;
	unsigned int k;

	err = 0;
	if(iptou32(whom, &k) < 0) FAILASSERT("iptou32", err, fail);
	dr = getht(pktq, k);
	if(!dr){
		dr = mkdarray();
		if(insertht(pktq, k, dr) < 0){
			freedarray(dr);
			FAILASSERT("insertht", err, fail);
		}
	}

	if(appenddarray(dr, itemref) < 0) FAILASSERT("appenddarray", err, fail);
fail:
	return err;
}

struct darray *
getlinepktq(char *whom)
{
	struct darray *r;
	unsigned int k;

	r = NULL;
	if(iptou32(whom, &k) < 0) FAILASSERTNE("iptou32", fail);
	r = getht(pktq, k);
fail:
	return r;
}

int
flushlinepktq(char *whom)
{
	struct darray *r;
	unsigned int k;
	int err;

	err = 0;
	if(iptou32(whom, &k) < 0) FAILASSERTNE("iptou32", fail);
	r = delht(pktq, k);
	if(!r) FAILASSERTNE("delht", fail);

	freedarray(r);
fail:
	return err;
}
