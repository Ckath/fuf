#include <ctype.h>
#include <dirent.h>
#include <grp.h>
#include <libgen.h>
#include <locale.h>
#include <ncurses.h>
#include <pthread.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include "inc/colors.h"
#include "inc/sncurses.h"
#include "inc/sort.h"
#include "inc/sysext.h"
#include "inc/thr.h"
#include "inc/item.h"

#define CLI_PROGRAMS "vi nvim nano dhex man w3m elinks links2 links lynx"

static void handle_redraw();  /* utility */
static void handle_chld();
static void init();
static char ch_prompt(char *prefix);
static void str_prompt(char *prefix, char *result);
static void search_next(bool reverse);
static void open_file();      /* async */
static void open_with(char *launcher, char *file, bool cli);
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
char open_path[PATH_MAX];
char preview_path[PATH_MAX];

static void
handle_chld()
{
	while (waitpid((pid_t)(-1), 0, WNOHANG) > 0); 
}

static void
handle_redraw()
{
	sendwin();
	srefresh();

	/* keep the file list within resized window */
	scroll_pos = items_len-scroll_pos < LINES+2
		? 0 : scroll_pos;

	extern bool items_loading;
	if (!items_loading) {
		cancel_preview();
		refresh_layout();
	}
}

static void
init()
{
	/* check location of scripts */
	setlocale(LC_ALL, "");
	char user_config[PATH_MAX];
	struct stat sb;
	const char *home;
	if (!(home = getenv("HOME"))) {
		home = getpwuid(getuid())->pw_dir;
	}
	sprintf(user_config, "%s/.config/fuf/open", home);
	sprintf(open_path, stat(user_config, &sb) == 0 ? user_config : "/usr/lib/fuf/open");
	sprintf(user_config, "%s/.config/fuf/preview", home);
	sprintf(preview_path, stat(user_config, &sb) == 0 ? user_config : "/usr/lib/fuf/preview");

	/* ls_color check/set */
	extern char *ls_colors;
	if (strlen(getenv("LS_COLORS"))) {
		ls_colors = malloc(strlen(getenv("LS_COLORS")) * sizeof(char));
		strcpy(ls_colors, getenv("LS_COLORS"));
	} else {
		ls_colors = NULL;
	}

	/* init ncurses */
	initscr();
	cbreak();
	noecho();
	curs_set(0);
	init_colors();

	/* init fuf */
	handle_redraw();
	start_load(load_items, display_load);
	init_preview(load_preview);
	signal(SIGWINCH, handle_redraw);
	signal(SIGCHLD, handle_chld);
}

static char
ch_prompt(char *prefix)
{
	WINDOW *prompt = snewwin(1, COLS, LINES-1, 1);

	bool bold = false;
	char *str = strdup(prefix);
	char *tok = strtok(str, "[]");
	while (tok) {
		if (bold) {
			swattron(prompt, A_REVERSE | A_BOLD);
			wprintw(prompt, " %s ", tok);
			swattroff(prompt, A_REVERSE | A_BOLD);
		} else {
			waddstr(prompt, tok);
		}

		bold = !bold;
		tok = strtok(NULL, "[]");
	}
	free(tok);
	free(str);
	swrefresh(prompt);

	char c = getch();
	sdelwin(prompt);
	return c;
}

static void
str_prompt(char *prefix, char *result)
{
	WINDOW *prompt = snewwin(1, COLS, LINES-1, 1);
	smvwaddstr(prompt, 0, 0, prefix);
	keypad(prompt, true);
	swrefresh(prompt);

	int c;
	unsigned i = 0;
	while ((c = wgetch(prompt))) {
		curs_set(1);
		switch(c) {
			case KEY_LEFT: /* not handling these keys */
			case KEY_RIGHT:
			case KEY_UP:
			case KEY_DOWN:
			case 9: /* tab */
				continue;
			case 127:
			case KEY_BACKSPACE:
				smvwdelch(prompt, 0, (i > 0 ? --i : i)+strlen(prefix));
				continue;
			case 10:
			case KEY_ENTER:
				break;
			case '': /* ESC */
				strcpy(result, "");
				break;
			default:
				c = tolower(c);
				swaddch(prompt, c);
				result[i++] = c;
				continue;
		}
		result[i] = '\0';
		break;
	}

	curs_set(0);
	sdelwin(prompt);
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
	/* ensure no previews are loading */
	cancel_preview();

	pid_t pid = fork();
	if (!pid) { /* child: open file in program though open handler */
		char file[256];
		strcpy(file, items[sel_item].name);
		free(items);
		execlp(open_path, "open", file, NULL);
		_exit(1);
	} else { /* parent: check launched process */
		char proc_name[256];
		sleep(1); /* 1s, time open handler has to start program */

		if (ext_chldname(pid, proc_name)) { /* a program was launched by open handler */
			/* check if its a cli program fuf should wait for */
			char programs[strlen(CLI_PROGRAMS)+1];
			strcpy(programs, CLI_PROGRAMS);
			char *program = strtok(programs, " ");
			while(program) {
				if (!strncmp(proc_name, program, strlen(program))) {
					ext_waitpid(pid);
					break;
				}
				program = strtok(NULL, " ");
			}
		}

		handle_redraw(); /* redraw since its probably fucked */
	}
}

