/*
 * bw_pipe.c - pipe bandwidth benchmark.
 *
 * Usage: bw_pipe
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"

void	reader(int control[2], int pipes[2], int bytes);
void	writer(int control[2], int pipes[2]);

int	XFER	= 10*1024*1024;
int	pid;
char	*buf;

int
main()
{
	int	pipes[2];
	int	control[2];

	buf = valloc(XFERSIZE);
	touch(buf, XFERSIZE);
	if (pipe(pipes) == -1) {
		perror("pipe");
		return(1);
	}
	if (pipe(control) == -1) {
		perror("pipe");
		return(1);
	}
	switch (pid = fork()) {
	    case 0:
		writer(control, pipes);
		return(0);
		/*NOTREACHED*/
	    
	    case -1:
		perror("fork");
		return(1);
		/*NOTREACHED*/

	    default:
		break;
	}
	BENCH(reader(control, pipes, XFER), MEDIUM);
	fprintf(stderr, "Pipe bandwidth: ");
	mb(get_n() * XFER);
	kill(pid, 15);
	return(0);
}

void
writer(int control[2], int pipes[2])
{
	int	todo, n;

	for ( ;; ) {
		read(control[0], &todo, sizeof(todo));
		while (todo > 0) {
#ifdef	TOUCH
			touch(buf, XFERSIZE);
#endif
			n = write(pipes[1], buf, XFERSIZE);
			todo -= n;
		}
	}
}

void
reader(int control[2], int pipes[2], int bytes)
{
	int	todo = XFER, done = 0, n;

	write(control[1], &bytes, sizeof(bytes));
	while ((done < todo) && ((n = read(pipes[0], buf, XFERSIZE)) > 0)) {
		done += n;
	}
}
