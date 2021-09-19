#ifndef INC_FNS_H
#define INC_FNS_H

struct iphdr;
struct sockaddr_in;
struct dispatchcmdplus;
struct darray;
struct ht;

/* macros */
#define FAILASSERT(ERROR, ERRVAR, LABEL) \
	do{                             \
		perror(ERROR);           \
		ERRVAR = -1;             \
		goto LABEL;              \
	}while(0)

#define FAILASSERTNE(ERROR, LABEL) \
	do{                       \
		perror(ERROR);     \
		goto LABEL;        \
	}while(0)

#define PANIC(ERROR)           \
	do{                   \
		perror(ERROR); \
		return -1;     \
	}while(0)

#define LITERALLYDIE(ERROR)    \
	do{                   \
		perror(ERROR); \
		exit(-1);      \
	}while(0)

/* darray.c */
struct darray *mkdarray(void);
int appenddarray(struct darray *d, void *newobj);
void *getdarray(struct darray *d, unsigned int idx);
void freedarray(struct darray *d);

/* util.c */
int populatesaddr(char *addr, struct sockaddr_in *saddr);
int iptou32(char *ip, unsigned int *u32);
int u32toip(unsigned int u32, char *ip);
int attemptinternetcon(void);

/* tun.c */
int inittun(char *addr);
int readtun(char *pkt, unsigned int pktsz);
void deinittun(void);

/* rc.c */
int initrc(void);
void deinitrc(void);
struct rcentry *rdrc(char *dst);
int writerc(struct rcentry *r);
struct rcentry *delrc(char *dst);

/* netifc.c */
int initnetifc(void);
void deinitnetifc(void);
int bcastnetifc(char *msg, unsigned int msgsz);
int fwdpktnetifc(char *pkt, unsigned int pktsz);
int fwdudpnetifc(char *pkt, unsigned int pktsz, char *from, char *to);

/* ht.c */
struct ht *initht(int hf);
void deinitht(struct ht *ht);
int insertht(struct ht *ht, unsigned int k, void *v);
void *getht(struct ht *ht, unsigned int k);
void *delht(struct ht *ht, unsigned int k);

/* listen.c */
int startlisten(void);
int rdlisten(char *pktbuf, unsigned int pktsz, char **dev, unsigned int *devip,
	     unsigned int *from);
void stoplisten(void);

/* pktq.c */
int initpktq(void);
void deinitpktq(void);
int insertpktq(void *itemref, char *whom);
struct darray *getlinepktq(char *whom);
int flushlinepktq(char *whom);

/* route.c */
int doroute(void);

/* dispatch.c */
int wrcmd(struct dispatchcmdplus *c);
int rdcmd(struct dispatchcmdplus *c);
int readysetdispatch(void);

#endif /* INC_FNS_H */
