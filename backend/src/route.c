#include "dat.h"
#include "fns.h"

#include <arpa/inet.h>
#include <linux/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

unsigned long myseq = 0;

int
procrreqonline(struct dispatchcmdplus *c)
{
	char *dstip, *fromip, *devip;
	struct rreqdc *r;

	r = &c->rreq;
	printf("Processing route request via online mode\n");

	dstip = calloc(INET_ADDRSTRLEN, sizeof(char));
	if(!dstip) PANIC("calloc dstip");

	fromip = calloc(INET_ADDRSTRLEN, sizeof(char));
	if(!fromip) PANIC("calloc fromip");

	devip = calloc(INET_ADDRSTRLEN, sizeof(char));
	if(!devip) PANIC("calloc devip");

	if(u32toip(c->from, fromip) < 0) PANIC("u32toip from");
	if(r->srcip == 0){
		r->srcip = c->from;
		printf("Note: this packet is fresh off its source. Filling in "
		       "srcip\n");
	}
	if(u32toip(r->dstip, dstip) < 0) PANIC("u32toip r->dstip");
	if(u32toip(c->devip, devip) < 0) PANIC("u32toip devip");

	printf("Allocated buffers. Analyzing target IP address %s\n", dstip);
	if(memcmp(dstip, "10.", 3) == 0)
		printf("Dropping locally bound packet with 10.x.x.x prefix\n");
	else{
		struct dispatchcmd d;

		memset(&d, 0, sizeof(struct dispatchcmd));
		printf("Got internet-bound packet. I can get you there, "
		       "updating seqno\n");
		if(r->dstseq > myseq) myseq = r->dstseq;

		printf("Populating and sending rreq\n");
		d.d = RREP;
		d.rrep.srcip = r->dstip;
		d.rrep.srcseq = myseq;
		d.rrep.dstip = r->srcip;
		if(fwdudpnetifc((char *)&d, sizeof(struct dispatchcmd), devip,
				fromip) < 0)
			PANIC("fwdudpnetifc");
		printf("Pushed out RREP successfully\n");
	}

	free(dstip);
	free(fromip);
	free(devip);
	return 0;
}

int
procrreq(struct dispatchcmdplus *c)
{
	int usedfrom, usedsrc;
	char *srcip, *dstip, *fromip;
	struct rcentry *cr, *crf;
	struct dispatchcmd d;
	struct rreqdc *r;

	printf("Processing route request\n");

	r = &c->rreq;
	usedfrom = usedsrc = 0;

	dstip = calloc(INET_ADDRSTRLEN, sizeof(char));
	if(!dstip) PANIC("calloc dstip");

	srcip = calloc(INET_ADDRSTRLEN, sizeof(char));
	if(!srcip) PANIC("calloc srcip");

	fromip = calloc(INET_ADDRSTRLEN, sizeof(char));
	if(!fromip) PANIC("calloc fromip");

	if(u32toip(c->from, fromip) < 0) PANIC("u32toip from");
	if(r->srcip == 0){
		r->srcip = c->from;
		printf("Note: this packet is fresh off its source. Filling in "
		       "srcip\n");
	}
	if(u32toip(r->srcip, srcip) < 0) PANIC("u32toip r->srcip");
	if(u32toip(r->dstip, dstip) < 0) PANIC("u32toip r->dstip");

	printf("Allocated buffers. Checking the last hop, %s\n", fromip);

	/* Jam the last IP into the route cache
	 * Pay no mind to the sequence number
	 */
	crf = rdrc(fromip);
	if(!crf){
		printf("We haven't seen this node before\n");
		crf = calloc(1, sizeof(struct rcentry));
		if(!crf) PANIC("crf calloc");

		crf->t = DIRECT;
		crf->next = crf->dst = fromip;
		usedfrom = 1;
		crf->dev = c->dev;
		crf->seq = 0;
		if(writerc(crf) < 0) PANIC("writerc crf");
		printf("Wrote adjacent previous node with seq 0 to rc\n");
	}else
		printf("Used cached last node with seq %lu\n", crf->seq);

	printf("Referenced last node along route successfully\n");

	/* Do we know the source IP?
	 * If so, have we seen this sequence
	 * number off of them before?
	 */
	printf("Checking source IP, %s\n", srcip);
	cr = rdrc(srcip);
	if(cr){
		/* Yep, do updates if needed */
		printf("Cached source: sequence number we have is %lu, we were "
		       "given %lu\n",
		       cr->seq, r->srcseq);
		if(r->srcseq <= cr->seq){
			printf("Dropping packet due to loop detection\n");
			goto frees;
		}
		cr->seq = r->srcseq;
		printf("Set our cached sequence number for source to update\n");
	}else{
		/* Nope, add the reverse route
		 * Could worry about freeing, but for now
		 * this shouldn't leak memory since we aren't
		 * doing deletions for HackMIT
		 */
		printf("This is a new source, writing a new entry for it\n");
		cr = calloc(1, sizeof(struct rcentry));
		if(!cr) PANIC("calloc cr");

		cr->t = INDIRECT;
		cr->next = fromip;
		cr->dst = srcip;
		usedfrom = usedsrc = 1;
		cr->dev = c->dev;
		cr->seq = r->srcseq;
		printf("New source entry prepared\n");
	}

	if(writerc(cr) < 0) PANIC("writerc");
	printf("New source entry flushed to kernel\n");

	/* Okay, are we the dstip? */
	memset(&d, 0, sizeof(struct dispatchcmd));

	printf("This request wants destination IP %s\n", dstip);
	if(c->devip == r->dstip){
		/* We are...update our sequence number and rrep */
		printf("That's us! We have sequence number %lu, the request "
		       "carried %lu\n",
		       myseq, r->dstseq);
		if(r->dstseq > myseq) myseq = r->dstseq;

		d.d = RREP;
		d.rrep.srcip = r->dstip;
		d.rrep.srcseq = myseq;
		d.rrep.dstip = r->srcip;
		printf("Prepared RREP to %s\n", srcip);
		if(fwdudpnetifc((char *)&d, sizeof(struct dispatchcmd), dstip,
				fromip) < 0)
			PANIC("fwdudpnetifc");
		printf("Pushed out RREP successfully\n");
	}else{
		/* We are not. Bcast as is. */
		printf("That's not us. Forwarding along\n");
		d.d = RREQ;
		d.rreq.srcip = r->srcip;
		d.rreq.srcseq = r->srcseq;
		d.rreq.dstip = r->dstip;
		d.rreq.dstseq = r->dstseq;
		if(bcastnetifc((char *)&d, sizeof(struct dispatchcmd)) < 0)
			PANIC("bcastnetifc");
		printf("Forwarded RREQ successfully\n");
	}

frees:
	free(dstip);
	if(usedsrc == 0) free(srcip);
	else if(appenddarray(dystrs, srcip) < 0)
		PANIC("appenddarray");

	if(usedfrom == 0) free(fromip);
	else if(appenddarray(dystrs, fromip) < 0)
		PANIC("appenddarray");
	return 0;
}

