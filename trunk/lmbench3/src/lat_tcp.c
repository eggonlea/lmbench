/*
 * lat_tcp.c - simple TCP transaction latency test
 *
 * Three programs in one -
 *	server usage:	tcp_xact -s
 *	client usage:	tcp_xact [-P <parallelism>] hostname
 *	shutdown:	tcp_xact -hostname
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
	int	sock;
	char	*server;
} state_t;

void	init(void *cookie);
void	cleanup(void *cookie);
void	doclient(uint64 iterations, void * cookie);
void	server_main();
void	doserver(int sock);

int
main(int ac, char **av)
{
	state_t state;
	int	parallel = 1;
	int 	c;
	char	buf[256];
	char	*usage = "-s\n OR [-P <parallelism>] server\n OR [-]serverhost\n";

	if (ac == 2 && !strcmp(av[1], "-s")) { /* Server */
		if (fork() == 0) {
			server_main();
		}
		exit(0);
	}

       /*
	* Client args are -server OR [-P <parallelism>] server
	*/
	if (ac == 2) {
		if (!strcmp(av[1],"-"))
			lmbench_usage(ac, av, usage);
		if (av[1][0] == '-') { /* shut down server */
			state.sock = tcp_connect(&av[1][1],
						 TCP_XACT,
						 SOCKOPT_NONE);
			close(state.sock);
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

	benchmp(init, doclient, cleanup, MEDIUM, parallel, &state);

	sprintf(buf, "TCP latency using %s", state.server);
	micro(buf, get_n());
}

void init(void * cookie)
{
	state_t *state = (state_t *) cookie;

	state->sock = tcp_connect(state->server, TCP_XACT, SOCKOPT_NONE);
}

void cleanup(void * cookie)
{
	state_t *state = (state_t *) cookie;

	close(state->sock);
}

void
doclient(uint64 iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
	int 	sock   = state->sock;
	char    c;

	while (iterations-- > 0) {
		write(sock, &c, 1);
		read(sock, &c, 1);
	}
}

void
child()
{
	wait(0);
	signal(SIGCHLD, child);
}

void
server_main()
{
	int     newsock, sock;

	GO_AWAY;
	signal(SIGCHLD, child);
	sock = tcp_server(TCP_XACT, SOCKOPT_NONE);
	for (;;) {
		newsock = tcp_accept(sock, SOCKOPT_NONE);
		switch (fork()) {
		    case -1:
			perror("fork");
			break;
		    case 0:
			doserver(newsock);
			exit(0);
		    default:
			close(newsock);
			break;
		}
	}
	/* NOTREACHED */
}

void
doserver(int sock)
{
	char    c;
	int	n = 0;

	while (read(sock, &c, 1) == 1) {
		write(sock, &c, 1);
		n++;
	}

	/*
	 * A connection with no data means shut down.
	 */
	if (n == 0) {
		tcp_done(TCP_XACT);
		kill(getppid(), SIGTERM);
		exit(0);
	}
}
