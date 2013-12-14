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
typedef enum { HorizBox, VertBox } SwtType;

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
	int x;
	int y;
	int w;
	int h;
} Rect;

/* we don't really use SwtWidget */
/* but every widget has to look like it at the top */
/* maybe there is a better way */
typedef struct {
	SwtType type;
	Rect r;
	char name[256];
} SwtWidget;

typedef struct {
	SwtType type;
	Rect r;
	char name[256];
	SwtWidget *parent;
	SwtWidget **kids;
	int nkids;
} SwtBox;

typedef struct {
	Window win;
	char name[256];
	char title[256];
	Drw *drw;
	Fnt *fnt;
	Cur *cursor[CurLast];
	ClrScheme scheme[SchemeLast];
	SwtWidget **kids;
	int nkids;
} SwtWindow;

static void addbox(SwtWindow *w, char *battrs, Bool hbox);
static void cleanup(void);
static void cleanupwindow(SwtWindow *w);
static void closefifo(void);
static void closewindow(const Arg *arg);
static void configurenotify(const XEvent *ev);
static void createfifo(void);
static void createout(void);
static void destroynotify(const XEvent *ev);
static void draw(SwtWindow *w);
static void dumptree(void);
static void dumpwidget(SwtWidget *w);
static void dumpwindow(SwtWindow *w);
static void *emallocz(size_t size);
static void *erealloc(void *o, size_t size);
static void expose(const XEvent *ev);
static void focusin(const XEvent *ev);
static int  getwindow(Window w);
static int  getwindowc(char *name);
static void keypress(const XEvent *ev);
static void noop(void);
static void procinput(void);
static void procadd(char *attrs);
static void procremove(char *attrs);
static void procshow(char *attrs);
static void procwindow(char *attrs);
static void procx11events(void);
static void resetfifo(void);
static void resize(SwtWindow *w);
static void quit(const Arg *arg);
static void run(void);
static void setup(void);
static void usage(void);
static void writeout(const char *msg, ...);

/* variables */
char *argv0;

static int infd = -1;
static int winfd = -1; /* don't write here, it prevents EOF */
static int x11fd = -1;

static void (*handler[LASTEvent]) (const XEvent *) = {
	[KeyPress] = keypress,
	[FocusIn] = focusin,
	[DestroyNotify] = destroynotify,
	[ConfigureNotify] = configurenotify,
	[Expose] = expose,
};

static FILE *outfile;
static char *in = NULL, *out = NULL;
static Bool running = True;
static int screen;
static Display *dpy;
static Window root;
static SwtWindow **windows;
static int nwindows = 0;
static int sel = -1;

#include "config.h"

void
addbox(SwtWindow *w, char *battrs, Bool hbox) {
	writeout("add %s for %s\n", hbox ? "hbox":"vbox", w->name);
	SwtBox *box;

	box = emallocz(sizeof(*box));
	box->type = hbox ? HorizBox : VertBox;

	box->r.x = bordersize;
	box->r.y = bordersize;
	box->r.w = w->drw->w - (bordersize*2);
	box->r.h = w->drw->h - (bordersize*2);

	strncpy(box->name, battrs, sizeof(box->name)-1);

	w->nkids++;
	w->kids = erealloc(w->kids, sizeof(SwtWidget *) * w->nkids);
	w->kids[w->nkids -1] = (SwtWidget *)box;

	draw(w);
}

void
cleanup(void) {
	closefifo();

	if(fclose(outfile) == -1) {
		perror("swt unable to close outfile");
	}
}

