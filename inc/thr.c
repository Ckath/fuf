/* thr.c
 * functions for starting/stopping the loading threads */

#include <pthread.h>
#include <signal.h>
#include <dirent.h> /* this is where PATH_MAX comes from, dont think about it */
#include <stdbool.h>
#include "thr.h"
#include "sysext.h"

pthread_t load_thr;
pthread_t display_thr;
pthread_t preview_main_thr;
pthread_t preview_backup_thr;
#ifdef X_HACKS
pthread_t x_thr;
#endif
pthread_cond_t run_preview = PTHREAD_COND_INITIALIZER;
pthread_mutex_t preview_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t preview_pid_lock = PTHREAD_MUTEX_INITIALIZER;
pid_t preview_pid[2];
char preview_job[2][PATH_MAX];
_Atomic bool items_loading = false;
_Atomic bool pn = false;

void
start_load(void *load_items, void *display_load)
{
	if (items_loading) {
		return;
	}

	items_loading = true;
	pthread_create(&load_thr, NULL, load_items, NULL);
	pthread_create(&display_thr, NULL, display_load, NULL);
}

void
stop_load()
{
	if (items_loading) {
		items_loading = false;
		pthread_join(load_thr, NULL);
		pthread_join(display_thr, NULL);
	}
}

void
init_preview(void *load_preview)
{
	pthread_create(&preview_main_thr, NULL, load_preview, NULL);
	pthread_create(&preview_backup_thr, NULL, load_preview, NULL);
}

void
queue_preview()
{
	pthread_mutex_lock(&preview_lock);
	pthread_cond_signal(&run_preview);
	pthread_mutex_unlock(&preview_lock);
}

void
cancel_preview()
{
	pthread_mutex_lock(&preview_pid_lock);
	if (preview_pid[0]) {
		ext_kill(preview_pid[0], SIGKILL);
	} if (preview_pid[1]) {
		ext_kill(preview_pid[1], SIGKILL);
	}
	pthread_mutex_unlock(&preview_pid_lock);
}

#ifdef X_HACKS
void
init_xwinwatch(void *xwinwatch)
{
	pthread_create(&x_thr, NULL, xwinwatch, NULL);
}
#endif
