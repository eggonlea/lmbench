/*
 * lat_connect.c - simple TCP connection latency test
 *
 * Three programs in one -
 *	server usage:	lat_connect -s
 *	client usage:	lat_connect [-P <parallelism>] [-W <warmup>] [-N <repetitions>] hostname
 *	shutdown:	lat_connect -hostname
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";
#include "bench.h"

typedef struct _state {
	char	*server;
} state_t;

void	doclient(iter_t iterations, void * cookie);
void	server_main();

int main(int ac, char **av)
{
	state_t state;
	int	parallel = 1;
	int	warmup = 0;
	int	repetitions = TRIES;
	int 	c;
	char	buf[256];
	char	*usage = "-s\n OR [-S] [-P <parallelism>] [-W <warmup>] [-N <repetitions>] server\n";

	while (( c = getopt(ac, av, "sSP:W:N:")) != EOF) {
		switch(c) {
		case 's': /* Server */
			if (fork() == 0) {
				server_main();
			}
			exit(0);
		case 'S': /* shutdown serverhost */
		{
			int sock = tcp_connect(av[optind],
					       TCP_CONNECT,
					       SOCKOPT_NONE);
			write(sock, "0", 1);
			close(sock);
			exit(0);
		}
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0)
				lmbench_usage(ac, av, usage);
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

	if (optind + 1 != ac) {
		lmbench_usage(ac, av, usage);
	}

	state.server = av[optind];
	benchmp(NULL, doclient, NULL, REAL_SHORT, parallel, 
		warmup, repetitions, &state);

	sprintf(buf, "TCP/IP connection cost to %s", state.server);
	micro(buf, get_n());
	exit(0);
}

void doclient(iter_t iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
	register char	*server = state->server;
	register int 	sock;
	
	while (iterations-- > 0) {
		sock = tcp_connect(server, TCP_CONNECT, SOCKOPT_REUSE);
		close(sock);
	}
}

void
server_main()
{
	int     newsock, sock;
	char	c ='1';

	GO_AWAY;
	sock = tcp_server(TCP_CONNECT, SOCKOPT_NONE);
	for (;;) {
		newsock = tcp_accept(sock, SOCKOPT_NONE);
		if (read(newsock, &c, 1) > 0) {
			tcp_done(TCP_CONNECT);
			exit(0);
		}
		close(newsock);
	}
	/* NOTREACHED */
}
