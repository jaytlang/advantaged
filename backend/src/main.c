#include "dat.h"
#include "fns.h"

#include <arpa/inet.h>
#include <linux/if.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define WIFIMTU 2304
#define TESTMSG "NEVER GONNA GIVE YOU UP"

void
pktqtest(void)
{
	int i;
	int testarray[1000];
	struct darray *ref;

	if(initpktq() < 0) FAILASSERTNE("initpktq", fail);
	for(i = 0; i < 1000; i++){ testarray[i] = i; }

	for(i = 0; i < 1000; i++){
		if(insertpktq(testarray + i, "127.0.0.1") < 0){
			if(flushlinepktq("127.0.0.1") < 0)
				FAILASSERTNE("flushlinepktq", pfail);
			FAILASSERTNE("insertpktq", pfail);
		}
	}

	ref = getlinepktq("127.0.0.1");
	for(i = 0; i < 1000; i++)
		printf("%d ", *(int *)getdarray(ref, (unsigned int)i));
	printf("\n");

	if(flushlinepktq("127.0.0.1") < 0) FAILASSERTNE("flushlinepktq", pfail);

	deinitpktq();
	printf("TEST PASSED?\n");
	return;

pfail:
	deinitpktq();
fail:
	printf("FAILED TEST\n");
}

void
darraytest(void)
{
	int i;
	unsigned int testarray[350];
	struct darray *d;

	for(i = 0; i < 350; i++) testarray[i] = i;

	d = mkdarray();
	if(!d) FAILASSERTNE("mkdarray", fail);

	printf("Initial capacity/size: %d/%d\n", d->capacity, d->size);
	for(i = 0; i < 350; i++){
		if(appenddarray(d, testarray + i) < 0)
			FAILASSERTNE("append", tfail);
	}

	printf("New capacity/size: %d/%d\n", d->capacity, d->size);
	freedarray(d);
	printf("TEST PASSED?\n");
	return;

tfail:
	freedarray(d);
fail:
	printf("FAILED TEST\n");
}

void
httest(void)
{
	unsigned int keyarray[1000];
	unsigned int valarray[1000];
	int i;
	struct ht *ht;

	for(i = 0; i < 1000; i++){
		keyarray[i] = i;
		valarray[i] = i * 2;
	}

	ht = initht(FNV1AHF);
	if(!ht) FAILASSERTNE("initht", fail);

	for(i = 0; i < 1000; i++)
		if(insertht(ht, keyarray[i], &valarray[i]) < 0)
			FAILASSERTNE("insertht", fail);

	for(i = 0; i < 1000; i++){
		unsigned int *v;
		v = getht(ht, keyarray[i]);
		if(!v) FAILASSERTNE("getht", fail);
		if(keyarray[i] * 2 != *v)
			FAILASSERTNE("incorrect result", fail);
	}

	for(i = 0; i < 1000; i++)
		if(!delht(ht, keyarray[i])) FAILASSERTNE("deleteht", fail);

	if(insertht(ht, keyarray[2], &valarray[2]) < 0)
		FAILASSERTNE("insertht", fail);
	if(getht(ht, keyarray[1])) FAILASSERTNE("getht on deleted slot", fail);
	if(*(unsigned int *)getht(ht, keyarray[2]) != valarray[2])
		FAILASSERTNE("updated getht", fail);

	printf("HT NENTS BEFORE DUAL INSERT: %u\n", ht->nents);
	if(insertht(ht, 1500, &valarray[0]) < 0) FAILASSERTNE("insertht", fail);
	if(insertht(ht, 1500, &valarray[1]) < 0) FAILASSERTNE("insertht", fail);
	if(*(unsigned int *)getht(ht, 1500) != valarray[1])
		FAILASSERTNE("incorrect ht value obtained", fail);
	printf("HT NENTS AFTER DUAL INSERT: %u\n", ht->nents);

	printf("TEST PASSED PROBABLY\n");
	deinitht(ht);
	return;
fail:
	printf("TEST FAILED\n");
}

int
main()
{
	printf("=== advd ===\n");
	return readysetdispatch();
}
