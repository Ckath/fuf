#ifndef SYSEXT_H
#define SYSEXT_H

void ext_kill(pid_t pid, int sig);
pid_t ext_popen(const char *command, int *fd);
char *ext_chldname(pid_t pid, char *name);

#endif
