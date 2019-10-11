/* sort.c
 * sort functions for qsort used for sorting the files */

#include <ctype.h>
#include <string.h>
#include "sort.h"
#include "item.h"

int
size_cmp(const void *a, const void *b)
{
	item *item_a = (item *)a;
	item *item_b = (item *)b;

	/* dirs first */
	if (item_a->dir && !item_b->dir) {
		return -1;
	} else if (!item_a->dir && item_b->dir) {
		return 1;
	}

	return ((item *)b)->size - ((item *)a)->size;
}

int
Size_cmp(const void *a, const void *b)
{
	item *item_a = (item *)a;
	item *item_b = (item *)b;

	/* dirs first */
	if (item_a->dir && !item_b->dir) {
		return -1;
	} else if (!item_a->dir && item_b->dir) {
		return 1;
	}

	return ((item *)a)->size - ((item *)b)->size;
}

int
alpha_cmp(const void *a, const void *b)
{
	item *item_a = (item *)a;
	item *item_b = (item *)b;

	/* dirs first */
	if (item_a->dir && !item_b->dir) {
		return -1;
	} else if (!item_a->dir && item_b->dir) {
		return 1;
	}

	int cmp_len = ((strlen(item_a->name) < strlen(item_b->name))
		? strlen(item_a->name) : strlen(item_b->name))+1;
	for (int i = 0; i < cmp_len; ++i) {
		char a = tolower(item_a->name[i]);
		char b = tolower(item_b->name[i]);

		if (a < b) {
			return -1;
		} else if (a > b) {
			return 1;
		}
	}
}

int
Alpha_cmp(const void *a, const void *b)
{
	item *item_a = (item *)a;
	item *item_b = (item *)b;

	/* dirs first */
	if (item_a->dir && !item_b->dir) {
		return -1;
	} else if (!item_a->dir && item_b->dir) {
		return 1;
	}

	int cmp_len = ((strlen(item_a->name) < strlen(item_b->name))
		? strlen(item_a->name) : strlen(item_b->name))+1;
	for (int i = 0; i < cmp_len; ++i) {
		char a = tolower(item_a->name[i]);
		char b = tolower(item_b->name[i]);

		if (a < b) {
			return 1;
		} else if (a > b) {
			return -1;
		}
	}
}

int
time_cmp(const void *a, const void *b)
{
	item *item_a = (item *)a;
	item *item_b = (item *)b;

	/* dirs first */
	if (item_a->dir && !item_b->dir) {
		return -1;
	} else if (!item_a->dir && item_b->dir) {
		return 1;
	}

	return ((item *)b)->mtime - ((item *)a)->mtime;
}

int
Time_cmp(const void *a, const void *b)
{
	item *item_a = (item *)a;
	item *item_b = (item *)b;

	/* dirs first */
	if (item_a->dir && !item_b->dir) {
		return -1;
	} else if (!item_a->dir && item_b->dir) {
		return 1;
	}

	return ((item *)a)->mtime - ((item *)b)->mtime;
}
