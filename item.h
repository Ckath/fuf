#ifndef ITEM_TYPE
#define ITEM_TYPE
#include <stdbool.h>
#include <sys/stat.h>

typedef struct {
	off_t size;
	time_t mtime;
	bool dir;
	mode_t mode;
	char name[256];
} item;
#endif
