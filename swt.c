/*
 * See LICENSE file for copyright and license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <libgen.h>

#include "arg.h"

#ifndef PIPE_BUF /* FreeBSD don't know PIPE_BUF */
#define PIPE_BUF 4096
#endif

char *argv0;
static char buf[PIPE_BUF];

void
die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void
usage(void)
{
	die("usage: %s [-v]\n", basename(argv0));
}

int
main(int argc, char *argv[]) {
	int i;
	struct timeval tv;
	fd_set rd;

	ARGBEGIN {
	case 'v':
		die("swt-"VERSION", © 2013 swt engineers"
				", see LICENSE for details.\n");
	default:
		usage();
	} ARGEND;

	for(;;) {
		FD_ZERO(&rd);
		FD_SET(0, &rd);
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		i = select(1, &rd, NULL, NULL, &tv);
		if (i < 0) {
			perror("select()");
			exit(EXIT_FAILURE);
		}
		if (FD_ISSET(0, &rd)) {
		}
	}

	return 0;
}

