/*
 * lat_ctx.c - context switch timer 
 *
 * usage: lat_ctx [-P parallelism] [-W <warmup>] [-N <repetitions>] [-s size] #procs [#procs....]
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"

#if	defined(sgi) && defined(PIN)
#include <sys/sysmp.h>
#include <sys/syssgi.h>
int	ncpus;
#endif

#define	MAXPROC	2048
#define	CHUNK	(4<<10)
#define	TRIPS	5
#ifndef	max
#define	max(a, b)	((a) > (b) ? (a) : (b))
#endif

int	sumit(int*, int);
void	doit(int **p, int rd, int wr, int process_size);
int	create_pipes(int **p, int procs);
int	create_daemons(int **p, int pids[], int procs, int process_size);
void	initialize_overhead(void* cookie);
void	cleanup_overhead(void* cookie);
void	benchmark_overhead(iter_t iterations, void* cookie);
void	initialize(void* cookie);
void	cleanup(void* cookie);
void	benchmark(iter_t iterations, void* cookie);

struct _state {
	int	process_size;
	double	overhead;
	int	procs;
	pid_t*	pids;
	int	**p;
	int*	data;
};

struct _state* pGlobalState;
static int sigterm_cleanup = 0;
static int sigterm_received = 0;

void
sigterm_handler(int n)
{
	sigterm_received = 1;
	if (sigterm_cleanup) {
		cleanup(pGlobalState);
		exit(0);
	}
}

int
main(int ac, char **av)
{
	int	i, maxprocs;
	int	c;
	int	parallel = 1;
	int	warmup = 0;
	int	repetitions = TRIES;
	struct _state state;
	char *usage = "[-P <parallelism>] [-W <warmup>] [-N <repetitions>] [-s kbytes] processes [processes ...]\n";
	double	time;

	/*
	 * Need 4 byte ints.
	 */
	if (sizeof(int) != 4) {
		fprintf(stderr, "Fix sumit() in ctx.c.\n");
		exit(1);
	}

	state.process_size = 0;
	state.overhead = 0.0;
	state.pids = NULL;

	/*
	 * If they specified a context size, or parallelism level, get them.
	 */
	while (( c = getopt(ac, av, "s:P:W:N:")) != EOF) {
		switch(c) {
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
		case 's':
			state.process_size = atoi(optarg) * 1024;
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}

	if (optind > ac - 1)
		lmbench_usage(ac, av, usage);

#if	defined(sgi) && defined(PIN)
	ncpus = sysmp(MP_NPROCS);
	sysmp(MP_MUSTRUN, 0);
#endif

	pGlobalState = &state;

	/* compute pipe + sumit overhead */
	maxprocs = atoi(av[optind]);
	for (i = optind; i < ac; ++i) {
		state.procs = atoi(av[i]);
		if (state.procs > maxprocs)
			maxprocs = state.procs;
	}
	state.procs = maxprocs;
	benchmp(initialize_overhead, benchmark_overhead, cleanup_overhead, 
		0, 1, warmup, repetitions, &state);
	if (gettime() == 0) return(0);
	state.overhead = gettime();
	state.overhead /= get_n();
	fprintf(stderr, "\n\"size=%dk ovr=%.2f\n", 
		state.process_size/1024, state.overhead);

	/* compute the context switch cost for N processes */
	for (i = optind; i < ac; ++i) {
		state.procs = atoi(av[i]);
		benchmp(initialize, benchmark, cleanup, 0, parallel, 
			warmup, repetitions, &state);

		time = gettime();
		time /= get_n();
		time /= state.procs;
		time -= state.overhead;

		if (time > 0.0)
			fprintf(stderr, "%d %.2f\n", state.procs, time);
	}

	return (0);
}

