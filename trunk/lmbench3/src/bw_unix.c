/*
 * bw_unix.c - simple Unix stream socket bandwidth test
 *
 * Usage: bw_unix [-P <parallelism>]
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
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, state->pipes) == -1) {
		perror("socketpair");
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
main(int argc, char *argv[])
{
	struct _state state;
	int parallel = 1;
	int c;
	char* usage = "[-P <parallelism>]\n";

	state.bytes = XFER;

	while (( c = getopt(argc,argv,"P:")) != EOF) {
		switch(c) {
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0) lmbench_usage(argc, argv, usage);
			break;
		default:
			lmbench_usage(argc, argv);
			break;
		}
	}
	if (optind < argc) {
		lmbench_usage(argc, argv);
	}
	benchmp(initialize, reader, cleanup, MEDIUM, parallel, &state);

	fprintf(stderr, "AF_UNIX sock stream bandwidth: ");
	mb(get_n() * parallel * XFER);
	return(0);
}



