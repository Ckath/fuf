#include <ctype.h>
#include <dirent.h>
#include <grp.h>
#include <libgen.h>
#include <locale.h>
#include <ncurses.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "ext/colors.h"
#include "ext/sort.h"
#include "ext/sysext.h"
#include "ext/thr.h"
#include "item.h"

#define CTRL(c) (c&0x1f)
#define CLI_PROGRAMS "vi nvim nano dhex"

static void handle_redraw();  /* utility */
static char ch_prompt(char *prefix);
static void str_prompt(char *prefix, char *result);
static void search_next(bool reverse);
static void open_file();      /* async */
static void display_load();
static void load_items();
static void load_preview();
static void refresh_layout(); /* main refresh */

/* all needed info about current state */
item *items;
unsigned items_len = 0;
unsigned scroll_pos = 0;
unsigned sel_item = 0;
void *sort = alpha_cmp;
char search[256];
char goto_item[256];

static void
handle_redraw()
{
	endwin();
	refresh();

	/* keep the file list within resized window */
	scroll_pos = items_len-1 > scroll_pos-LINES+2 
		? items_len-1-LINES+2 : items_len-scroll_pos < LINES+2 
		? 0 : scroll_pos;

	refresh_layout();
}

static char
ch_prompt(char *prefix)
{
	WINDOW *prompt = newwin(1, COLS, LINES-1, 1);

	bool bold = false;
	char *str = strdup(prefix);
	char *tok = strtok(str, "[]");
	while (tok) {
		if (bold) {
			wattron(prompt, A_REVERSE | A_BOLD);
			wprintw(prompt, " %s ", tok);
			wattroff(prompt, A_REVERSE | A_BOLD);
		} else {
			waddstr(prompt, tok);
		}

		bold = !bold;
		tok = strtok(NULL, "[]");
	}
	free(tok);
	free(str);
	wrefresh(prompt);

	char c = getch();
	delwin(prompt);
	return c;
}

static void
str_prompt(char *prefix, char *result)
{
	WINDOW *prompt = newwin(1, COLS, LINES-1, 1);
	mvwaddstr(prompt, 0, 0, prefix);
	keypad(prompt, true);
	wrefresh(prompt);
	curs_set(1);

	int c;
	unsigned i = 0;
	while ((c = wgetch(prompt))) {
		switch(c) {
			case KEY_LEFT: /* not handling these keys */
			case KEY_RIGHT:
			case KEY_UP:
			case KEY_DOWN:
			case 9: /* tab */
				continue;
			case 127:
			case KEY_BACKSPACE:
				mvwdelch(prompt, 0, (i > 0 ? --i : i)+strlen(prefix));
				continue;
			case 10:
			case KEY_ENTER:
				break;
			case '': /* ESC */
				strcpy(result, "");
				break;
			default:
				c = tolower(c);
				waddch(prompt, c);
				result[i++] = c;
				continue;
		}
		result[i] = '\0';
		break;
	}

	curs_set(0);
	delwin(prompt);
}

static void
search_next(bool reverse)
{
	if (strlen(search) < 1) {
		return;
	}

	unsigned search_pos = sel_item;
	char name[256];
	int i;
	while ((reverse ? --search_pos : ++search_pos)%items_len != sel_item) {
		for (i = 0; items[search_pos%items_len].name[i]; ++i) {
			name[i] = tolower(items[search_pos%items_len].name[i]);
		}
		name[i] = '\0';
		if (strstr(name, search)) {
			break;
		}
	}

	sel_item = search_pos%items_len;
}