int
procrrep(struct dispatchcmdplus *c)
{
	int usedfrom, usedsrc;
	char *srcip, *dstip, *fromip, *devip;
	struct rcentry *cr, *crf, *crn;
	struct rrepdc *r;

	printf("Processing route reply\n");

	r = &c->rrep;
	usedfrom = usedsrc = 0;

	dstip = calloc(INET_ADDRSTRLEN, sizeof(char));
	if(!dstip) PANIC("calloc dstip");

	srcip = calloc(INET_ADDRSTRLEN, sizeof(char));
	if(!srcip) PANIC("calloc srcip");

	fromip = calloc(INET_ADDRSTRLEN, sizeof(char));
	if(!fromip) PANIC("calloc fromip");

	devip = calloc(INET_ADDRSTRLEN, sizeof(char));
	if(!devip) PANIC("calloc devip");

	if(u32toip(r->srcip, srcip) < 0) PANIC("u32toip r->srcip");
	else if(u32toip(r->dstip, dstip) < 0)
		PANIC("u32toip r->dstip");
	else if(u32toip(c->from, fromip) < 0)
		PANIC("u32toip from");
	else if(u32toip(c->devip, devip) < 0)
		PANIC("u32toip devip");

	/* Jam the last IP into the route cache
	 * Pay no mind to the sequence number
	 */

	printf("Allocated buffers. Checking the last hop, %s\n", fromip);
	crf = rdrc(fromip);
	if(!crf){
		printf("We haven't seen this node before\n");
		crf = calloc(1, sizeof(struct rcentry));
		if(!crf) PANIC("crf calloc");

		crf->t = DIRECT;
		crf->next = crf->dst = fromip;
		usedfrom = 1;
		crf->dev = c->dev;
		crf->seq = 0;
		if(writerc(crf) < 0) PANIC("writerc crf");
		printf("Wrote adjacent previous node with seq 0 to rc\n");
	}else
		printf("Used cached last node with seq %lu\n", crf->seq);

	printf("Referenced last node along route successfully\n");

	/* Do we know the source IP? If
	 * so, update the sequence number.
	 * We are already loop free, but we
	 * should ensure the srcseq is accurate
	 */
	printf("Checking source IP, %s\n", srcip);
	cr = rdrc(srcip);
	if(cr){
		printf("Cached source: sequence number we have is %lu, we are "
		       "given %lu\n",
		       cr->seq, r->srcseq);
		cr->seq = r->srcseq;
	}else{
		/* Nope, add the reverse route
		 * Could worry about freeing, but for now
		 * this shouldn't leak memory since we aren't
		 * doing deletions for HackMIT
		 */
		printf("This is a new source, writing a new entry for it\n");
		cr = calloc(1, sizeof(struct rcentry));
		if(!cr) PANIC("calloc cr");

		cr->t = INDIRECT;
		cr->next = fromip;
		cr->dst = srcip;
		usedfrom = usedsrc = 1;
		cr->dev = c->dev;
		cr->seq = r->srcseq;
		printf("New source entry prepared\n");
	}

	if(writerc(cr) < 0) PANIC("writerc");
	printf("New source entry flushed to kernel\n");

	/* Okay, are we the dstip?
	 * If so, we need to flush our pktq
	 */
	printf("This request wants destination IP %s\n", dstip);
	if(c->devip == r->dstip){
		int i;
		struct darray *d;

		d = getlinepktq(srcip);
		if(!d){
			printf("We're already set on this route! Skipping "
			       "gratitious rrep\n");
			goto done;
		}
		printf("That's us! Flushing packet queue with %u els down new "
		       "route\n",
		       d->size);

		for(i = 0; i < (int)d->size; i++){
			struct rawpacket *pkt;

			pkt = getdarray(d, i);
			if(!pkt) PANIC("getdarray at known index failed");
			printf("About to send through packet with size %d\n",
			       pkt->pktsz);
			if(fwdpktnetifc(pkt->pkt, pkt->pktsz) < 0)
				PANIC("fwdpktnetifc during flush");
			free(pkt);
		}

		printf("All elements flushed, freeing packet queue line\n");
		if(flushlinepktq(srcip) < 0) PANIC("flushlinepktq");
	}else{
		struct dispatchcmd d;
		memset(&d, 0, sizeof(struct dispatchcmd));

		crn = rdrc(dstip);
		if(!crn) PANIC("rdrc for resp destip");

		printf("That's not us. Forwarding rrep...\n");
		/* Else, forward on */
		d.d = RREP;
		d.rrep.srcip = r->srcip;
		d.rrep.srcseq = r->srcseq;
		d.rrep.dstip = r->dstip;
		if(fwdudpnetifc((char *)&d, sizeof(struct dispatchcmd), devip,
				crn->next) < 0)
			PANIC("fwdudpnetifc");
	}

done:
	printf("Done!\n");
	free(devip);
	free(dstip);
	if(usedsrc == 0) free(srcip);
	else if(appenddarray(dystrs, srcip) < 0)
		PANIC("appenddarray");

	if(usedfrom == 0) free(fromip);
	else if(appenddarray(dystrs, fromip) < 0)
		PANIC("appenddarray");

	return 0;
}

