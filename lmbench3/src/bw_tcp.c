/*
 * bw_tcp.c - simple TCP bandwidth test
 *
 * Three programs in one -
 *	server usage:	bw_tcp -s
 *	client usage:	bw_tcp [-m <message size>] [-P <parallelism>] [-W <warmup>] [-N <repetitions>] hostname [bytes]
 *	shutdown:	bw_tcp -hostname
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
	uint64	move;
	int	msize;
	char	*server;
	int	fd;
	char	*buf;
} state_t;

void	server_main();
void	client_main(int parallel, state_t *state);
void	source(int data);
void	transfer(state_t* state);

void	loop_transfer(uint64 iter, void *cookie);

int main(int ac, char **av)
{
	int	parallel = 1;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	server = 0;
	int	shutdown = 0;
	state_t state;
	char	*usage = "-s\n OR [-m <message size>] [-P <parallelism>] [-W <warmup>] [-N <repetitions>] server [size]\n OR -S serverhost\n";
	int	c;
	uint64	usecs;
	
	state.msize = 0;
	state.move = 10*1024*1024;

	/* Rest is client argument processing */
	while (( c = getopt(ac, av, "sSm:P:W:N:")) != EOF) {
		switch(c) {
		case 's': /* Server */
			server = 1;
			break;
		case 'S': /* shutdown serverhost */
			shutdown = 1;
			break;
		case 'm':
			state.msize = atoi(optarg);
			break;
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

	if (server && shutdown
	    || !server && (optind < ac - 2 || optind >= ac)) {
		lmbench_usage(ac, av, usage);
	}

	if (server) {
		if (fork() == 0) {
			server_main();
		}
		exit(0);
	}

	state.server = av[optind++];
	if (optind < ac) {
		state.move = bytes(av[optind]);
	}
	if (state.msize == 0) {
		state.msize = state.move;
	}
	/* make the number of bytes to move a multiple of the message size */
	if (state.move % state.msize) {
		state.move += state.move - state.move % state.msize;
	}

	if (shutdown) {
		int	conn;
		int	n = htonl(0);
		conn = tcp_connect(state.server, TCP_DATA, SOCKOPT_NONE);
		write(conn, &n, sizeof(int));
		exit(0);
	}

	state.buf = valloc(state.msize);
	if (!state.buf) {
		perror("valloc");
		exit(1);
	}
	touch(state.buf, state.msize);

	/*
	 * Make one run take at least 5 seconds.
	 * This minimizes the effect of connect & reopening TCP windows.
	 */
	benchmp(NULL, loop_transfer, NULL, LONGER, parallel, warmup, repetitions, &state );

out:	(void)fprintf(stderr, "Socket bandwidth using %s: ", state.server);
	mb(state.move * get_n() * parallel);
}

void loop_transfer(uint64 iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
	uint64	move = state->move;
	char	*server = state->server;

	while (iterations-- > 0) {
		transfer(state);
	}
}

void transfer(state_t* state)
{
	int	data, c;
	uint64	todo;
	char	*buf    = state->buf;

	/*printf("Move %lu MB\n", (unsigned long)(move>>20)); */
	
	data = tcp_connect(state->server, TCP_DATA, SOCKOPT_READ);
	c = htonl(state->move);
	if (write(data, &c, sizeof(int)) != sizeof(int)) {
		perror("control write");
		exit(1);
	}
	c = htonl(state->msize);
	if (write(data, &c, sizeof(int)) != sizeof(int)) {
		perror("control write");
		exit(1);
	}
	for (todo = state->move; todo > 0; todo -= c) {
		if (c = read(data, state->buf, state->msize) <= 0) {
			break;
		}
	}
	(void)close(data);
}

void child()
{
	wait(0);
	signal(SIGCHLD, child);
}

void server_main()
{
	int	data, newdata;

	GO_AWAY;

	signal(SIGCHLD, child);
	data = tcp_server(TCP_DATA, SOCKOPT_WRITE);

	for ( ;; ) {
		newdata = tcp_accept(data, SOCKOPT_WRITE);
		switch (fork()) {
		    case -1:
			perror("fork");
			break;
		    case 0:
			source(newdata);
			exit(0);
		    default:
			close(newdata);
			break;
		}
	}
}

/*
 * Read the number of bytes to be transfered.
 * Write that many bytes on the data socket.
 */
void source(int data)
{
	int	tmp, n, m, nbytes;
	char	*buf;

	if (read(data, &tmp, sizeof(int)) != sizeof(int)) {
		perror("control nbytes");
		exit(7);
	}
	nbytes = ntohl(tmp);

	/*
	 * A hack to allow turning off the absorb daemon.
	 */
     	if (nbytes == 0) {
		tcp_done(TCP_DATA);
		kill(getppid(), SIGTERM);
		exit(0);
	}

	/*
	 * Now read the message size
	 */
	if (read(data, &tmp, sizeof(int)) != sizeof(int)) {
		perror("control msize");
		exit(7);
	}
	m = ntohl(tmp);
	buf = valloc(m);
	bzero(buf, m);

	while (nbytes > 0) {
#ifdef	TOUCH
		touch(buf, m);
#endif
		n = write(data, buf, m);
		if (n <= 0) {
			perror("data write");
			exit(1);
		}
		nbytes -= n;
	}
	free(buf);
}
