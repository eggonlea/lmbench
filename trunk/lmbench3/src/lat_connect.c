/*
 * lat_connect.c - simple TCP connection latency test
 *
 * Three programs in one -
 *	server usage:	lat_connect -s
 *	client usage:	lat_connect [-P <parallelism> hostname
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

void	doclient(uint64 iterations, void * cookie);
void	server_main();

int main(int ac, char **av)
{
	state_t state;
	int	parallel = 1;
	char 	c;
	char	buf[256];
	char	*usage = "-s\n OR [-P <parallelism>] server\n OR [-]serverhost\n";

	if (ac == 2 && !strcmp(av[1], "-s")) { /* Server */
		if (fork() == 0) {
			server_main();
		}
		exit(0);
	}
       /*
	* Client
	*/

	if (ac == 2) {
		if (!strcmp(av[1],"-"))
			lmbench_usage(ac, av, usage);
		if (av[1][0] == '-') { /* shut down server */
			int sock = tcp_connect(&av[1][1],
					       TCP_CONNECT,
					       SOCKOPT_NONE);
			write(sock, "0", 1);
			close(sock);
			exit(0);
		}
	}

	while (( c = getopt(ac, av, "P:")) != EOF) {
		switch(c) {
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0)
				lmbench_usage(ac, av, usage);
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

	benchmp(NULL, doclient, NULL,
		REAL_SHORT, parallel, &state);

	sprintf(buf, "TCP/IP connection cost to %s", state.server);
	micro(buf, get_n());
}

void doclient(uint64 iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
	register char	*server = state->server;
	register int 	sock;
	
	while (iterations-- > 0) {
		sock = tcp_connect(server, TCP_CONNECT, SOCKOPT_NONE);
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
		read(newsock, &c, 1);
		if (c == '0') {
			tcp_done(TCP_CONNECT);
			exit(0);
		}
		close(newsock);
	}
	/* NOTREACHED */
}
