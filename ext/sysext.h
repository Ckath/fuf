#ifndef SYSEXT_H
#define SYSEXT_H

void ext_kill(pid_t pid, int sig);
pid_t ext_popen(const char *command, int *fd);
char *ext_chldname(pid_t pid, char *name);
char *ext_filesize(long size, char *humanfs);
char *ext_shesc(char *s);
char * ext_itoa(int i, char *s);

#endif
