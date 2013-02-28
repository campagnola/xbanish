/*
 * xbanish
 * Copyright (c) 2013 joshua stein <jcs@jcs.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/cursorfont.h>

void snoop(Display *, Window);
void usage(void);
Cursor blank_cursor(Display *, Window);

int
main(int argc, char *argv[])
{
	Display *dpy;
	int debug = 0, hiding = 0, ch;

	while ((ch = getopt(argc, argv, "d")) != -1)
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (!(dpy = XOpenDisplay(NULL)))
		errx(1, "can't open display %s", XDisplayName(NULL));

	/* recurse from root window down */
	snoop(dpy, DefaultRootWindow(dpy));

	for (;;) {
		XEvent e;
		XNextEvent(dpy, &e);

		switch (e.type) {
		case KeyRelease:
			if (debug)
				printf("keystroke %d, %shiding cursor\n",
				    e.xkey.keycode, (hiding ? "already " :
				    ""));

			if (!hiding) {
				Cursor c = blank_cursor(dpy,
				    DefaultRootWindow(dpy));
				XGrabPointer(dpy, DefaultRootWindow(dpy), 0,
				    PointerMotionMask | ButtonPressMask |
				    ButtonReleaseMask, GrabModeAsync,
				    GrabModeAsync, None, c, CurrentTime);

				hiding = 1;
			}

			break;

		case ButtonRelease:
		case MotionNotify:
			if (debug)
				printf("mouse moved to %d,%d, %sunhiding "
				    "cursor\n", e.xmotion.x, e.xmotion.y,
				    (hiding ? "" : "already "));

			if (hiding) {
				XUngrabPointer(dpy, CurrentTime);
				hiding = 0;
			}

			break;

		case CreateNotify:
			if (debug)
				printf("created new window, snooping on it\n");

			/* not sure why snooping directly on the window doesn't
			 * work, so recurse from its parent (probably root) */
			snoop(dpy, e.xcreatewindow.parent);

			break;
		}
	}
}

void
snoop(Display *dpy, Window win)
{
	Window parent, root, *kids;
	XSetWindowAttributes nattrs;
	unsigned int nkids, i;

	/* firefox stops responding to keys when KeyPressMask is used, so
	 * settle for keyreleasemask */
	int type = PointerMotionMask | KeyReleaseMask | Button1MotionMask |
		Button2MotionMask | Button3MotionMask | Button4MotionMask |
		Button5MotionMask | ButtonMotionMask;

	if (XQueryTree(dpy, win, &root, &parent, &kids, &nkids) == FALSE)
		err(1, "can't query window tree\n");

	if (!nkids)
		goto done;

	XSelectInput(dpy, root, type);

	/* listen for newly mapped windows */
	nattrs.event_mask = SubstructureNotifyMask;
	XChangeWindowAttributes(dpy, root, CWEventMask, &nattrs);

	/* recurse */
	for (i = 0; i < nkids; i++) {
		XSelectInput(dpy, kids[i], type);
		snoop(dpy, kids[i]);
	}

done:
	if (kids != NULL)
		XFree(kids); /* hide yo kids */
}

Cursor
blank_cursor(Display *dpy, Window win)
{
	Pixmap mask;
	XColor nocolor;
	Cursor nocursor;

	mask = XCreatePixmap(dpy, win, 1, 1, 1);
	nocolor.pixel = 0;
	nocursor = XCreatePixmapCursor(dpy, mask, mask, &nocolor, &nocolor, 0,
	    0);

	XFreePixmap(dpy, mask);

	return nocursor;
}

void
usage(void)
{
	fprintf(stderr, "usage: xbanish [-d]\n");
	exit(1);
}
