/*
 * bw_tcp.c - simple TCP bandwidth test
 *
 * Three programs in one -
 *	server usage:	bw_tcp -s
 *	client usage:	bw_tcp hostname
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

#define	MINMOVE	(10<<20)

int	server_main(int ac, char **av);
int	client_main(int ac, char **av);
void	source(int data);

char	*buf;

void
transfer(uint64 move, char *server)
{
	int	data, c;
	uint64	todo;

	todo = move;
	/*printf("Move %lu MB\n", (unsigned long)(move>>20));
	*/
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

/* ARGSUSED */
int
client_main(int ac, char **av)
{
	uint64	move;
	char   *server;
	uint64	usecs;

	if (ac != 2 && ac != 3) {
		(void)fprintf(stderr, "usage: %s remotehost [bytes]\n",
		    av[0]);
		exit(0);
	}
	if (!buf) {
		perror("valloc");
		exit(1);
	}
	if (ac == 3) {
		move = bytes(av[2]);
	} else {
		move = MINMOVE;
	}
	/*
	 * Disabler message to other side.
	 */
	if (av[1][0] == '-') {
		int	data;

		move = 0;
		server = &av[1][1];
		data = tcp_connect(server, TCP_DATA, SOCKOPT_NONE);
		write(data, "0", 1);
		exit(0);
	} else {
		server = av[1];
	}
	/*
	 * Make one run take at least 5 seconds.
	 * This minimizes the effect of connect & reopening TCP windows.
	 */
	start(0);
	transfer(move, server);
	usecs = stop(0,0);
	if (usecs >= LONGER) {	/* must be 10Mbit ether or sloooow box */
		save_n(1);
		goto out;
	}
	usecs = 5000000 / usecs;
	move *= usecs * 1.25;
	if (move < MINMOVE) move = MINMOVE;
	BENCH(transfer(move, server), LONGER);
out:	(void)fprintf(stderr, "Socket bandwidth using %s: ", server);
	mb(move * get_n());
	exit(0);
	/*NOTREACHED*/
}

void
child()
{
	wait(0);
	signal(SIGCHLD, child);
}

/* ARGSUSED */
int
server_main(int ac, char **av)
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
void
source(int data)
{
	int	n, nbytes;

	if (!buf) {
		perror("valloc");
		exit(1);
	}
	bzero((void*)buf, XFERSIZE);
	if (read(data, buf, XFERSIZE) <= 0) {
		perror("control nbytes");
		exit(7);
	}
	nbytes = atoi(buf);

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
		touch(buf, XFERSIZE);
#endif
		n = write(data, buf, XFERSIZE);
		if (n <= 0) break;
		nbytes -= n;
	}
}


int
main(int ac, char **av)
{
	if (ac != 2 && ac != 3) {
		fprintf(stderr, "Usage: %s -s OR %s [-]serverhost\n",
		    av[0], av[0]);
		exit(1);
	}
	buf = valloc(XFERSIZE);
	touch(buf, XFERSIZE);
	if (!strcmp(av[1], "-s")) {
		if (fork() == 0) {
			server_main(ac, av);
		}
		exit(0);
	} else {
		client_main(ac, av);
	}
	return(0);
}
