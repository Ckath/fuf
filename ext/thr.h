#ifndef THR_H
#define THR_H

void start_load(void *load_items, void *display_load);
void stop_load();
void init_preview(void *load_preview);
void queue_preview();
void cancel_preview();

#endif
