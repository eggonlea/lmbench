/*
 * lat_syscall.c - time simple system calls
 *
 * Copyright (c) 1996 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 */
char	*id = "$Id: s.lat_syscall.c 1.11 97/06/15 22:38:58-07:00 lm $\n";

#include "bench.h"
#define	FNAME "/usr/include/sys/types.h"

struct _state {
	int fd;
	char* file;
};

void
do_getppid(uint64 iterations, void *cookie)
{
	struct _state *pState = (struct _state*)cookie;
	char	c;

	for (; iterations > 0; --iterations) {
		getppid();
	}
}

void
do_write(uint64 iterations, void *cookie)
{
	struct _state *pState = (struct _state*)cookie;
	char	c;

	for (; iterations > 0; --iterations) {
		if (write(pState->fd, &c, 1) != 1) {
			perror("/dev/null");
			return;
		}
	}
}

void
do_read(uint64 iterations, void *cookie)
{
	struct _state *pState = (struct _state*)cookie;
	char	c;

	for (; iterations > 0; --iterations) {
		if (read(pState->fd, &c, 1) != 1) {
			perror("/dev/zero");
			return;
		}
	}
}

void
do_stat(uint64 iterations, void *cookie)
{
	struct _state *pState = (struct _state*)cookie;
	struct	stat sbuf;

	for (; iterations > 0; --iterations) {
		if (stat(pState->file, &sbuf) == -1) {
			perror(pState->file);
			return;
		}
	}
}

void
do_fstat(uint64 iterations, void *cookie)
{
	struct _state *pState = (struct _state*)cookie;
	struct	stat sbuf;

	for (; iterations > 0; --iterations) {
		if (fstat(pState->fd, &sbuf) == -1) {
			perror("fstat");
			return;
		}
	}
}

void
do_openclose(uint64 iterations, void *cookie)
{
	struct _state *pState = (struct _state*)cookie;
	int	fd;

	for (; iterations > 0; --iterations) {
		fd = open(pState->file, 0);
		if (fd == -1) {
			perror(pState->file);
			return;
		}
		close(fd);
	}
}

int
main(int ac, char **av)
{
	struct _state state;

	if (ac < 2) goto usage;
	state.file = av[2] ? av[2] : FNAME;

	if (!strcmp("null", av[1])) {
		benchmp(NULL, do_getppid, NULL, 0, 1, &state);
		micro("Simple syscall", get_n());
	} else if (!strcmp("write", av[1])) {
		state.fd = open("/dev/null", 1);
		benchmp(NULL, do_write, NULL, 0, 1, &state);
		micro("Simple write", get_n());
		close(state.fd);
	} else if (!strcmp("read", av[1])) {
		state.fd = open("/dev/zero", 0);
		if (state.fd == -1) {
			fprintf(stderr, "Read from /dev/zero: -1");
			return(1);
		}
		benchmp(NULL, do_read, NULL, 0, 1, &state);
		micro("Simple read", get_n());
		close(state.fd);
	} else if (!strcmp("stat", av[1])) {
		benchmp(NULL, do_stat, NULL, 0, 1, &state);
		micro("Simple stat", get_n());
	} else if (!strcmp("fstat", av[1])) {
		state.fd = open(state.file, 0);
		benchmp(NULL, do_fstat, NULL, 0, 1, &state);
		micro("Simple fstat", get_n());
		close(state.fd);
	} else if (!strcmp("open", av[1])) {
		benchmp(NULL, do_openclose, NULL, 0, 1, &state);
		micro("Simple open/close", get_n());
	} else {
usage:		printf("Usage: %s null|read|write|stat|open\n", av[0]);
	}
	return(0);
}
