/* * See LICENSE file for copyright and license details.  */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <libgen.h>
#include <X11/cursorfont.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#include "drw.h"
#include "util.h"
#include "arg.h"

#ifndef PIPE_BUF
#define PIPE_BUF 4096
#endif

#define PING_TIMEOUT 300

#define LENGTH(x)                (sizeof((x)) / sizeof(*(x)))
#define CLEANMASK(mask)          (mask & (MODKEY))

enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeLast }; /* color schemes */

typedef union {
	int i;
	const void *v;
} Arg;

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	Window win;
	char name[256];
	char title[256];
} SwtWindow;

static void buttonpress(const XEvent *ev);
static void cleanup(void);
static void closefifo(void);
static void closewindow(const Arg *arg);
static void createfifo(void);
static void createout(void);
static void destroynotify(const XEvent *ev);
static void dumptree(void);
static void dumpwindow(SwtWindow *w);
static void *emallocz(size_t size);
static void *erealloc(void *o, size_t size);
static void focusin(const XEvent *ev);
static void keypress(const XEvent *ev);
static void noop(void);
static void procinput(void);
static void procadd(char *attrs);
static void procremove(char *attrs);
static void procshow(char *attrs);
static void procwindow(char *attrs);
static void procx11events(void);
static void resetfifo(void);
static void run(void);
static int  getwindow(Window w);
static void setup(void);
static void usage(void);
static void quit(const Arg *arg);
static void writeout(const char *msg, ...);

/* variables */
char *argv0;

static int infd = -1;
static int winfd = -1; /* don't write here, it prevents EOF */
static int x11fd = -1;

static int screen;
static void (*handler[LASTEvent]) (const XEvent *) = {
	[ButtonPress] = buttonpress,
	[KeyPress] = keypress,
	[FocusIn] = focusin,
	[DestroyNotify] = destroynotify,
};

static int sw, sh, wx, wy, ww, wh;
static FILE *outfile;
static char *in = NULL, *out = NULL;
static Bool running = True;
static Display *dpy;
static Window root;
static SwtWindow **windows;
static int nwindows = 0;
static int sel = -1;
static Drw *drw;
static Fnt *fnt;
static Cur *cursor[CurLast];
static ClrScheme scheme[SchemeLast];

#include "config.h"

void
buttonpress(const XEvent *e) {
	const XButtonPressedEvent *ev = &e->xbutton;

	writeout("button pressed: %d (%d,%d)\n", ev->button, ev->x, ev->y);
}

void
closefifo(void) {
	close(winfd);
	close(infd);
}

void
closewindow(const Arg *arg) {
	if(sel < 0) return;
	XDestroyWindow(dpy, windows[sel]->win);
}

void
createfifo(void) {
	if(access(in, F_OK) == -1)
		mkfifo(in, S_IRWXU);

	if((infd = open(in, O_RDONLY | O_NONBLOCK, 0)) == -1) {
		perror("swt unable to open input fifo for reading");
		exit(EXIT_FAILURE);
	}

	if((winfd = open(in, O_WRONLY, 0)) == -1) {
		perror("swt unable to open input fifo for writting");
		exit(EXIT_FAILURE);
	}
}

void
createout(void) {
	if(!(outfile = fopen(out, "a"))) {
		perror("swt unable to open output file");
		exit(EXIT_FAILURE);
	}
}

SwtWindow *
createwindow(char *name, char *title) {
	XClassHint class_hint;
	XTextProperty xtp;
	SwtWindow *swtwin;

	swtwin = emallocz(sizeof(*swtwin));

	swtwin->win = XCreateSimpleWindow(dpy, root, wx, wy, ww, wh, 0,
			scheme[SchemeNorm].fg->rgb, scheme[SchemeNorm].bg->rgb);
	XSelectInput(dpy, swtwin->win, KeyPressMask|ButtonPressMask|StructureNotifyMask|FocusChangeMask);

	class_hint.res_name = name;
	class_hint.res_class = "SWT";
	XSetClassHint(dpy, swtwin->win, &class_hint);

	if(XmbTextListToTextProperty(dpy, (char **)&title, 1, XCompoundTextStyle,
				&xtp) == Success) {
		XSetTextProperty(dpy, swtwin->win, &xtp, XA_WM_NAME);
		XFree(xtp.value);
	}

	strncpy(swtwin->name, name, sizeof(swtwin->name)-1);
	swtwin->name[sizeof(swtwin->name)-1] = '\0';
	strncpy(swtwin->title, title, sizeof(swtwin->title)-1);
	swtwin->title[sizeof(swtwin->title)-1] = '\0';

	nwindows++;
	windows = erealloc(windows, sizeof(SwtWindow *) * nwindows);

	windows[nwindows - 1] = swtwin;

	return swtwin;
}