static void
open_with(char *launcher, char *file, bool cli)
{
	pid_t pid = fork();
	if (!pid) { /* child: open file */
		char cmd[PATH_MAX];
		sprintf(cmd, cli ? file ? "%s \"%s\"":"%s":
				file ? "%s \"%s\" &>/dev/null":"%s &>/dev/null",
				launcher, ext_shesc(file));
		free(items);
		execl("/bin/bash", "bash", "-c", cmd, NULL);
		_exit(1);
	} else { /* parent: check launched process */
		if (cli) {
			ext_waitpid(pid);
		}
	}

	handle_redraw(); /* redraw since its probably fucked */
}

static void
display_load()
{
	WINDOW *prompt = snewwin(1, COLS, LINES-1, 1);
	extern bool items_loading;
	while (items_loading) {
		smvwprintw(prompt, 0, 0, "loading: %d", items_len);
		swrefresh(prompt);
	}
	sdelwin(prompt);
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
			items[items_len-1].uid = sb.st_uid;
			items[items_len-1].gid = sb.st_gid;
			items[items_len-1].nlink = sb.st_nlink;
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
	char file[256];
	extern bool pn;
	char nthr = pn++;
	prctl(PR_SET_NAME, nthr ? "preview0" : "preview1");
	extern pthread_cond_t run_preview;
	extern pthread_mutex_t preview_lock;
	extern pthread_mutex_t preview_pid_lock;
	extern pid_t preview_pid[2];
	for (;;) {
		pthread_mutex_lock(&preview_lock);
		do {
			pthread_cond_wait(&run_preview, &preview_lock);
		} while(!items); /* edgecase, called before items are loaded */
		pthread_mutex_unlock(&preview_lock);

		strcpy(file, items[sel_item].name);
		WINDOW *preview_w = snewwin(LINES-2, COLS/2-2, 1, COLS/2+1);

		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		char preview_cmd[PATH_MAX];
		int col_px = w.ws_xpixel/COLS;
		int line_px = w.ws_ypixel/LINES;
		sprintf(preview_cmd, "%s \"%s\" %d %d %d %d %d %d 2>&1",
				preview_path, ext_shesc(file), COLS/2-2, LINES-2,
				col_px*(COLS/2-3), line_px*(LINES-2), /* preview img size */
				col_px*(COLS/2+1), line_px);          /* preview img pos */


		int fd;
		pthread_mutex_lock(&preview_pid_lock);
		preview_pid[nthr] = ext_popen(preview_cmd, &fd);
		pthread_mutex_unlock(&preview_pid_lock);

		int l = 0;
		char buf[COLS];
		FILE *fp = fdopen(fd, "r");
		while (fgets(buf, COLS, fp)) {
			smvwaddstr(preview_w, l++, 0, buf);
		}
		if (!(l == 1 && buf[0] == '\n')) { /* guess this was not an image */
			swrefresh(preview_w);
		}
		sdelwin(preview_w);

		fclose(fp);

		pthread_mutex_lock(&preview_pid_lock);
		preview_pid[nthr] = 0;
		pthread_mutex_unlock(&preview_pid_lock);
	}
}

