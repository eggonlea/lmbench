/*
 * bw_pipe.c - pipe bandwidth benchmark.
 *
 * Usage: bw_pipe [-P <parallelism>] [-W <warmup>] [-N <repetitions>]
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"

void	reader(uint64 iterations, void * cookie);
void	writer(int control[2], int pipes[2], char* buf);

int	XFER	= 10*1024*1024;

struct _state {
	int	pid;
	int	bytes;	/* bytes to read/write in one iteration */
	char	*buf;	/* buffer memory space */
	int	pipes[2];
	int	control[2];
	int	initerr;
};

void initialize(void *cookie)
{
	struct _state* state = (struct _state*)cookie;

	state->buf = valloc(XFERSIZE);
	touch(state->buf, XFERSIZE);
	state->initerr = 0;
	if (pipe(state->pipes) == -1) {
		perror("pipe");
		state->initerr = 1;
		return;
	}
	if (pipe(state->control) == -1) {
		perror("pipe");
		state->initerr = 2;
		return;
	}
	switch (state->pid = fork()) {
	    case 0:
		writer(state->control, state->pipes, state->buf);
		return;
		/*NOTREACHED*/
	    
	    case -1:
		perror("fork");
		state->initerr = 3;
		return;
		/*NOTREACHED*/

	    default:
		break;
	}
}

void cleanup(void * cookie)
{
	struct _state* state = (struct _state*)cookie;

	signal(SIGCHLD,SIG_IGN);
	kill(state->pid, 9);
}

void reader(uint64 iterations, void * cookie)
{
	struct _state* state = (struct _state*)cookie;
	int	done, n;
	int	todo = state->bytes;

	while (iterations-- > 0) {
		write(state->control[1], &todo, sizeof(todo));
		for (done = 0; done < todo; done += n) {
			if ((n = read(state->pipes[0], state->buf, XFERSIZE)) <= 0) {
				/* error! */
				break;
			}
		}
	}
}

void
writer(int control[2], int pipes[2], char* buf)
{
	int	todo, n;

	for ( ;; ) {
		read(control[0], &todo, sizeof(todo));
		while (todo > 0) {
#ifdef TOUCH
			touch(buf, XFERSIZE);
#endif
			n = write(pipes[1], buf, XFERSIZE);
			todo -= n;
		}
	}
}

int
main(int ac, char *av[])
{
	struct _state state;
	int parallel = 1;
	int warmup = 0;
	int repetitions = TRIES;
	int c;
	char* usage = "[-P <parallelism>] [-W <warmup>] [-N <repetitions>]\n";

	state.bytes = XFER;

	while (( c = getopt(ac, av, "P:W:N:")) != EOF) {
		switch(c) {
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0) lmbench_usage(ac, av, usage);
			break;
		case 'W':
			warmup = atoi(optarg);
			break;
		case 'N':
			repetitions = atoi(optarg);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}
	if (optind < ac) {
		lmbench_usage(ac, av, usage);
	}
	benchmp(initialize, reader, cleanup, MEDIUM, parallel, 
		warmup, repetitions, &state);

	fprintf(stderr, "Pipe bandwidth: ");
	mb(get_n() * parallel * XFER);
	return(0);
}