void
destroynotify(const XEvent *e) {
	const XDestroyWindowEvent *ev = &e->xdestroywindow;
	int w = getwindow(ev->window);

	if(!nwindows) {
		return;
	} else if(w == 0) {
		/* First client. */
		nwindows--;
		free(windows[0]);
		memmove(&windows[0], &windows[1], sizeof(SwtWindow *) * nwindows);
	} else if(w == nwindows - 1) {
		/* Last client. */
		nwindows--;
		free(windows[w]);
		windows = erealloc(windows, sizeof(SwtWindow *) * nwindows);
	} else {
		/* Somewhere inbetween. */
		free(windows[w]);
		memmove(&windows[w], &windows[w+1],
				sizeof(SwtWindow *) * (nwindows - (w + 1)));
		nwindows--;
	}
}

void
dumptree(void) {
	for(int i=0; i<nwindows;i++) {
		dumpwindow(windows[i]);
	}
}

void
dumpwindow(SwtWindow *w) {
	writeout("window xid=%lu name=%s title=%s\n", w->win, w->name, w->title);
}

void *
emallocz(size_t size) {
	void *p;

	if(!(p = calloc(1, size)))
		die("swt cannot malloc\n");
	return p;
}

void *
erealloc(void *o, size_t size) {
	void *p;

	if(!(p = realloc(o, size)))
		die("swt cannot realloc\n");
	return p;
}

void
focusin(const XEvent *e) {
	const XFocusChangeEvent *ev = &e->xfocus;
	int dummy;
	Window focused;

	if(ev->mode != NotifyUngrab) {
		XGetInputFocus(dpy, &focused, &dummy);
		sel = getwindow(focused);
	}
}

void
keypress(const XEvent *e) {
	const XKeyEvent *ev = &e->xkey;
	unsigned int i;
	KeySym keysym;

	keysym = XkbKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0, 0);
	for(i = 0; i < LENGTH(keys); i++) {
		if(keysym == keys[i].keysym
				&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
				&& keys[i].func) {
			keys[i].func(&(keys[i].arg));
		}
	}
}

void
noop(void) {
	time_t t;

	t = time(NULL);
	writeout("NOOP %lu\n", t);
}

void
procinput(void) {
	size_t len = -1;
	char *input;
	char buf[PIPE_BUF];

	if((len = read(infd, buf, PIPE_BUF)) == -1) {
		perror("swt failed to read from pipe");
	}

	if(len > 0) {
		buf[len - 1] = '\0';
	} else {
		resetfifo();
		return;
	}

	for(input = buf;;input = NULL) {
		char *command = NULL, *attributes = NULL;

		command = strtok(input, ";");
		if(command == NULL)
			break;

		if((attributes = strchr(command, ' '))) {
			*(attributes++) = '\0';
		} else {
			if(strcasecmp("noop", command) == 0) {
				noop();
			} else if(strcasecmp("dump", command) == 0) {
				dumptree();
			} else if(strcasecmp("quit", command) == 0) {
				quit(NULL);
			} else if(strcasecmp("window", command) == 0) {
				procwindow(NULL);
			} else
				writeout("ERROR parsing command: %s\n", command);
			continue;
		}

		if(strcasecmp("window", command) == 0) {
			procwindow(attributes);
		} else if(strcasecmp("add", command) == 0) {
			procadd(attributes);
		} else if(strcasecmp("show", command) == 0) {
			procshow(attributes);
		} else if(strcasecmp("remove", command) == 0) {
			procremove(attributes);
		} else {
			writeout("ERROR unknown command: %s(%s)\n", command, attributes);
		}
		fflush(NULL);
	}
}

void
procadd(char *attrs) {
}

void
procremove(char *attrs) {
}

void
procshow(char *attrs) {
}