static void
open_file()
{
	/* clear sigchld handler, might want to wait */
	signal(SIGCHLD, SIG_DFL);

	pid_t pid = fork();
	if (!pid) { /* child: open file in program though open handler */
		char file[256];
		strcpy(file, items[sel_item].name);
		free(items);
		const char *home;
		char open_path[PATH_MAX];
		if (!(home = getenv("HOME"))) {
			home = getpwuid(getuid())->pw_dir;
		}
		sprintf(open_path, "%s/.config/fuf/open", home);

		execlp(open_path, "open", file, NULL);
		_exit(1);
	} else { /* parent: check launched process */
		char proc_path[256];
		char proc_name[256];
		int proc_pid = 0;
		FILE *f;
		sprintf(proc_path, "/proc/%d/task/%d/children", pid, pid);
		usleep(200000); /* 200ms, time open handler has to start program */
		f = fopen(proc_path, "r");
		fscanf(f, "%d", &proc_pid);
		fclose(f);

		if (proc_pid) { /* a program was launched by open handler */
			sprintf(proc_path, "/proc/%d/comm", proc_pid);
			f = fopen(proc_path, "r");
			fgets(proc_name, 256, f);
			fclose(f);

			/* check if its a cli program fuf should wait for */
			char programs[strlen(CLI_PROGRAMS)+1];
			strcpy(programs, CLI_PROGRAMS);
			char *program = strtok(programs, " ");
			while(program) {
				if (!strncmp(proc_name, program, strlen(program))) {
					while(wait(NULL) != pid);
					handle_redraw(); /* redraw since its probably fucked */
					break;
				}
				program = strtok(NULL, " ");
			}
		} else { /* edge case: open handler didnt launch anything */
			while(wait(NULL) != pid);
		}

		/* set sigchld handler, no zombies allowed */
		signal(SIGCHLD, SIG_IGN);
	}
}

static void
display_load()
{
	WINDOW *prompt = newwin(1, COLS, LINES-1, 1);
	extern bool items_loading;
	while (items_loading) {
		mvwprintw(prompt, 0, 0, "loading: %d", items_len);
		wrefresh(prompt);
	}
	delwin(prompt);
}

static void
load_items()
{
	DIR *dp;
	struct dirent *ep;     

	sel_item = 0;
	items_len = 0;
	if ((dp = opendir(".")))
	{
		extern bool items_loading;
		while ((ep = readdir(dp)) && items_loading) {
			/* get rid of . and .. */
			if (ep->d_name[0] == '.' && 
					(ep->d_name[1] == '\0' || 
					 (ep->d_name[1] == '.' && ep->d_name[2] == '\0'))) {
					continue;
			}

			items = realloc(items, sizeof(item)*++items_len);
			strcpy(items[items_len-1].name ,ep->d_name);

			/* symlinked dirs treated as regular dirs */
			struct stat sb;
			stat(ep->d_name, &sb);
			items[items_len-1].dir = S_ISDIR(sb.st_mode);
			/* symlink info stored for the rest */
			lstat(ep->d_name, &sb);
			items[items_len-1].size = sb.st_size;
			items[items_len-1].mtime = sb.st_mtime;
			items[items_len-1].mode = sb.st_mode;
		}
		closedir(dp);
		items_loading = false;
	}
	qsort(items, items_len, sizeof(item), sort);
	refresh_layout();
}

static void
load_preview()
{
	/* gets called once before anything is loaded somehow */
	if (!items_len) {
		return;
	}
	WINDOW *preview_w = newwin(LINES-2, COLS/2-2, 1, COLS/2+1);

	char file[256];
	strcpy(file, items[sel_item].name);
	const char *home;
	char preview_cmd[PATH_MAX];
	if (!(home = getenv("HOME"))) {
		home = getpwuid(getuid())->pw_dir;
	}
	sprintf(preview_cmd, "%s/.config/fuf/preview \"%s\" %d %d 2>&1",
 			home, file, COLS/2-2, LINES-2);

	int fd;
	FILE *fp = NULL;
	int l = 0;
	char buf[COLS/2];
	extern pid_t preview_pid;
	preview_pid = ext_popen(preview_cmd, &fd);
	fp = fdopen(fd, "r");
	while (fgets(buf, COLS/2, fp)) {
		mvwaddstr(preview_w, l++, 0, buf);
	}
	wrefresh(preview_w);
	delwin(preview_w);

	fclose(fp);
	preview_pid = 0;
}

