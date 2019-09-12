#ifndef SNCURSES
#define SNCURSES
#include <ncurses.h>

int sinit_pair(short pair, short f, short b);
int sendwin(void);
int srefresh(void);
WINDOW *snewwin(int nlines, int ncols, int begin_y, int begin_x);
int sdelwin(WINDOW *win);
int sbox(WINDOW *win, chtype verch, chtype horch);
int swaddch(WINDOW *win, const chtype ch);
int smvwaddch(WINDOW *win, int y, int x, const chtype ch);
int smvwdelch(WINDOW *win, int y, int x);
int smvwaddstr(WINDOW *win, int y, int x, const char *str);
int smvwprintw(WINDOW *win, int y, int x, const char *fmt, ...);
int swattron(WINDOW *win, int attrs);
int swattroff(WINDOW *win, int attrs);
int swrefresh(WINDOW *win);

#endif
