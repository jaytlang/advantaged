#include "dat.h"
#include "fns.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define FNV1APRIME  16777619
#define FNV1AOFFSET 2166136261

#define HASH(HT, K) (HT)->hf((K)) % (HT)->capacity

unsigned int
robotmiller()
{
	return 0;
}

unsigned int
fnv1a(unsigned int k)
{
	char *b, *i;
	unsigned int hash;

	hash = FNV1AOFFSET;
	b = (char *)&k;

	for(i = b; i < b + sizeof(unsigned int); i++){
		hash *= FNV1APRIME;
		hash ^= *i;
	}

	return hash;
}

struct ht *
initht(int hf)
{
	struct ht *ht;

	ht = calloc(1, sizeof(struct ht));
	if(!ht) FAILASSERTNE("calloc", fail);

	switch(hf){
	case FNV1AHF: ht->hf = fnv1a; break;
	case ROBOTMILLERHF: ht->hf = robotmiller; break;
	default: FAILASSERTNE("invalid hash function selected", htafail);
	}

	ht->capacity = INITCAP;

	ht->nents = 0;
	ht->keys = calloc(INITCAP, sizeof(unsigned int));
	if(!ht->keys) FAILASSERTNE("calloc keys", htafail);

	ht->reslist = calloc(INITCAP, sizeof(unsigned char));
	if(!ht->reslist) FAILASSERTNE("calloc wl", htrfail);

	ht->vals = calloc(INITCAP, sizeof(void *));
	if(!ht->vals) FAILASSERTNE("calloc vals", htkfail);

	return ht;

htkfail:
	free(ht->keys);
htrfail:
	free(ht->reslist);
htafail:
	free(ht);
fail:
	return NULL;
}

void
deinitht(struct ht *ht)
{
	free(ht->keys);
	free(ht->vals);
	free(ht->reslist);
	free(ht);
}

int
doinsertion(struct ht *ht, unsigned int k, void *v, int *didexist)
{
	int err;
	unsigned int slot, ogslot;

	err = 0;
	ogslot = slot = HASH(ht, k);

	while(ht->reslist[slot] == RESIDENT && ht->keys[slot] != k){
		slot = (slot + 1) % ht->capacity;
		if(slot == ogslot) FAILASSERT("ht load == 1", err, done);
	}

	if(ht->reslist[slot] != RESIDENT){
		ht->nents++;
		ht->keys[slot] = k;
		ht->reslist[slot] = RESIDENT;
		*didexist = 0;
	}else
		*didexist = 1;

	ht->vals[slot] = v;
done:
	return err;
}

int
dodeletion(struct ht *ht, unsigned int k)
{
	int err;
	unsigned int slot, ogslot;

	err = 0;
	ogslot = slot = HASH(ht, k);
	while(!(ht->reslist[slot] == RESIDENT && ht->keys[slot] == k)){
		slot = (slot + 1) % ht->capacity;
		if(slot == ogslot) FAILASSERT("key never inserted", err, done);
	}

	ht->nents--;
	ht->reslist[slot] = EMPTY;
	ht->keys[slot] = 0;
	ht->vals[slot] = NULL;
done:
	return err;
}

int
resizerehash(struct ht *ht, int delta)
{
	int err;
	unsigned int i;
	double lf;
	unsigned char *newreslist;
	unsigned int newcap, *newkeys;
	void **newvals;

	err = 0;
	lf = ((double)ht->nents + (double)delta) / (double)ht->capacity;

	if(lf < MAXLF && lf > MINLF) goto end;
	else if(lf >= MAXLF)
		newcap = ht->capacity * 2;
	else
		newcap = ht->capacity / 2;

	if(lf <= MINLF && newcap < 5) goto end;

	newkeys = calloc(newcap, sizeof(unsigned int));
	if(!newkeys) FAILASSERT("calloc keys", err, end);
	newvals = calloc(newcap, sizeof(void *));
	if(!newvals) FAILASSERT("calloc vals", err, nkfail);
	newreslist = calloc(newcap, sizeof(unsigned char));
	if(!newreslist) FAILASSERT("calloc reslist", err, nvfail);

	for(i = 0; i < ht->capacity; i++){
		unsigned int key, nslot, ognslot;
		void *val;

		if(ht->reslist[i] == EMPTY) continue;
		key = ht->keys[i];
		val = ht->vals[i];

		ognslot = nslot = ht->hf(key) % newcap;
		while(newreslist[nslot] == RESIDENT){
			nslot = (nslot + 1) % newcap;
			if(nslot == ognslot)
				FAILASSERT("ht load == 1", err, nrfail);
		}

		newkeys[nslot] = key;
		newreslist[nslot] = RESIDENT;
		newvals[nslot] = val;
	}

	free(ht->keys);
	free(ht->reslist);
	free(ht->vals);

	ht->keys = newkeys;
	ht->reslist = newreslist;
	ht->vals = newvals;
	ht->capacity = newcap;

	return 0;

nrfail:
	free(newreslist);
nvfail:
	free(newvals);
nkfail:
	free(newkeys);
end:
	return err;
}

int
insertht(struct ht *ht, unsigned int k, void *v)
{
	int err, didexist;

	didexist = err = 0;
	if(doinsertion(ht, k, v, &didexist) < 0)
		FAILASSERT("doinsertion", err, done);
	if(didexist == 0)
		if(resizerehash(ht, 1) < 0)
			FAILASSERT("resizerehash", err, done);

done:
	return err;
}

void *
getht(struct ht *ht, unsigned int k)
{
	void *out;
	unsigned int slot, ogslot;

	out = NULL;
	ogslot = slot = HASH(ht, k);

	while(!(ht->reslist[slot] == RESIDENT && ht->keys[slot] == k)){
		slot = (slot + 1) % ht->capacity;
		if(slot == ogslot) goto done;
	}

	out = ht->vals[slot];
done:
	return out;
}

void *
delht(struct ht *ht, unsigned int k)
{
	void *out;

	out = NULL;
	if(resizerehash(ht, -1) < 0) FAILASSERTNE("resizerehash", done);
	if(!(out = getht(ht, k))) FAILASSERTNE("getht", done);
	if(dodeletion(ht, k) < 0) FAILASSERTNE("dodeletion", done);
done:
	return out;
}
