/*
 * lat_pipe.c - pipe transaction test
 *
 * usage: lat_pipe [-P <parallelism>]
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"

void initialize(void *cookie);
void cleanup(void *cookie);
void doit(uint64 iterations,void *cookie);
void writer(int w, int r);

typedef struct _state {
	int	pid;
	int	p1[2];
	int	p2[2];
} state_t;

int main(int ac, char **av)
{
	state_t state;
	int parallel = 1;
	int c;
	char* usage = "[-P <parallelism>]\n";

	while (( c = getopt(ac, av, "P:")) != EOF) {
		switch(c) {
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0) lmbench_usage(ac, av, usage);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}
	if (optind < ac) {
		lmbench_usage(ac, av, usage);
	}

	benchmp(initialize, doit, cleanup, SHORT, parallel, &state);
	micro("Pipe latency", get_n());
}

void initialize(void *cookie)
{
	char	c;
	state_t * state = (state_t *)cookie;

	if (pipe(state->p1) == -1) {
		perror("pipe");
		exit(1);
	}
	if (pipe(state->p2) == -1) {
		perror("pipe");
		exit(1);
	}
	switch (state->pid = fork()) {
	    case 0:
		writer(state->p2[1], state->p1[0]);
		return;

	    case -1:
		perror("fork");
		return;

	    default:
		break;
	}

	/*
	 * One time around to make sure both processes are started.
	 */
	if (write(state->p1[1], &c, 1) != 1 ||read(state->p2[0], &c, 1) != 1) {
		perror("(i) read/write on pipe");
		exit(1);
	}
}

void cleanup(void * cookie)
{
	state_t * state = (state_t *)cookie;

	kill(state->pid, 15);
	signal(SIGCHLD,SIG_IGN);
	kill(state->pid, 9);
}

void doit(register uint64 iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
	char		c;
	register int	w = state->p1[1];
	register int	r = state->p2[0];
	register char	*cptr = &c;

	while (iterations-- > 0) {
		if (write(w, cptr, 1) != 1 ||
		    read(r, cptr, 1) != 1) {
			perror("(r) read/write on pipe");
			exit(1);
		}
	}
}

void writer(register int w, register int r)
{
	char		c;
	register char	*cptr = &c;

	for ( ;; ) {
		if (read(r, cptr, 1) != 1 ||
			write(w, cptr, 1) != 1) {
			    perror("(w) read/write on pipe");
		}
	}
}