void
initialize_overhead(void* cookie)
{
	int i;
	int procs;
	int* p;
	struct _state* pState = (struct _state*)cookie;

	pState->pids = NULL;
	signal(SIGTERM, sigterm_handler);
	pState->p = (int**)malloc(pState->procs * (sizeof(int*) + 2 * sizeof(int)));
	p = (int*)&pState->p[pState->procs];
	for (i = 0; i < pState->procs; ++i) {
		pState->p[i] = p;
		p += 2;
	}

	pState->data = (pState->process_size > 0) ? malloc(pState->process_size) : NULL;
	if (pState->data)
		bzero((void*)pState->data, pState->process_size);

	procs = create_pipes(pState->p, pState->procs);
	if (procs < pState->procs) {
		cleanup_overhead(cookie);
		exit(1);
	}
}

void
cleanup_overhead(void* cookie)
{
	int i;
	struct _state* pState = (struct _state*)cookie;

     	for (i = 0; i < pState->procs; ++i) {
		close(pState->p[i][0]);
		close(pState->p[i][1]);
	}

	free(pState->p);
	if (pState->data) free(pState->data);
}

void
benchmark_overhead(iter_t iterations, void* cookie)
{
	struct _state* pState = (struct _state*)cookie;
	int	i = 0;
	int	msg = 1;
	int	sum = 0;

	while (iterations-- > 0) {
		if (write(pState->p[i][1], &msg, sizeof(msg)) != sizeof(msg)) {
			perror("read/write on pipe");
			exit(1);				
		}
		if (read(pState->p[i][0], &msg, sizeof(msg)) != sizeof(msg)) {
			perror("read/write on pipe");
			exit(1);
		}
		if (++i == pState->procs) {
			i = 0;
		}
		sum += sumit(pState->data, pState->process_size);
	}
	use_int(sum);
}

void 
initialize(void* cookie)
{
	int procs;
	struct _state* pState = (struct _state*)cookie;

	initialize_overhead(cookie);

	pState->pids = (pid_t*)malloc(pState->procs * sizeof(pid_t));
	if (pState->pids == NULL)
		exit(1);
	bzero((void*)pState->pids, pState->procs * sizeof(pid_t));
	procs = create_daemons(pState->p, pState->pids, 
			       pState->procs, pState->process_size);
	sigterm_cleanup = 1;
	if (sigterm_received || procs < pState->procs) {
		pState->procs = procs;
		cleanup(cookie);
		exit(1);
	}
}

void cleanup(void* cookie)
{
	int i;
	struct _state* pState = (struct _state*)cookie;

	/*
	 * Close the pipes and kill the children.
	 */
     	for (i = 1; pState->pids && i < pState->procs; ++i) {
		if (pState->pids[i] > 0) {
			kill(pState->pids[i], SIGKILL);
			waitpid(pState->pids[i], NULL, 0);
		}
	}
	if (pState->pids)
		free(pState->pids);
	pState->pids = NULL;
	cleanup_overhead(cookie);
}

void
benchmark(iter_t iterations, void* cookie)
{
	struct _state* pState = (struct _state*)cookie;
	int	msg;
	int	sum = 0;

	/*
	 * Main process - all others should be ready to roll, time the
	 * loop.
	 */
	while (iterations-- > 0) {
		if (write(pState->p[0][1], &msg, sizeof(msg)) !=
		    sizeof(msg)) {
			perror("read/write on pipe");
			exit(1);
		}
		if (read(pState->p[pState->procs-1][0], &msg, sizeof(msg)) != sizeof(msg)) {
			perror("read/write on pipe");
			exit(1);
		}
		sum += sumit(pState->data, pState->process_size);
	}
	use_int(sum);
}


void
doit(int **p, int rd, int wr, int process_size)
{
	int	msg, sum = 0 /* lint */;
	int*	data = NULL;

	signal(SIGTERM, SIG_DFL);
	if (process_size) {
		data = malloc(process_size);
		if (data) bzero((void*)data, process_size);
	}
	for ( ;; ) {
		if (read(p[rd][0], &msg, sizeof(msg)) != sizeof(msg)) {
			perror("read/write on pipe");
			break;
		}
		sum = sumit(data, process_size);
		if (write(p[wr][1], &msg, sizeof(msg)) != sizeof(msg)) {
			perror("read/write on pipe");
			break;
		}
	}
	use_int(sum);
	exit(1);
}


