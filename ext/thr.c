#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include "thr.h"
#include "sysext.h"

pthread_t load_thr;
pthread_t display_thr;
pthread_t preview_thr;
pid_t preview_pid = 0;
bool items_loading = false;

void
start_load(void *load_items, void *display_load)
{
	items_loading = true;
	pthread_create(&load_thr, NULL, load_items, NULL);
	pthread_create(&display_thr, NULL, display_load, NULL);
}

void
stop_load()
{
	items_loading = false;
	pthread_join(load_thr, NULL);
	pthread_join(display_thr, NULL);
}

void
start_preview(void *load_preview)
{
	pthread_create(&preview_thr, NULL, load_preview, NULL);
}

void
stop_preview()
{
	if (preview_pid) {
		ext_kill(preview_pid, SIGTERM);
	} if (preview_thr) {
		pthread_join(preview_thr, NULL);
	}
}
