/* sncurses.c
 * safe(r) ncurses wrappers that should be fine across threads */

#include <ncurses.h>
#include <pthread.h>
#include "sncurses.h"

pthread_mutex_t ncurses_lock = PTHREAD_MUTEX_INITIALIZER;

int
sinit_pair(short pair, short f, short b)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = init_pair(pair, f, b);
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

int
sendwin(void)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = endwin();
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

int
srefresh(void)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = refresh();
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

WINDOW *
snewwin(int nlines, int ncols, int begin_y, int begin_x)
{
	pthread_mutex_lock(&ncurses_lock);
	WINDOW *r = newwin(nlines, ncols, begin_y, begin_x);
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

int
sdelwin(WINDOW *win)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = delwin(win);
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

int
sbox(WINDOW *win, chtype verch, chtype horch)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = box(win, verch, horch);
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

int
swaddch(WINDOW *win, const chtype ch)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = waddch(win, ch);
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

int
smvwaddch(WINDOW *win, int y, int x, const chtype ch)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = mvwaddch(win, y, x, ch);
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

int
smvwdelch(WINDOW *win, int y, int x)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = mvwdelch(win, y, x);
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

int
smvwaddstr(WINDOW *win, int y, int x, const char *str)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = mvwaddstr(win, y, x, str);
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

int
smvwprintw(WINDOW *win, int y, int x, const char *fmt, ...)
{
	char buf[999];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, 999, fmt, args);
	va_end(args);

	return smvwaddstr(win, y, x, buf);
}

int
swattron(WINDOW *win, int attrs)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = wattron(win, attrs);
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

int
swattroff(WINDOW *win, int attrs)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = wattroff(win, attrs);
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}

int
swrefresh(WINDOW *win)
{
	pthread_mutex_lock(&ncurses_lock);
	int r = wrefresh(win);
	pthread_mutex_unlock(&ncurses_lock);
	return r;
}
