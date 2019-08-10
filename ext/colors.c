/* colors.c
 * helper functions for coloring based on LS_COLORS */

#include <string.h>
#include <ncurses.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <wchar.h>
#include <unistd.h>
#include "colors.h"

void
init_colors()
{
	start_color();
	use_default_colors();

	/* create pairs for every possible color
	 * pairs identified by 8 bit value:
	 * fg: first 4 bits, bg: last 4 bits */
	for (int fg = COLOR_DEFAULT; fg <= COLOR_WHITE; ++fg) {
		for (int bg = COLOR_DEFAULT; bg <= COLOR_WHITE; ++bg) {
			init_pair(COL(fg, bg), fg, bg);
		}
	}
}

static int
ls_attr(int val)
{
	/* map value found in LS_COLORS to closest ncurses equivalant */
	switch (val) {
		case 0:
			return A_NORMAL;
		case 1:
			return A_BOLD;
		case 4:
			return A_UNDERLINE;
		case 5:
			return A_BLINK;
		case 7:
			return A_REVERSE;
		case 8:
			return A_INVIS;
		case 30:
		case 40:
		case 90:
		case 100:
			return COLOR_BLACK;
		case 31:
		case 41:
		case 91:
		case 101:
			return COLOR_RED;
		case 32:
		case 42:
		case 92:
		case 102:
			return COLOR_GREEN;
		case 33:
		case 43:
		case 93:
		case 103:
			return COLOR_YELLOW;
		case 34:
		case 44:
		case 94:
		case 104:
			return COLOR_BLUE;
		case 35:
		case 45:
		case 95:
		case 105:
			return COLOR_MAGENTA;
		case 36:
		case 46:
		case 96:
		case 106:
			return COLOR_CYAN;
		case 37:
		case 47:
		case 97:
		case 107:
			return COLOR_WHITE;
	}
}

static int
find_lsattrs(const char *key)
{
	char *k = malloc(strlen(getenv("LS_COLORS")) * sizeof(char));
	char *k_addr = k;
	strcpy(k, getenv("LS_COLORS"));

	/* find key in LS_COLORS store */
	char key_match[256];
	sprintf(key_match, key[0] == '.' ?  "*%s=":"%s=", key); 
	while (strncmp(k, key_match, strlen(key_match))) {
		k = strchr(k, ':')+1;
		if (!strlen(k)) {
			free(k_addr);
			return 0;
		}
	}
	strchr(k, ':')[0] = '\0';
	k += strlen(key_match);

	/* parse values found in LS_COLORS */
	int fg = 0, bg = 0, attr = 0;
	while(k-1) {
		int val = 0;
		sscanf(k, "%d", &val);
		if (IS_FG(val)) {
			fg = val;
		} else if (IS_BG(val)) {
			bg = val;
		} else {
			attr |= ls_attr(val);
		}
		k = strchr(k, ';')+1;
	}
	free(k_addr);

	/* create ncurses attribute from extracted values */
	return fg && bg ? COLOR_PAIR(COL(ls_attr(fg), ls_attr(bg))) | attr :
		fg ? COLOR_PAIR(COL(ls_attr(fg), COLOR_DEFAULT)) | attr :
		bg ? COLOR_PAIR(COL(COLOR_DEFAULT, ls_attr(bg))) | attr : attr;
}

void
mvwaddcolitem(WINDOW *win, int y, int x, const char *name, mode_t mode)
{
	/* terrible workaround to format names with wide characters */
	wchar_t wname[256];
	char fname[256];
	char last_fname[256];
	swprintf(wname, 256, L"%hs", name);
	if (strlen(name) > wcslen(wname)) { /* special wide formatting */
		sprintf(fname, "%-*.*ls", /* format, cut to size, pad right */
				COLS/2-2+(strlen(name)-wcslen(wname))/2,
				COLS/2-2+(strlen(name)-wcslen(wname))/2, wname);
		do { /* keep cutting it until it no longer gets cut */
			strcpy(last_fname, fname);
			swprintf(wname, 256, L"%hs", fname);
			sprintf(fname, "%.*ls",
					COLS/2-2+(strlen(fname)-wcslen(wname))/2, wname);
		} while (strcmp(fname, last_fname));
	} else {                            /* normal formatting */
		sprintf(fname, "%-*.*s", COLS/2-2, COLS/2-2, name);
	}

	if (!getenv("LS_COLORS")) {
		mvwaddstr(win, y, x-1, fname);
		return;
	}

	/* (nearly) full LS_COLORS type identification */
	int attrs = 0;
	switch (mode & S_IFMT) {
		case S_IFBLK:
			attrs = find_lsattrs("bd");
			break;
		case S_IFCHR:
			attrs = find_lsattrs("cd");
			break;
		case S_IFDIR:
			if (mode & S_ISVTX & S_IWOTH) {
				attrs = find_lsattrs("tw");
			} else if (mode & S_ISVTX) {
				attrs = find_lsattrs("st");
			} else if (mode & S_IWOTH) {
				attrs = find_lsattrs("ow");
			} if (!attrs) {
				attrs = find_lsattrs("di");
			}
			break;
		case S_IFIFO:
			attrs = find_lsattrs("pi");
			break;
		case S_IFLNK:
			if (access(name, F_OK)) {
				attrs = find_lsattrs("or");
			} else {
				attrs = find_lsattrs("ln");
			}
			break;
		case S_IFSOCK:
			attrs = find_lsattrs("so");
			break;
		case S_IFREG:
			if (mode & S_ISUID) {
				attrs = find_lsattrs("su");
			} else if (mode & S_ISGID) {
				attrs = find_lsattrs("sg");
			} else if (mode & S_IXUSR) {
				attrs = find_lsattrs("ex");
			}

			if (!attrs) {
				char *ext = strrchr(name, '.');
				if (ext == name || !ext) {
					attrs = find_lsattrs("fi");
				} else {
					attrs = find_lsattrs(ext);
				}
			}
			break;
	}
	if (!attrs) {
		attrs = find_lsattrs("no");
	}

	wattron(win, attrs);
	mvwaddstr(win, y, x-1, fname);
	wattroff(win, attrs);
}