void
procwindow(char *attrs) {
	char *name = NULL, *title = NULL;
	SwtWindow *sw;

	name = attrs ? attrs : "swt";
	if(attrs && (title = strchr(attrs, ' '))) {
		*(title++) = '\0';
	} else {
		title = "swt window";
	}

	sw = createwindow(name, title);

	XMapWindow(dpy, sw->win);

	XSync(dpy, False);
	writeout("window %s %lu\n", sw->name, sw->win);
}

void
procx11events(void) {
	XEvent ev;

	while(XPending(dpy)) {
		XNextEvent(dpy, &ev);
		if(handler[ev.type])
			handler[ev.type](&ev);
	}
}

void
quit(const Arg *arg) {
	running = False;
}

void
resetfifo(void) {
	closefifo();
	createfifo();
}

void
run(void) {
	time_t last_response;

	last_response = time(NULL);
	x11fd = XConnectionNumber(dpy);

	for(;;) {
		int i, nfds = 0;
		fd_set rd;
		struct timeval tv = { .tv_sec = PING_TIMEOUT / 5, .tv_usec = 0 };

		if (!running) break;

		FD_ZERO(&rd);
		FD_SET(infd, &rd);
		FD_SET(x11fd, &rd);
		nfds = MAX(nfds, infd);
		nfds = MAX(nfds, x11fd);

		i = select(nfds + 1, &rd, NULL, NULL, &tv);

		if(i == -1 && errno == EINTR) continue;

		if(i == -1) {
			perror("swt error on select()");
			exit(EXIT_FAILURE);
		} else if(i == 0) {
			if(time(NULL) - last_response >= PING_TIMEOUT) {
				writeout("NOOP\n");
			}
			continue;
		} else {
			if(FD_ISSET(infd, &rd)) {
				last_response = time(NULL);
				procinput();
			}
			if(FD_ISSET(x11fd, &rd)) {
				last_response = time(NULL);
				procx11events();
			}
		}
	}

	writeout("done\n");
}

int
getwindow(Window w) {
	for(int i=0;i<nwindows;i++) {
		if(w == windows[i]->win) {
			return i;
		}
	}
	return -1;
}

void
setup(void) {
	createfifo();
	createout();

	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	fnt = drw_font_create(dpy, font);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	wx = 0;
	wy = 0;
	ww = 800;
	wh = 600;

	drw = drw_create(dpy, screen, root, sw, sh);
	drw_setfont(drw, fnt);

	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);

	scheme[SchemeNorm].border = drw_clr_create(drw, normbordercolor);
	scheme[SchemeNorm].bg = drw_clr_create(drw, normbgcolor);
	scheme[SchemeNorm].fg = drw_clr_create(drw, normfgcolor);
	scheme[SchemeSel].border = drw_clr_create(drw, selbordercolor);
	scheme[SchemeSel].bg = drw_clr_create(drw, selbgcolor);
	scheme[SchemeSel].fg = drw_clr_create(drw, selfgcolor);
}

void
cleanup(void) {
	drw_clr_free(scheme[SchemeNorm].border);
	drw_clr_free(scheme[SchemeNorm].bg);
	drw_clr_free(scheme[SchemeNorm].fg);
	drw_clr_free(scheme[SchemeSel].border);
	drw_clr_free(scheme[SchemeSel].bg);
	drw_clr_free(scheme[SchemeSel].fg);

	drw_cur_free(drw, cursor[CurNormal]);
	drw_cur_free(drw, cursor[CurResize]);
	drw_cur_free(drw, cursor[CurMove]);

	drw_font_free(dpy, fnt);
	drw_free(drw);

	closefifo();

	if(fclose(outfile) == -1) {
		perror("swt unable to close outfile");
	}
}

void
usage(void) {
	die("usage: %s [-v] -i <infifo> -o <outfile>\n", basename(argv0));
}

void
writeout(const char *msg, ...) {
	va_list ap;

	va_start(ap, msg);
	vfprintf(outfile, msg, ap);
	va_end(ap);

	fflush(outfile);
}

int
main(int argc, char *argv[]) {
	ARGBEGIN {
	case 'i':
		in = EARGF(usage());
		break;
	case 'o':
		out = EARGF(usage());
		break;
	case 'v':
		die("swt-"VERSION", © 2013 swt engineers"
				", see LICENSE for details.\n");
	default:
		usage();
	} ARGEND;

	if(!in || !out) usage();

	if(!(dpy = XOpenDisplay(NULL)))
		die("swt cannot open display\n");

	setup();
	run();
	cleanup();

	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}

/* vim: set noexpandtab : */