int
create_daemons(int **p, int pids[], int procs, int process_size)
{
	int	i;
	int	msg;

	/*
	 * Use the pipes as a ring, and fork off a bunch of processes
	 * to pass the byte through their part of the ring.
	 *
	 * Do the sum in each process and get that time before moving on.
	 */
     	for (i = 1; i < procs; ++i) {
		switch (pids[i] = fork()) {
		    case -1:	/* could not fork, out of processes? */
			return i;

		    case 0:	/* child */
#if	defined(sgi) && defined(PIN)
			sysmp(MP_MUSTRUN, i % ncpus);
#endif
			doit(p, i-1, i, process_size);
			/* NOTREACHED */

		    default:	/* parent */
			;
	    	}
		if (sigterm_received)
			return i + 1;
	}

	/*
	 * Go once around the loop to make sure that everyone is ready and
	 * to get the token in the pipeline.
	 */
	if (write(p[0][1], &msg, sizeof(msg)) != sizeof(msg) ||
	    read(p[procs-1][0], &msg, sizeof(msg)) != sizeof(msg)) {
		perror("write/read/write on pipe");
		exit(1);
	}
	return procs;
}

int
create_pipes(int **p, int procs)
{
	int	i;
	/*
	 * Get a bunch of pipes.
	 */
	morefds();
     	for (i = 0; i < procs; ++i) {
		if (pipe(p[i]) == -1) {
			return i;
		}
	}
	return procs;
}

/*
 * Bring howmuch data into the cache, assuming that the smallest cache
 * line is 16 bytes.
 */
int
sumit(int* data, int howmuch)
{
	int	done, sum = 0;
	register int *d = data;

#if 0
#define	A	sum+=d[0]+d[4]+d[8]+d[12]+d[16]+d[20]+d[24]+d[28]+\
		d[32]+d[36]+d[40]+d[44]+d[48]+d[52]+d[56]+d[60]+\
		d[64]+d[68]+d[72]+d[76]+d[80]+d[84]+d[88]+d[92]+\
		d[96]+d[100]+d[104]+d[108]+d[112]+d[116]+d[120]+d[124];\
		d+=128;
#define	TWOKB	A A A A
#else
#define	A	sum+=d[0]+d[1]+d[2]+d[3]+d[4]+d[5]+d[6]+d[7]+d[8]+d[9]+\
		d[10]+d[11]+d[12]+d[13]+d[14]+d[15]+d[16]+d[17]+d[18]+d[19]+\
		d[20]+d[21]+d[22]+d[23]+d[24]+d[25]+d[26]+d[27]+d[28]+d[29]+\
		d[30]+d[31]+d[32]+d[33]+d[34]+d[35]+d[36]+d[37]+d[38]+d[39]+\
		d[40]+d[41]+d[42]+d[43]+d[44]+d[45]+d[46]+d[47]+d[48]+d[49]+\
		d[50]+d[51]+d[52]+d[53]+d[54]+d[55]+d[56]+d[57]+d[58]+d[59]+\
		d[60]+d[61]+d[62]+d[63]+d[64]+d[65]+d[66]+d[67]+d[68]+d[69]+\
		d[70]+d[71]+d[72]+d[73]+d[74]+d[75]+d[76]+d[77]+d[78]+d[79]+\
		d[80]+d[81]+d[82]+d[83]+d[84]+d[85]+d[86]+d[87]+d[88]+d[89]+\
		d[90]+d[91]+d[92]+d[93]+d[94]+d[95]+d[96]+d[97]+d[98]+d[99]+\
		d[100]+d[101]+d[102]+d[103]+d[104]+\
		d[105]+d[106]+d[107]+d[108]+d[109]+\
		d[110]+d[111]+d[112]+d[113]+d[114]+\
		d[115]+d[116]+d[117]+d[118]+d[119]+\
		d[120]+d[121]+d[122]+d[123]+d[124]+d[125]+d[126]+d[127];\
		d+=128;	/* ints; bytes == 512 */
#define	TWOKB	A A A A
#endif

	for (done = 0; done < howmuch; done += 2048) {
		TWOKB
	}
	return (sum);
}

