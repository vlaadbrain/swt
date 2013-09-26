/*
 * See LICENSE file for copyright and license details.
 */

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

#include "drw.h"
#include "util.h"
#include "arg.h"

#ifndef PIPE_BUF
#define PIPE_BUF 4096
#endif

#define PING_TIMEOUT 300

static void closefifo(void);
static void createfifo(void);
static void createout(void);
static void dumptree(void);
static void noop(void);
static void procinput(void);
static void procadd(char *attrs);
static void procremove(char *attrs);
static void procshow(char *attrs);
static void procwindow(char *attrs);
static void resetfifo(void);
static void run(void);
static void usage(void);
static void quit(void);
static void writeout(const char *msg, ...);

char *argv0;

static int infd = -1;
static int winfd = -1; /* don't write here, it prevents EOF */
static FILE *outfile;
static char *in = NULL, *out = NULL;

void
closefifo(void) {
	if(close(winfd) == -1) {
		perror("unable to close O_WRONLY fifo");
		exit(EXIT_FAILURE);
	}

	if(close(infd) == -1) {
		perror("unable to close O_RDONLY fifo");
		exit(EXIT_FAILURE);
	}
}

void
createfifo(void) {
	if(access(in, F_OK) == -1)
		mkfifo(in, S_IRWXU);

	if((infd = open(in, O_RDONLY | O_NONBLOCK, 0)) == -1) {
		perror("unable to open input fifo for reading");
		quit();
	}
	if((winfd = open(in, O_WRONLY, 0)) == -1) {
		perror("unable to open input fifo for writting");
		quit();
	}
}

void
createout(void) {
	if(!(outfile = fopen(out, "a"))) {
		perror("unable to open output file");
		exit(EXIT_FAILURE);
	}
}

void
dumptree(void) {
	writeout("CAN'T DUMP TREE YET\n");
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
		perror("failed to read from pipe");
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
				quit();
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
	writeout("window request processing: %s\n", attrs);
}

void
quit(void) {
	closefifo();

	writeout("done\n");

	if(fclose(outfile) == -1) {
		perror("unable to close outfile");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
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

	for(;;) {
		int i, nfds = 0;
		fd_set rd;
		struct timeval tv = { .tv_sec = PING_TIMEOUT / 5, .tv_usec = 0 };

		FD_ZERO(&rd);
		FD_SET(infd, &rd);
		nfds = MAX(nfds, infd);

		i = select(nfds + 1, &rd, NULL, NULL, &tv);

		if(i == -1 && errno == EINTR) 
			continue;

		if(i == -1) {
			perror("swt: error on select()");
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
		}
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

	createfifo();
	createout();

	run();

	return 0;
}

