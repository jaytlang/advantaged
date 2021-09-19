#ifndef INC_DAT_H
#define INC_DAT_H

/* tun.c */
#define DEFTUNADDR "127.0.0.2"
#define MTU	   1500

extern char *tunname;

/* darray.c */
#define DARRAYINITSZ 20

struct darray{
	unsigned int capacity;
	unsigned int size;
	void **content;
};

/* rc.c */
enum rtype{ DEFAULT, DIRECT, INDIRECT };
struct rcentry{
	enum rtype t;
	char *dst;
	char *dev;
	char *next;
	unsigned long seq;
};

/* netifc.c */

#define RECVPORT   42069
#define MAXNETIFCS 10

/* ht.c */
struct ht{
	unsigned int nents;
	unsigned int *keys;

	unsigned char *reslist;
	void **vals;

	unsigned int capacity;
	unsigned int (*hf)(unsigned int k);
};

#define FNV1AHF	      1
#define ROBOTMILLERHF 2
#define MAXLF	      0.75
#define MINLF	      0.1
#define INITCAP	      5

#define RESIDENT 1
#define EMPTY	 0

/* pktq.c */
struct rawpacket{
	unsigned int pktsz;
	char pkt[MTU];
};

/* dispatch.c */

#define INTERNETMODE 1
#define OFFLINEMODE  0

#define FRONTENDPIPE "/tmp/pipe_a"

enum dctype{ RREQ, RREP, NEWRT, TOGGLE };

extern int mode;
extern int rpipe;
extern int wpipe;
extern struct darray *dystrs;

struct rreqdc{
	unsigned int srcip;
	unsigned long srcseq;
	unsigned int dstip;
	unsigned long dstseq;
};

struct rrepdc{
	unsigned int srcip;
	unsigned long srcseq;
	unsigned int dstip;
};

struct dispatchcmd{
	enum dctype d;
	char *dev;	    /* zero me before sending onto the wire */
	unsigned int devip; /* me too */
	unsigned int from;  /* me three */
	union{
		struct rreqdc rreq;
		struct rrepdc rrep;
	};
};

struct dispatchcmdplus{
	enum dctype d;
	char *dev;	    /* zero me before sending onto the wire */
	unsigned int devip; /* me too */
	unsigned int from;  /* me three */
	union{
		struct rreqdc rreq;
		struct rrepdc rrep;
		struct rawpacket rp;
	};
};

/* util.c */
#define TESTADDR "8.8.8.8"
#define TESTPORT 53

#endif /* INC_DAT_H */