static void
refresh_layout()
{
	if (!items) {
		return;
	}
	WINDOW *cwd_w;
	WINDOW *preview_w = snewwin(LINES, COLS/2, 0, COLS/2);
	WINDOW *dir_w= snewwin(LINES, COLS/2, 0, 1);
	sbox(preview_w, 0, 0);
	sbox(dir_w, 0, 0);
	smvwaddch(preview_w, 0, 0, ACS_TTEE);
	smvwaddch(preview_w, LINES-1, 0, ACS_BTEE);

	/* select previous dir if set */
	if (items_len && strlen(goto_item) > 0) {
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
	char index[80];    /* right corner */
	sprintf(index, "%u/%u", sel_item+1, items_len);
	smvwaddstr(preview_w, 0, COLS/2-strlen(index), index);

	char cwd[PATH_MAX]; /* left corner */
	bool cwdok = getcwd(cwd, PATH_MAX);
	if (cwdok) {
		wchar_t wcwd[PATH_MAX];
		swprintf(wcwd, PATH_MAX, L"%hs", cwd);
		int cwd_len = strlen(cwd) > wcslen(wcwd) ?
			strlen(cwd)-(strlen(cwd)-wcslen(wcwd))/2 :
			strlen(cwd);
		cwd_w = snewwin(1, cwd_len < COLS-strlen(index)-3 ?
				cwd_len : COLS-strlen(index)-3, 0, 1);
		smvwaddstr(cwd_w, 0, 0, cwd_len < COLS-strlen(index)-3 ?
				cwd : cwd + (cwd_len-COLS+strlen(index)+3));
	} else {
		swattron(dir_w, COLOR_PAIR(COL(COLOR_RED, COLOR_DEFAULT)) | A_BOLD);
		smvwaddstr(dir_w, 0, 0, "dir not found");
		swattroff(dir_w, COLOR_PAIR(COL(COLOR_RED, COLOR_DEFAULT)) | A_BOLD);
	}

	/* dir contents */
	for (unsigned i = scroll_pos, y = 1; y < LINES-1 && i < items_len;
			++i, ++y) {
		if (i == sel_item) {
			swattron(dir_w, A_REVERSE);
			mvwaddcolitem(dir_w, y, 2, items[i].name, items[i].mode);
			swattroff(dir_w, A_REVERSE);
		} else {
			mvwaddcolitem(dir_w, y, 2, items[i].name, items[i].mode);
		}
	}

	/* bottom bar */
	char fs[69];       /* left corner */
	char id[69];
	char *ext = strrchr(items[sel_item].name, '.');
	ext = ext == items[sel_item].name ? NULL : ext;
	smvwprintw(dir_w, LINES-1, 0, ext ?
			"%c%c%c%c%c%c%c%c%c%c %d %s %s %s %s" :
			"%c%c%c%c%c%c%c%c%c%c %d %s %s %s",
		S_ISDIR(items[sel_item].mode) ? 'd' :
		S_ISLNK(items[sel_item].mode) ? 'l' : '-',
		items[sel_item].mode & S_IRUSR ? 'r' : '-',
		items[sel_item].mode & S_IWUSR ? 'w' : '-',
		items[sel_item].mode & S_IXUSR &&
		items[sel_item].mode & S_ISUID ? 's' :
		items[sel_item].mode & S_ISUID ? 'S' :
		items[sel_item].mode & S_IXUSR ? 'x' : '-',
		items[sel_item].mode & S_IRGRP ? 'r' : '-',
		items[sel_item].mode & S_IWGRP ? 'w' : '-',
		items[sel_item].mode & S_IXGRP &&
		items[sel_item].mode & S_ISGID ? 's' :
		items[sel_item].mode & S_ISGID ? 'S' :
		items[sel_item].mode & S_IXGRP ? 'x' : '-',
		items[sel_item].mode & S_IROTH ? 'r' : '-',
		items[sel_item].mode & S_IWOTH ? 'w' : '-',
		items[sel_item].mode & S_IXOTH &&
		items[sel_item].mode & S_ISVTX ? 't' :
		items[sel_item].mode & S_ISVTX ? 'T' :
		items[sel_item].mode & S_IXOTH ? 'x' : '-',
		items[sel_item].nlink,
		getpwuid(items[sel_item].uid) ?
			getpwuid(items[sel_item].uid)->pw_name :
			ext_itoa(items[sel_item].uid, id),
		getgrgid(items[sel_item].gid) ?
			getgrgid(items[sel_item].gid)->gr_name :
			ext_itoa(items[sel_item].gid, id),
		ext_filesize(items[sel_item].size, fs), ext);
	char timedate[80]; /* right corner */
	strftime(timedate, 80, "%x %a %H:%M:%S", localtime(&items[sel_item].mtime));
	smvwaddstr(preview_w, LINES-1, COLS/2-strlen(timedate), timedate);

	swrefresh(dir_w);
	swrefresh(preview_w);
	sdelwin(dir_w);
	sdelwin(preview_w);
	if (cwdok) {
		swrefresh(cwd_w);
		sdelwin(cwd_w);
		queue_preview();
	}
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
				puts("fuf-"VERSION);
				exit(0);
			case 'h':
			default:
				usage();
		}
	}

	init();

	/* main key handling loop */
	char launcher[256];
	char cwd[PATH_MAX];
	char ch;
	while((ch = getch()) != 'q') {
		cancel_preview();
		stop_load();
		switch(ch) {
			case '\232': /* handle request for redraw after resize */
				refresh_layout();
				break;
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
			case CTRL('l'):
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
			case 'o':
				str_prompt("open with: ", launcher);
				if (launcher[0]) {
					open_with(launcher, items[sel_item].name, false);
				} else {
					refresh_layout();
				}
				break;
			case 'O':
				str_prompt("open with: ", launcher);
				if (launcher[0]) {
					open_with(launcher, items[sel_item].name, true);
				} else {
					refresh_layout();
				}
				break;
			case 't':
				endwin();
				open_with(getenv("SHELL"), NULL, true);
				break;
		}
	}
	endwin();
	return 0;
}
