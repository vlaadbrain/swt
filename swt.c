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

#include "arg.h"

#ifndef PIPE_BUF
#define PIPE_BUF 4096
#endif

#define PING_TIMEOUT 300

#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))

static void createfifo(void);
static void createout(void);
static void die(const char *errstr, ...);
static void parse(void);
static void resetfifo(void);
static void run(void);
static void usage(void);
static void writeout(const char *msg, ...);

char *argv0;

static int infd;
static FILE *outfile;
static char *in = NULL, *out = NULL;

static void
die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static void
writeout(const char *msg, ...) {
	va_list ap;

	va_start(ap, msg);
	vfprintf(outfile, msg, ap);
	va_end(ap);

	fflush(outfile);
}

static void
usage(void) {
	die("usage: %s [-v] -i <infifo> -o <outfile>\n", basename(argv0));
}

static void
createfifo(void) {
	if(access(in, F_OK) == -1)
		mkfifo(in, S_IRWXU);

	if((infd = open(in, O_RDONLY | O_NONBLOCK, 0)) == -1) {
		perror("unable to open input fifo");
		exit(EXIT_FAILURE);
	}
}

static void
resetfifo() {
	if(close(infd) == -1) {
		perror("unable to close fifo");
		exit(EXIT_FAILURE);
	} 

	createfifo();
}

static void
createout(void) {
	if(!(outfile = fopen(out, "a"))) {
		perror("unable to open output file");
		exit(EXIT_FAILURE);
	}
}

static void
parse(void) {
	int len = -1;
	char *input;
	char *cmd;
	char buf[PIPE_BUF];

	if((len = read(infd, buf, PIPE_BUF)) == -1) {
		perror("failed to read from pipe");
	}

	if(len == 0) {
		resetfifo();
		return;
	}

	for(input = buf;;input = NULL) {
		cmd = strtok(input, " ");
		if(cmd == NULL)
			break;
		if(!strncasecmp("NOOP", cmd, 4)) {
			writeout("NOOP\n");
			break;
		} else if(!strncasecmp("QUIT", cmd, 4)) {
			writeout("QUIT\n");
			break;
		} else if(!strncasecmp("DRAW", cmd, 4)) {
			writeout("DRAWN\n");
			break;
		} else if(!strncasecmp("DUMP", cmd, 4)) {
			writeout("CAN'T DUMP TREE YET\n");
			break;
		} else {
			die("ERROR unknown command: %s\n", cmd);
		}
		fflush(NULL);
	}
}


static void
run(void) {
	time_t last_response;

	last_response = time(NULL);

	for(;;) {
		int i, nfds = 0;
		fd_set rd;
		struct timeval tv = { .tv_sec = PING_TIMEOUT / 3, .tv_usec = 0 };

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
				parse();
			}
		}
	}
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

