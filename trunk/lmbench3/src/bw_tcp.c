/*
 * bw_tcp.c - simple TCP bandwidth test
 *
 * Three programs in one -
 *	server usage:	bw_tcp -s
 *	client usage:	bw_tcp [-P <parallelism>] hostname [bytes]
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
	char	server[256];
	int	fd;
	char	*buf;
} state_t;

void	server_main(state_t *state);
void	client_main(int parallel, state_t *state);
void	source(int data, state_t *state);
void	transfer(uint64 move, char *server, char *buf);

void	loop_transfer(uint64 iter, void *cookie);

int main(int ac, char **av)
{
	int parallel = 1;
	state_t state;
	char	*usage = "-s\n OR [-P <parallelism>] server [size]\n OR [-]serverhost\n";
	char	*server;
	int	c;
	uint64	usecs;
	
	state.buf = valloc(XFERSIZE);
	touch(state.buf, XFERSIZE);

	/* Start the server */
	if (!strcmp(av[1], "-s")) {
		if (fork() == 0) {
			server_main(&state);
		}
		exit(0);
	}

	/* Shut down the server */
	if ((ac == 2) && (av[1][0] == '-')) {
		int	conn;
		server = &av[1][1];
		conn = tcp_connect(server, TCP_DATA, SOCKOPT_NONE);
		write(conn, "0", 1);
		exit(0);
	}
	
		
	/* Rest is client argument processing */
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

	/*
	 * Client should have one, possibly two [indicates size] arguments left
	 */
	state.move = 10*1024*1024;
	if (optind + 2 == ac) {
		state.move = bytes(av[optind+1]);
		strcpy(state.server,av[optind]);
	} else if (optind + 1 == ac) {
		strcpy(state.server,av[optind]);
	} else {
		lmbench_usage(ac, av, usage);
	}

	if (!state.buf) {
		perror("valloc");
		exit(1);
	}

	/*
	 * Make one run take at least 5 seconds.
	 * This minimizes the effect of connect & reopening TCP windows.
	 */
	start(0);
	transfer(state.move, state.server, state.buf);
	usecs = stop(0,0);
	if (usecs >= LONGER) {	/* must be 10Mbit ether or sloooow box */
		save_n(1);
		if (parallel == 1)
			goto out; /* can't skip next step for MP tests! */
	}
	usecs = 5000000 / usecs;
	state.move  *= usecs * 1.25;

	benchmp(NULL, loop_transfer, NULL,
		LONGER, parallel, &state );

out:	(void)fprintf(stderr, "Socket bandwidth using %s: ", state.server);
	mb(state.move * get_n() * parallel);
}

void loop_transfer(uint64 iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
	uint64	move = state->move;
	char	*server = state->server;
	char	*buf    = state->buf;

	while (iterations-- > 0) {
		transfer(move,server,buf);
	}
}

void transfer(uint64 move, char *server, char *buf)
{
	int	data, c;
	uint64	todo;

	todo = move;
	/*printf("Move %lu MB\n", (unsigned long)(move>>20)); */
	
	data = tcp_connect(server, TCP_DATA, SOCKOPT_READ);
	(void)sprintf(buf, "%lu", (unsigned long)move);
	if (write(data, buf, strlen(buf)) != strlen(buf)) {
		perror("control write");
		exit(1);
	}
	while (todo > 0 && (c = read(data, buf, XFERSIZE)) > 0) {
		todo -= c;
	}
	(void)close(data);
}

void child()
{
	wait(0);
	signal(SIGCHLD, child);
}

void server_main(state_t *state)
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
			source(newdata, state);
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
void source(int data, state_t *state)
{
	int	n, nbytes;

	if (!state->buf) {
		perror("valloc");
		exit(1);
	}
	bzero((void*)state->buf, XFERSIZE);
	if (read(data, state->buf, XFERSIZE) <= 0) {
		perror("control nbytes");
		exit(7);
	}
	nbytes = atoi(state->buf);

	/*
	 * A hack to allow turning off the absorb daemon.
	 */
     	if (nbytes == 0) {
		tcp_done(TCP_DATA);
		kill(getppid(), SIGTERM);
		exit(0);
	}
	while (nbytes > 0) {
#ifdef	TOUCH
		touch(state->buf, XFERSIZE);
#endif
		n = write(data, state->buf, XFERSIZE);
		if (n <= 0) break;
		nbytes -= n;
	}
}