static void
refresh_layout()
{
	if (!items) {
		return;
	}
	WINDOW *preview_w = newwin(LINES, COLS/2, 0, COLS/2);
	WINDOW *dir_w= newwin(LINES, COLS/2, 0, 1);
	box(preview_w, 0, 0);
	box(dir_w, 0, 0);
	mvwaddch(preview_w, 0, 0, ACS_TTEE);
	mvwaddch(preview_w, LINES-1, 0, ACS_BTEE);

	/* select previous dir if set */
	if (strlen(goto_item) > 0) {
		unsigned prev_pos = 0;
		while (sel_item != ++prev_pos%items_len &&
				strcmp(goto_item, items[prev_pos].name));
		sel_item = prev_pos%items_len;
		strcpy(goto_item, "");
	}
	
	/* scroll to out of bounds sel_item */
	scroll_pos = sel_item > scroll_pos+LINES-3 
		? sel_item-LINES+3 : sel_item < scroll_pos
		? sel_item : scroll_pos;
	
	/* top bar */
	char cwd[PATH_MAX]; /* left corner */
	getcwd(cwd, PATH_MAX);
	mvwaddstr(dir_w, 0, 0, strlen(cwd) > COLS/2 ?
			cwd + (strlen(cwd)-COLS/2+1)/2*2 : cwd);
	char index[80];    /* right corner */
	sprintf(index, "%u/%u", sel_item+1, items_len);
	mvwaddstr(preview_w, 0, COLS/2-strlen(index), index);

	/* dir contents */
	for (unsigned i = scroll_pos, y = 1; y < LINES-1 && i < items_len;
			++i, ++y) {
		if (i == sel_item) {
			wattron(dir_w, A_REVERSE); 
			mvwaddcolitem(dir_w, y, 2, items[i].name, items[i].mode);
			wattroff(dir_w, A_REVERSE);
		} else {
			mvwaddcolitem(dir_w, y, 2, items[i].name, items[i].mode);
		}
	}

	/* bottom bar */
	struct stat sb;
	if (lstat(items[sel_item].name, &sb)) { /* left corner */
		wattron(dir_w, COLOR_PAIR(COL(COLOR_RED, COLOR_DEFAULT)) | A_BOLD);
		mvwaddstr(dir_w, LINES-1, 0, "file not found");
		wattroff(dir_w, COLOR_PAIR(COL(COLOR_RED, COLOR_DEFAULT)) | A_BOLD);
	} else {
		char *ext = strrchr(items[sel_item].name, '.');
		ext = ext == items[sel_item].name ? NULL : ext;
		mvwprintw(dir_w, LINES-1, 0, ext ?
				"%c%c%c%c%c%c%c%c%c%c %d %s %s %lu %s" :
				"%c%c%c%c%c%c%c%c%c%c %d %s %s %lu",
			S_ISDIR(sb.st_mode) ? 'd' :
			S_ISLNK(sb.st_mode) ? 'l' : '-',
			sb.st_mode & S_IRUSR ? 'r' : '-',
			sb.st_mode & S_IWUSR ? 'w' : '-',
			sb.st_mode & S_IXUSR && sb.st_mode & S_ISUID ? 's' :
			sb.st_mode & S_ISUID ? 'S' :
			sb.st_mode & S_IXUSR ? 'x' : '-',
			sb.st_mode & S_IRGRP ? 'r' : '-',
			sb.st_mode & S_IWGRP ? 'w' : '-',
			sb.st_mode & S_IXGRP && sb.st_mode & S_ISGID ? 's' :
			sb.st_mode & S_ISGID ? 'S' :
			sb.st_mode & S_IXGRP ? 'x' : '-',
			sb.st_mode & S_IROTH ? 'r' : '-',
			sb.st_mode & S_IWOTH ? 'w' : '-',
			sb.st_mode & S_IXOTH && sb.st_mode & S_ISVTX ? 't' :
			sb.st_mode & S_ISVTX ? 'T' :
			sb.st_mode & S_IXOTH ? 'x' : '-',
			sb.st_nlink,
			getpwuid(sb.st_uid) ? getpwuid(sb.st_uid)->pw_name : "???",
			getgrgid(sb.st_gid) ? getgrgid(sb.st_gid)->gr_name : "???",
			sb.st_size, ext);
		char timedate[80];                  /* right corner */
		strftime(timedate, 80, "%x %a %H:%M:%S", localtime(&items[sel_item].mtime));
		mvwaddstr(preview_w, LINES-1, COLS/2-strlen(timedate), timedate);
	}

	wrefresh(dir_w);
	wrefresh(preview_w);
	delwin(dir_w);
	delwin(preview_w);
	stop_preview();
	start_preview(load_preview);
}

