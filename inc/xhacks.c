#include "xhacks.h"
#include "sysext.h"

int
xerrorignore(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

int
xwinpid(Display* dpy, Window w)
{
    unsigned long n;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, w, XInternAtom(dpy, "_NET_WM_PID", 0), 0,
		4194304, 0, XA_CARDINAL, N(Atom), N(int), &n, N(long), &data) || !n) {
        return 0;
	}

    int pid = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    XFree(data);
    return pid;
}
