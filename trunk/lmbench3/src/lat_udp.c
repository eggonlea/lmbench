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
void	server_main();
void	init(void *cookie);
void	cleanup(void *cookie);
void    doit(uint64 iter, void *cookie);

typedef struct _state {
	int	sock;
	int	seq;
	char	*server;
} state_t;


void doit(uint64 iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
	int seq = state->seq;
	int net = htonl(seq);
	int sock = state->sock;
	int ret;


	while (iterations-- > 0) {
		seq++;
		if (send(sock, &net, sizeof(net), 0) != sizeof(net)) {
			perror("lat_udp client: send failed");
			exit(5);
		}
		if (recv(sock, &ret, sizeof(ret), 0) != sizeof(ret)) {
			perror("lat_udp client: recv failed");
			exit(5);
		}
	}
	state->seq = seq;
}

int main(int ac, char **av)
{
	state_t state;
	int parallel = 1;
 	char	buf[256];
	char	*usage = "-s\n OR [-P <parallelism>] server\n OR [-]serverhost\n";
	if (sizeof(int) != 4) {
		fprintf(stderr, "lat_udp: Wrong sequence size\n");
		return(1);
	}
	if (ac == 1)
		lmbench_usage(ac, av, usage);
	if (!strcmp(av[1], "-s")) {
		if (fork() == 0) {
			server_main();
		}
		return(0);
	} else { /*
		  * Client args are -server OR [-P <parallelism>] server
		  */
		int c;
		if (ac == 2) {
			if (!strcmp(av[1],"-"))
				lmbench_usage(ac, av, usage);
			if (av[1][0] == '-') { /* shut down server */
				int seq = -1;
				int sock = udp_connect(&av[1][1],
						       UDP_XACT,
						       SOCKOPT_NONE);
				while (seq-- > -5) {
					int	net = htonl(seq);
					(void) send(sock, &net,
						    sizeof(net), 0);
				}
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
	}

	benchmp(init, doit, cleanup,
		SHORT, parallel, &state);
	sprintf(buf, "UDP latency using %s", state.server);
	micro(buf, get_n());
}

void init(void * cookie)
{
	state_t *state = (state_t *) cookie;

	state->sock = udp_connect(state->server, UDP_XACT, SOCKOPT_NONE);
	state->seq = 0;
}

void cleanup(void * cookie)
{
	state_t *state = (state_t *) cookie;

	close(state->sock);
}

void
server_main()
{
	int     net, sock, sent, namelen, seq = 0;
	struct sockaddr_in it;

	GO_AWAY;

	sock = udp_server(UDP_XACT, SOCKOPT_NONE);

	while (1) {
		namelen = sizeof(it);
		if (recvfrom(sock, (void*)&sent, sizeof(sent), 0, 
		    (struct sockaddr*)&it, &namelen) < 0) {
			fprintf(stderr, "lat_udp server: recvfrom: got wrong size\n");
			exit(9);
		}
		sent = ntohl(sent);
		if (sent < 0) {
			udp_done(UDP_XACT);
			exit(0);
		}
		if (sent != ++seq) {
			seq = sent;
		}
		net = htonl(seq);
		if (sendto(sock, (void*)&net, sizeof(net), 0, 
		    (struct sockaddr*)&it, sizeof(it)) < 0) {
			perror("lat_udp sendto");
			exit(9);
		}
	}
}