static void
usage()
{
	fputs("usage: fuf [-aAsStTvh] [PATH]\n"
			"-a\tsort alphabetical, this is default\n"
			"-A\tsort alphabetical, reversed\n"
			"-s\tsort by size\n"
			"-S\tsort by size, reversed\n"
			"-t\tsort by time\n"
			"-T\tsort by time, resersed\n"
			"-v\tversion info\n"
			"-h\tusage info\n", stderr);
	exit(1);
}

int
main(int argc, char *argv[])
{
	if (argc > 1) {
		chdir(argv[argc-1]);
	}

	/* handle options */
	char c;
	if ((c = getopt(argc, argv, "aAsStTvh")) != -1) {
		switch (c) {
			case 'a':
				sort = alpha_cmp;
				break;
			case 'A':
				sort = Alpha_cmp;
				break;
			case 's':
				sort = size_cmp;
				break;
			case 'S':
				sort = Size_cmp;
				break;
			case 't':
				sort = time_cmp;
				break;
			case 'T':
				sort = Time_cmp;
				break;
			case 'v':
				puts("todo");
				exit(0);
				break;
			case 'h':
			default:
				usage();
		}
	}

	setlocale(LC_ALL, "");
	initscr();
	cbreak();
	noecho();
	curs_set(0);
	init_colors();
	handle_redraw();
	start_load(load_items, display_load);
	signal(SIGWINCH, handle_redraw);
	signal(SIGCHLD, SIG_IGN);

	/* main key handling loop */
	char cwd[PATH_MAX];
	char ch;
	while((ch = getch()) != 'q') {
		stop_load();
		switch(ch) {
			case 'j':
				sel_item += sel_item < items_len-1 ? 1 : 0;
				refresh_layout();
				break;
			case 'k':
				sel_item -= sel_item > 0 ? 1 : 0;
				refresh_layout();
				break;
			case 'h':
				getcwd(cwd, PATH_MAX);
				strcpy(goto_item, basename(cwd));
				chdir("..");
				start_load(load_items, display_load);
				break;
			case 'l':
				if (items[sel_item].dir) {
					chdir(items[sel_item].name);
					start_load(load_items, display_load);
				} else {
					open_file();
				}
				break;
			case 'r':
				strcpy(goto_item, items[sel_item].name);
				start_load(load_items, display_load);
				break;
			case 'R':
				handle_redraw();
				break;
			case 'g':
				if (ch_prompt("goto: [g] top") == 'g') {
					sel_item = 0;
				}
				refresh_layout();
				break;
			case 'G':
				sel_item = items_len-1;
				refresh_layout();
				break;
			case 's':
				switch(ch_prompt("sort: "
							"[a/A] alphabetical, "
							"[s/S] size, "
							"[t/T] time ")) {
					case 'a':
						sort = alpha_cmp;
						break;
					case 'A':
						sort = Alpha_cmp;
						break;
					case 's':
						sort = size_cmp;
						break;
					case 'S':
						sort = Size_cmp;
						break;
					case 't':
						sort = time_cmp;
						break;
					case 'T':
						sort = Time_cmp;
						break;
				}
				qsort(items, items_len, sizeof(item), sort);
				refresh_layout();
				break;
			case 'H':
				sel_item = scroll_pos;
				refresh_layout();
				break;
			case 'M':
				sel_item = items_len > scroll_pos + LINES-3 ?
					scroll_pos + (LINES-3)/2 : scroll_pos + (items_len-scroll_pos)/2;
				refresh_layout();
				break;
			case 'L':
				sel_item = items_len > scroll_pos + LINES-3 ?
					scroll_pos + LINES-3 : scroll_pos + items_len-1;
				refresh_layout();
				break;
			case CTRL('d'):
				if (sel_item + (LINES-3)+(LINES-3)/2 < items_len-1) {
					scroll_pos += scroll_pos + LINES-3 < items_len-1 ?
						(LINES-3)/2 : 0;
					sel_item += (LINES-3)/2;
				} else {
					scroll_pos = items_len-1 > LINES-3 ?
						items_len-1 - LINES+3 : 0;
					sel_item = sel_item + (LINES-3)/2 < items_len-1 ?
						sel_item + (LINES-3)/2 : items_len-1;
				}
				refresh_layout();
				break;
			case CTRL('u'):
				if (scroll_pos > (LINES-3)/2) {
					scroll_pos -= (LINES-3)/2;
					sel_item -= (LINES-3)/2;
				} else {
					scroll_pos = 0;
					sel_item = sel_item > (LINES-3)/2 ?
						sel_item - (LINES-3)/2 : 0;
				}
				refresh_layout();
				break;
			case CTRL('f'):
				if (scroll_pos + (LINES-3)*2 < items_len-1) {
					scroll_pos += LINES-3;
					sel_item += sel_item + LINES-3 < items_len-1 ?
						LINES-3 : 0;
				} else {
					sel_item += items_len-1 > LINES-3 ?
						items_len-1 - LINES+3 - scroll_pos : 0;
					scroll_pos = items_len-1 > LINES-3 ?
						items_len-1 - LINES+3 : 0;
						
				}
				refresh_layout();
				break;
			case CTRL('b'):
				if (scroll_pos > LINES-3) {
					scroll_pos -= LINES-3;
					sel_item -= LINES-3;
				} else {
					sel_item -= items_len-1 > LINES-3 ?
						scroll_pos : 0;
					scroll_pos = 0;
				}
				refresh_layout();
				break;
			case CTRL('e'):
				scroll_pos += scroll_pos + LINES-3 < items_len-1;
				sel_item = scroll_pos > sel_item ?
					scroll_pos : sel_item;
				refresh_layout();
				break;
			case CTRL('y'):
				scroll_pos -= scroll_pos > 0;
				sel_item = scroll_pos+LINES-3 < sel_item ?
					scroll_pos+LINES-3 : sel_item;
				refresh_layout();
				break;
			case '/':
				str_prompt("search: ", search);
				search_next(false);
				refresh_layout();
				break;
			case 'n':
				search_next(false);
				refresh_layout();
				break;
			case 'N':
				search_next(true);
				refresh_layout();
				break;
			case 'z':
				switch(ch_prompt("focus selection: "
							"[z] center, "
							"[b] bottom, "
							"[t] top")) {
					case 'z':
						scroll_pos = items_len-1 > LINES-3 &&
							sel_item >= (LINES-3)/2 &&
							sel_item < items_len - (LINES-3)/2 ?
							sel_item - (LINES-3)/2 : scroll_pos;
						break;
					case 'b':
						scroll_pos = items_len-1 > LINES-3 &&
							sel_item >= LINES-3 ?
							sel_item - LINES-3 : scroll_pos;
						break;
					case 't':
						scroll_pos = items_len-1 > LINES-3 &&
							sel_item < items_len - LINES+3 ?
							sel_item + LINES-3 : scroll_pos;
						break;
				}
				refresh_layout();
				break;
		}
	}
	endwin();
	return 0;
}
