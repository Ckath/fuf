#ifndef X_HACKS_H
#define X_HACKS_H
#include <X11/Xlib.h>
#include <X11/Xatom.h>

int xerrorignore(Display *dpy, XErrorEvent *ee);
int xwinpid(Display* dpy, Window w);

#endif
