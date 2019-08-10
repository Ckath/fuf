/* sysext.c
 * extended system functions
 * ext_kill: kills process and all child processes
 * ext_popen: popen clone that returns pid of process */

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sysext.h"

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
		pid = child_pid;
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
