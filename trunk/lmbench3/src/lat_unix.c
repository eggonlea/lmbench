/*
 * tcp_xact.c - simple TCP transaction latency test
 *
 * Three programs in one -
 *	server usage:	tcp_xact -s
 *	client usage:	tcp_xact hostname
 *	shutwn:	tcp_xact -hostname
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";
#include "bench.h"

struct _state {
	int	sv[2];
	int	pid;
};
void	initialize(void* cookie);
void	benchmark(uint64 iterations, void* cookie);
void	cleanup(void* cookie);

int
main(int ac, char **av)
{
	struct _state state;

	benchmp(initialize, benchmark, cleanup, 0, 1, &state);
	micro("AF_UNIX sock stream latency", get_n());
	return(0);
}

void initialize(void* cookie)
{
	struct _state* pState = (struct _state*)cookie;
	char    c;
	void	exit();

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pState->sv) == -1) {
		perror("socketpair");
	}

	if (pState->pid = fork())
		return;

	/* Child sits and ping-pongs packets back to parent */
	signal(SIGTERM, exit);
	while (read(pState->sv[0], &c, 1) == 1) {
		write(pState->sv[0], &c, 1);
	}
	exit(0);
}

void benchmark(uint64 iterations, void* cookie)
{
	struct _state* pState = (struct _state*)cookie;

	while (iterations-- > 0) {
		char    c;

		if (write(pState->sv[1], &c, 1) != 1
		    || read(pState->sv[1], &c, 1) != 1) {
			/* error handling: how do we signal failure? */
			cleanup(cookie);
			exit(0);
		}
	}
}

void cleanup(void* cookie)
{
	struct _state* pState = (struct _state*)cookie;

	kill(pState->pid, SIGTERM);
	wait(0);
}