int
procnewrt(struct dispatchcmdplus *c)
{
	struct rawpacket *np;
	struct rcentry *cr;
	struct iphdr *iph;
	struct dispatchcmd d;
	unsigned int du32;
	char destip[INET_ADDRSTRLEN];

	printf(
	    "Intercepted packet off tun. Needs a route. Details forthcoming\n");
	iph = (struct iphdr *)c->rp.pkt;
	du32 = iph->daddr;
	if(u32toip(du32, destip) < 0) PANIC("u32toip");

	printf("This packet wants IP %s\n", destip);

	/* If we don't have a pktq yet, this means
	 * our view of the topology has changed. Add one
	 * to our sequence number if so
	 */
	if(!getlinepktq(destip)){
		printf("We don't have a packet queue for this guy. Will need "
		       "to route\n");
		myseq++;
	}else
		printf("We do have a packet queue. Will add to packet queue "
		       "and wait for last route\n");

	/* Append to the pktq, start routing */

	np = calloc(1, sizeof(struct rawpacket));
	if(!np) PANIC("calloc np");
	memcpy(np, &c->rp, sizeof(struct rawpacket));

	if(insertpktq(np, destip) < 0) PANIC("insertpktq");
	printf("Injected packet of size %u into appropriate packet queue\n",
	       np->pktsz);

	printf("Routing packet with initial rreq\n");
	memset(&d, 0, sizeof(struct dispatchcmd));
	d.d = RREQ;
	d.rreq.srcip = 0;
	d.rreq.srcseq = myseq;
	d.rreq.dstip = du32;
	d.rreq.dstseq = ((cr = rdrc(destip))) ? cr->seq : 0;
	if(bcastnetifc((char *)&d, sizeof(struct dispatchcmd)) < 0)
		PANIC("bcastnetifc");

	printf("Done!\n");
	return 0;
}

int
doroute(void)
{
	struct dispatchcmdplus c;
	int err;

	err = 0;
	printf("Running advd routing protocol\n");

	for(;;){
		if(rdcmd(&c) < 0) FAILASSERT("rdcmd", err, fail);
		switch(c.d){
		case RREQ:
			if(mode == INTERNETMODE){
				if(procrreqonline(&c) < 0)
					FAILASSERT("procrreqonline", err, fail);
			}else if(procrreq(&c) < 0)
				FAILASSERT("procrreq", err, fail);
			break;
		case RREP:
			if(mode == OFFLINEMODE && procrrep(&c) < 0)
				FAILASSERT("procrrep", err, fail);
			break;
		case NEWRT:
			if(mode == OFFLINEMODE && procnewrt(&c) < 0)
				FAILASSERT("procnewrt", err, fail);
			break;
		default: return 0;
		}
	}

fail:
	return err;
}
