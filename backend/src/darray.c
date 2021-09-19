#include "dat.h"
#include "fns.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct darray *
mkdarray(void)
{
	struct darray *new;

	new = calloc(1, sizeof(struct darray));
	if(!new) FAILASSERTNE("calloc darray", fail);

	new->capacity = DARRAYINITSZ;
	new->content = calloc(DARRAYINITSZ, sizeof(void *));
	if(!new->content) FAILASSERTNE("calloc objs", sfail);
	return new;

sfail:
	free(new);
fail:
	return NULL;
}

int
appenddarray(struct darray *d, void *newobj)
{
	int err;

	err = 0;
	if(d->capacity == d->size){
		void **nc;

		nc = calloc(d->capacity * 2, sizeof(void *));
		if(!nc) FAILASSERT("resize", err, fail);
		memcpy(nc, d->content, d->capacity * sizeof(void *));

		free(d->content);
		d->content = nc;
		d->capacity *= 2;
	}

	d->content[d->size++] = newobj;

fail:
	return err;
}

void *
getdarray(struct darray *d, unsigned int idx)
{
	if(idx >= d->size) return NULL;
	return d->content[idx];
}

void
freedarray(struct darray *d)
{
	free(d->content);
	free(d);
}
