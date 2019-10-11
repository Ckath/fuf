#ifndef COLORS_H
#define COLORS_H

#define COLOR_DEFAULT -1
#define COL(fg, bg) (((fg&15) << 4) | (bg&15))
#define IS_ATTR(val) (val < 9)
#define IS_FG(val) ((val > 29 && val < 38) \
                   || (val > 89 && val < 98))
#define IS_BG(val) ((val > 39 && val < 48) \
                   || (val > 99 && val < 108))

void init_colors();
void mvwaddcolitem(WINDOW *win, int y, int x, const char *name, mode_t mode);

#endif
