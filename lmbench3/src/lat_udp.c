/*
 * udp_xact.c - simple UDP transaction latency test
 *
 * Three programs in one -
 *	server usage:	udp_xact -s
 *	client usage:	udp_xact hostname
 *	shutdown:	udp_xact -hostname
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";
#include "bench.h"

void	client_main(int ac, char **av);
void	server_main(int msize);
void	init(void *cookie);
void	cleanup(void *cookie);
void    doit(uint64 iter, void *cookie);

typedef struct _state {
	int	sock;
	int	seq;
	int	msize;
	char	*server;
	char	*buf;
} state_t;


int main(int ac, char **av)
{
	state_t state;
	int	c;
	int	parallel = 1;
	int	server = 0;
	int	shutdown = 0;
	int	msize = 4;
 	char	buf[256];
	char	*usage = "-s\n OR [-S] [-P <parallelism>] server\n NOTE: message size must be >= 4\n";

	if (sizeof(int) != 4) {
		fprintf(stderr, "lat_udp: Wrong sequence size\n");
		return(1);
	}

	while (( c = getopt(ac, av, "sSm:P:")) != EOF) {
		switch(c) {
		case 's': /* Server */
			server = 1;
			break;
		case 'S': /* shutdown serverhost */
		{
			int seq, n;
			int sock = udp_connect(av[optind],
					       UDP_XACT,
					       SOCKOPT_NONE);
			for (n = -1; n > -5; --n) {
				seq = htonl(n);
				(void) send(sock, &seq, sizeof(int), 0);
			}
			close(sock);
			exit (0);
		}
		case 'm':
			msize = atoi(optarg);
			if (msize < sizeof(int)) {
				lmbench_usage(ac, av, usage);
				msize = 4;
			}
			break;
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

	if (server) {
		if (fork() == 0) {
			server_main(msize);
		}
		exit(0);
	}

	if (optind + 1 != ac) {
		lmbench_usage(ac, av, usage);
	}

	state.server = av[optind];
	state.msize = msize;
	benchmp(init, doit, cleanup, SHORT, parallel, &state);
	sprintf(buf, "UDP latency using %s", state.server);
	micro(buf, get_n());
	exit(0);
}

void init(void * cookie)
{
	state_t *state = (state_t *) cookie;

	state->sock = udp_connect(state->server, UDP_XACT, SOCKOPT_NONE);
	state->seq = 0;
	state->buf = (char*)malloc(state->msize);
}

void doit(uint64 iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
	int seq = state->seq;
	int net = htonl(seq);
	int sock = state->sock;
	int ret;

	while (iterations-- > 0) {
		*(int*)state->buf = htonl(seq++);
		if (send(sock, state->buf, state->msize, 0) != state->msize) {
			perror("lat_udp client: send failed");
			exit(5);
		}
		if (recv(sock, state->buf, state->msize, 0) != state->msize) {
			perror("lat_udp client: recv failed");
			exit(5);
		}
	}
	state->seq = seq;
}

void cleanup(void * cookie)
{
	state_t *state = (state_t *) cookie;

	close(state->sock);
	free(state->buf);
}

void
server_main(int msize)
{
	char	*buf = (char*)valloc(msize);
	int     sock, sent, namelen, seq = 0;
	struct sockaddr_in it;

	GO_AWAY;

	sock = udp_server(UDP_XACT, SOCKOPT_NONE);

	while (1) {
		namelen = sizeof(it);
		if (recvfrom(sock, (void*)buf, msize, 0, 
		    (struct sockaddr*)&it, &namelen) < 0) {
			fprintf(stderr, "lat_udp server: recvfrom: got wrong size\n");
			exit(9);
		}
		sent = ntohl(*(int*)buf);
		if (sent < 0) {
			udp_done(UDP_XACT);
			exit(0);
		}
		if (sent != ++seq) {
			seq = sent;
		}
		*(int*)buf = htonl(seq);
		if (sendto(sock, (void*)buf, msize, 0, 
		    (struct sockaddr*)&it, sizeof(it)) < 0) {
			perror("lat_udp sendto");
			exit(9);
		}
	}
}
