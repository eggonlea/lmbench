/*
 * lat_rpc.c - simple RPC transaction latency test
 *
 * Four programs in one -
 *	server usage:	lat_rpc -s
 *	client usage:	lat_rpc hostname
 *	client usage:	lat_rpc hostname tcp
 *	shutdown:	lat_rpc -hostname
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
void	server_main(void);
char	*client_rpc_xact_1(char *argp, CLIENT *clnt);

void
doit(CLIENT *cl, char *server)
{
	char	c = 1;
	char	*resp;
	
	resp = client_rpc_xact_1(&c, cl);
	if (!resp) {
		clnt_perror(cl, server);
		exit(1);
	}
	if (*resp != 123) {
		fprintf(stderr, "lat_rpc: got bad data\n");
		exit(1);
	}
}

int
main(int ac, char **av)
{
	if (ac != 2 && ac != 3) {
		fprintf(stderr, "Usage: %s -s OR %s [-]serverhost [proto]\n",
		    av[0], av[0]);
		exit(1);
	}
	if (!strcmp(av[1], "-s")) {
		if (fork() == 0) {
			server_main();
		}
		exit(0);
	} else {
		client_main(ac, av);
	}
	return(0);
}

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 0, 2500 };

char	*proto[] = { "tcp", "udp", 0 };

void
client_main(int ac, char **av)
{
	CLIENT *cl;
	struct	timeval tv;
	char	*server;
	char	buf[256];
	int	i;

	if (ac != 2 && ac != 3) {
		fprintf(stderr, "Usage: %s hostname [protocol]\n", av[0]);
		exit(1);
	}
	server = av[1][0] == '-' ? &av[1][1] : av[1];
	for (i = 0; i < 2; ++i) {
		if (!(cl =
		    clnt_create(server, XACT_PROG, XACT_VERS, proto[i]))) {
			clnt_pcreateerror(server);
			exit(1);
		}
		if (av[1][0] == '-') {
done:			clnt_call(cl, RPC_EXIT, (xdrproc_t)xdr_void, 0, 
			    (xdrproc_t)xdr_void, 0, TIMEOUT);
			exit(0);
		}
		if (i == 1) {
			tv.tv_sec = 0;
			tv.tv_usec = 2500;
			if (!clnt_control(cl,
			    CLSET_RETRY_TIMEOUT, (char *)&tv)) {
				clnt_perror(cl, "setting timeout");
				exit(1);
			}
		}
		BENCH(doit(cl, server), MEDIUM);
		sprintf(buf, "RPC/%s latency using %s", proto[i], server);
		micro(buf, get_n());

	}
	goto done;
	/* NOTREACHED */
}

char *
client_rpc_xact_1(char *argp, CLIENT *clnt)
{
	static char res;

	bzero((void*)&res, sizeof(res));
	if (clnt_call(clnt, RPC_XACT, (xdrproc_t)xdr_char,
	    argp, (xdrproc_t)xdr_char, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}

/*
 * The remote procedure[s] that will be called
 */
/* ARGSUSED */
char	*
rpc_xact_1(msg, transp)
     	char	*msg;
	register SVCXPRT *transp;
{
	static char r = 123;

	return &r;
}

static void xact_prog_1();
bool_t	pmap_set(u_long prognum, u_long versnum, u_long protocol, u_short port);
bool_t	pmap_unset(u_long prognum, u_long versnum);

void
server_main(void)
{
	register SVCXPRT *transp;

	GO_AWAY;

	(void) pmap_unset(XACT_PROG, XACT_VERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		fprintf(stderr, "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, XACT_PROG, XACT_VERS, xact_prog_1, IPPROTO_UDP)) {
		fprintf(stderr, "unable to register (XACT_PROG, XACT_VERS, udp).");
		exit(1);
	}

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		fprintf(stderr, "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, XACT_PROG, XACT_VERS, xact_prog_1, IPPROTO_TCP)) {
		fprintf(stderr, "unable to register (XACT_PROG, XACT_VERS, tcp).");
		exit(1);
	}

	svc_run();
	fprintf(stderr, "svc_run returned");
	exit(1);
	/* NOTREACHED */
}

static void
xact_prog_1(rqstp, transp)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	union {
		char rpc_xact_1_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, (xdrproc_t)xdr_void, (char *)NULL);
		return;

	case RPC_XACT:
		xdr_argument = xdr_char;
		xdr_result = xdr_char;
		local = (char *(*)()) rpc_xact_1;
		break;

	case RPC_EXIT:
		(void) svc_sendreply(transp, (xdrproc_t)xdr_void, (char *)NULL);
		(void) pmap_unset(XACT_PROG, XACT_VERS);
		exit(0);

	default:
		svcerr_noproc(transp);
		return;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, (void *)xdr_argument, (char*)&argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, (xdrproc_t)xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, (void*)xdr_argument, (char*)&argument)) {
		fprintf(stderr, "unable to free arguments");
		exit(1);
	}
	return;
}
