/* sysext.c
 * extended system functions
 * ext_kill: kills process and all child processes
 * ext_popen: popen clone that returns pid of process
 * ext_chldname: get name of youngest child process
 * ext_filesize: human readable filesize
 * ext_shesc: questionable escaping for strings passed to the shell
 * ext_itoa: itoa equivalent that reuses input buffer
 * ext_waitpid: wait until pid exists, regardless if its a child */

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "sysext.h"
unsigned s_idpos;

void
ext_kill(pid_t pid, int sig)
{
	FILE *f;
	char proc_path[256];

	while(pid) {
		sprintf(proc_path, "/proc/%d/task/%d/children", pid, pid);
		if (!(f = fopen(proc_path, "r"))) {
			kill(pid, sig);
			break;
		}
		pid_t child_pid;
		fscanf(f, "%d", &child_pid);
		fclose(f);
		kill(pid, sig);
		pid = pid == child_pid ? 0 : child_pid;
	}
}

pid_t
ext_popen(const char *command, int *fd)
{
	int p_stdout[2];
	pid_t pid;

	if (pipe(p_stdout) != 0) {
		return -1;
	} if ((pid = fork()) < 0) {
		return pid;
	} else if (pid == 0) {
		dup2(p_stdout[1], STDOUT_FILENO);
		close(p_stdout[0]);
		close(p_stdout[1]);

		execl("/bin/bash", "bash", "-c", command, NULL);
		perror("execl");
		exit(1);
	}

	close(p_stdout[1]);
	*fd = p_stdout[0];
	return pid;
}

char *
ext_chldname(pid_t pid, char *name)
{
	char path[256];
	int chld_pid = pid;
	FILE *f;
	do {
		pid = chld_pid;
		sprintf(path, "/proc/%d/task/%d/children", pid, pid);
		if (!(f = fopen(path, "r"))) {
			return NULL;
		}
		chld_pid = 0;
		fscanf(f, "%d", &chld_pid);
		fclose(f);
	} while(chld_pid);

	if (!pid) {
		name[0] = '\0';
		return NULL;
	}

	sprintf(path, "/proc/%d/comm", pid);
	f = fopen(path, "r");
	fgets(name, 256, f);

	fclose(f);
	return name;
}

char *
ext_filesize(long size, char *humanfs)
{
	const char units[] = {'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'};
	int i = 0;
	double fs = (double) size;
	while (fs > 1024) {
		fs /= 1024;
		++i;
	}
	sprintf(humanfs, i ? "%.*f%c":"%.*f", i, fs, units[i-1]);
	return humanfs;
}

char *
ext_shesc(char *s)
{
	if (!s) {
		return s;
	}

	char tmp[256];
	strcpy(tmp, s);
	int i = 0, offset = 0;
	for (; tmp[i]; ++i) {
		if (tmp[i] == '`' || tmp[i] == '"') {
			s[i+offset++] = '\\';
		}
		s[i+offset] = tmp[i];
	}
	s[i+1] = '\0';
	return s;
}

char *
ext_itoa(int i, char *s)
{
	s_idpos = s[0] ? s_idpos : 0;
	char *o = s+s_idpos;
	sprintf(o, "%d", i);
	s_idpos += strlen(o)+1;
	return o;
}

void
ext_waitpid(pid_t pid)
{
	struct stat sb;
	char proc_pid[256];
	sprintf(proc_pid, "/proc/%d", pid);
	while(stat(proc_pid, &sb) == 0) {
		usleep(10000);
	}
}