void cleanupwindow(SwtWindow *w) {
	drw_clr_free(w->scheme[SchemeNorm].border);
	drw_clr_free(w->scheme[SchemeNorm].bg);
	drw_clr_free(w->scheme[SchemeNorm].fg);
	drw_clr_free(w->scheme[SchemeSel].border);
	drw_clr_free(w->scheme[SchemeSel].bg);
	drw_clr_free(w->scheme[SchemeSel].fg);

	drw_cur_free(w->drw, w->cursor[CurNormal]);
	drw_cur_free(w->drw, w->cursor[CurResize]);
	drw_cur_free(w->drw, w->cursor[CurMove]);

	drw_font_free(dpy, w->fnt);
	drw_free(w->drw);

	free(w);
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
configurenotify(const XEvent *e) {
	const XConfigureEvent *ev = &e->xconfigure;

	int w = getwindow(ev->window);

	if(w > -1 && (ev->width != windows[w]->drw->w || ev->height != windows[w]->drw->h)) {
		drw_resize(windows[w]->drw, ev->width, ev->height);
		resize(windows[w]);
	}
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

	swtwin->nkids = 0;
	swtwin->drw = drw_create(dpy, screen, root, DisplayWidth(dpy, screen), DisplayHeight(dpy, screen));
	swtwin->fnt = drw_font_create(dpy, font);
	drw_setfont(swtwin->drw, swtwin->fnt);

	swtwin->cursor[CurNormal] = drw_cur_create(swtwin->drw, XC_left_ptr);
	swtwin->cursor[CurResize] = drw_cur_create(swtwin->drw, XC_sizing);
	swtwin->cursor[CurMove] = drw_cur_create(swtwin->drw, XC_fleur);

	swtwin->scheme[SchemeNorm].border = drw_clr_create(swtwin->drw, normbordercolor);
	swtwin->scheme[SchemeNorm].bg = drw_clr_create(swtwin->drw, normbgcolor);
	swtwin->scheme[SchemeNorm].fg = drw_clr_create(swtwin->drw, normfgcolor);
	swtwin->scheme[SchemeSel].border = drw_clr_create(swtwin->drw, selbordercolor);
	swtwin->scheme[SchemeSel].bg = drw_clr_create(swtwin->drw, selbgcolor);
	swtwin->scheme[SchemeSel].fg = drw_clr_create(swtwin->drw, selfgcolor);
	
	swtwin->win = XCreateSimpleWindow(dpy, root, 0, 0, swtwin->drw->w, swtwin->drw->h, 0,
			swtwin->scheme[SchemeNorm].fg->rgb, swtwin->scheme[SchemeNorm].bg->rgb);
	XSelectInput(dpy, swtwin->win, KeyPressMask|ButtonPressMask|StructureNotifyMask|FocusChangeMask|ExposureMask);

	class_hint.res_name = name;
	class_hint.res_class = "SWT";
	XSetClassHint(dpy, swtwin->win, &class_hint);

	if(XmbTextListToTextProperty(dpy, (char **)&title, 1, XUTF8StringStyle,
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
		cleanupwindow(windows[0]);
		memmove(&windows[0], &windows[1], sizeof(SwtWindow *) * nwindows);
	} else if(w == nwindows - 1) {
		/* Last client. */
		nwindows--;
		cleanupwindow(windows[w]);
		windows = erealloc(windows, sizeof(SwtWindow *) * nwindows);
	} else {
		/* Somewhere inbetween. */
		cleanupwindow(windows[w]);
		memmove(&windows[w], &windows[w+1],
				sizeof(SwtWindow *) * (nwindows - (w + 1)));
		nwindows--;
	}
}

void
draw(SwtWindow *w) {
	writeout("drawing window xid=%lu name=%s title=%s width=%lu height=%lu\n",
			w->win, w->name, w->title, w->drw->w, w->drw->h);
	drw_setscheme(w->drw, &w->scheme[SchemeSel]);

	Bool invert = true;
	for (int i=0;i<w->nkids;i++) {
		XSetForeground(w->drw->dpy, w->drw->gc,
				invert ? w->drw->scheme->bg->rgb : w->drw->scheme->fg->rgb);
		XFillRectangle(w->drw->dpy, w->drw->drawable, w->drw->gc,
				w->kids[i]->r.x, w->kids[i]->r.y, w->kids[i]->r.w, w->kids[i]->r.h);
	}

	drw_map(w->drw, w->win, 0, 0, w->drw->w, w->drw->h);
}

void
dumptree(void) {
	for(int i=0; i<nwindows;i++) {
		dumpwindow(windows[i]);
	}
}

void
dumpwidget(SwtWidget *w) {
	writeout("dump %s name=\"%s\" w=%lu h=%lu\n", w->type == HorizBox ? "hbox" : "vbox",  w->name, w->r.w, w->r.h);
}

void
dumpwindow(SwtWindow *w) {
	writeout("dump window xid=%lu name=%s title=%s\n", w->win, w->name, w->title);
	for(int i=0; i<w->nkids;i++) {
		dumpwidget((SwtWidget *)w->kids[i]);
	}
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
expose(const XEvent *e) {
	const XExposeEvent *ev = &e->xexpose;
	int w;

	writeout("expose event\n");
	if(ev->count == 0) {
		w = getwindow(ev->window);
		draw(windows[w]);
	}
}

void
focusin(const XEvent *e) {
	const XFocusChangeEvent *ev = &e->xfocus;
	int dummy;
	Window focused;

	writeout("focusin event\n");
	if(ev->mode != NotifyUngrab) {
		XGetInputFocus(dpy, &focused, &dummy);
		sel = getwindow(focused);
	}
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

int
getwindowc(char *name) {
	for (int i=0;i<nwindows;i++) {
		if(strcmp(name, windows[i]->name) == 0) {
			return i;
		}
	}
	return -1;
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
	char *parent = NULL, *wtype = NULL, *wattrs = NULL;

	parent = attrs ? attrs : "swt";

	if(attrs && (wtype = strchr(attrs, ' '))) {
		*(wtype++) = '\0';
	}
	/* find parent widget/window or error and return */
	int w = getwindowc(parent);

	if(w < 0) {
		writeout("ERROR window/widget \"%s\" not found\n", parent);
		return;
	}

	if (wtype && (wattrs = strchr(wtype, ' '))) {
		*(wattrs++) = '\0';
	}

	if(strcasecmp("hbox", wtype) == 0) {
		addbox(windows[w], wattrs, True);
	} else if(strcasecmp("vbox", wtype) == 0) {
		addbox(windows[w], wattrs, False);
	} else {
		writeout("ERROR unknown widget type: %s\n", wtype);
	}
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
resize(SwtWindow *w) {
	for(int i=0;i<w->nkids;i++) {
		if(w->kids[i]->type == HorizBox) {
			w->kids[i]->r.x = bordersize;
			w->kids[i]->r.y = bordersize;
			w->kids[i]->r.w = w->drw->w - (bordersize*2);
			w->kids[i]->r.h = w->drw->h - (bordersize*2);
		}
	}
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

void
setup(void) {
	createfifo();
	createout();

	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
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
