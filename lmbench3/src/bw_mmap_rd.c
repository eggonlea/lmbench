/*
 * bw_mmap_rd.c - time reading & summing of a file using mmap
 *
 * Usage: bw_mmap_rd [-P <parallelism>] [-W <warmup>] [-N <repetitions>] size file
 *
 * Sizes less than 2m are not recommended.  Memory is read by summing it up
 * so the numbers include the cost of the adds.  If you use sizes large
 * enough, you can compare to bw_mem_rd and get the cost of TLB fills 
 * (very roughly).
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"
#ifdef MAP_FILE
#	define	MMAP_FLAGS	MAP_FILE|MAP_SHARED
#else
#	define	MMAP_FLAGS	MAP_SHARED
#endif

#define	TYPE	int
#define	MINSZ	(sizeof(TYPE) * 128)
#define	CHK(x)	if ((int)(x) == -1) { perror("x"); exit(1); }

typedef struct _state {
	int	nbytes;
	char	filename[256];
	int	fd;
	int	clone;
	TYPE	*buf;
	TYPE	*lastone;
} state_t;

void time_no_open(iter_t iterations, void * cookie);
void time_with_open(iter_t iterations, void * cookie);
void initialize(void *cookie);
void init_open(void *cookie);
void cleanup(void *cookie);

int main(int ac, char **av)
{
	int	fd, nbytes;
	struct	stat sbuf;
	TYPE	*buf, *lastone;
	int	parallel = 1;
	int	warmup = 0;
	int	repetitions = TRIES;
	state_t	state;
	int	c;
	char	*usage = "[-P <parallelism>] [-W <warmup>] [-N <repetitions>] <size> open2close|mmap_only <filename>";

	state.clone = 0;

	while (( c = getopt(ac, av, "P:W:N:C")) != EOF) {
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
		case 'C':
			state.clone = 1;
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}

	/* should have three arguments left (bytes type filename) */
	if (optind + 3 != ac) {
		lmbench_usage(ac, av, usage);
	}

	nbytes = state.nbytes = bytes(av[optind]);
	strcpy(state.filename,av[optind+2]);
	CHK(stat(state.filename, &sbuf));
	if ((S_ISREG(sbuf.st_mode) && nbytes > sbuf.st_size) 
	    || (nbytes < MINSZ)) {
		fprintf(stderr,"<size> out of range!\n");
		exit(1);
	}

	if (!strcmp("open2close", av[optind+1])) {
		benchmp(initialize, time_with_open, cleanup,
			0, parallel, warmup, repetitions, &state);
	} else if (!strcmp("mmap_only", av[optind+1])) {
		benchmp(init_open, time_no_open, cleanup,
			0, parallel, warmup, repetitions, &state);
	} else {
		lmbench_usage(ac, av, usage);
	}
	bandwidth(nbytes, get_n() * parallel, 0);
	return (0);
}

void
initialize(void* cookie)
{
	state_t	*state = (state_t *) cookie;

	state->fd = -1;
	state->buf = NULL;

	if (state->clone) {
		char buf[8192];
		char* s;

		/* copy original file into a process-specific one */
		sprintf(buf, "%d", (int)getpid());
		s = (char*)malloc(strlen(state->filename) + strlen(buf) + 1);
		sprintf(s, "%s%d", state->filename, (int)getpid());
		if (cp(state->filename, s, S_IREAD|S_IWRITE) < 0) {
			perror("creating private tempfile");
			unlink(s);
			exit(1);
		}
		strcpy(state->filename, s);
	}
}

void
init_open(void *cookie)
{
	state_t *state = (state_t *) cookie;

	initialize(cookie);
	CHK(state->fd = open(state->filename, 0));
	CHK(state->buf = (TYPE*)mmap(0, state->nbytes, PROT_READ,
				     MMAP_FLAGS, state->fd, 0));
	state->lastone = (TYPE*)((char*)state->buf + state->nbytes - MINSZ);
}

void
cleanup(void *cookie)
{
	state_t *state = (state_t *) cookie;
	if (state->buf) munmap((void*)state->buf, state->nbytes);
	if (state->fd >= 0) close(state->fd);
	if (state->clone) unlink(state->filename);
}

int
doit(register TYPE *p, register TYPE *lastone)
{
	register int sum = 0;
	while (p <= lastone) {
		sum += p[0]+p[1]+p[2]+p[3]+p[4]+p[5]+p[6]+p[7]+p[8]+
		    p[9]+p[10]+p[11]+p[12]+p[13]+p[14]+p[15]+p[16]+p[17]+
		    p[18]+p[19]+p[20]+p[21]+p[22]+p[23]+p[24]+p[25]+p[26]+
		    p[27]+p[28]+p[29]+p[30]+p[31]+p[32]+p[33]+p[34]+p[35]+
		    p[36]+p[37]+p[38]+p[39]+p[40]+p[41]+p[42]+p[43]+
		    p[44]+p[45]+p[46]+p[47]+p[48]+p[49]+p[50]+p[51]+
		    p[52]+p[53]+p[54]+p[55]+p[56]+p[57]+p[58]+p[59]+
		    p[60]+p[61]+p[62]+p[63]+p[64]+p[65]+p[66]+p[67]+
		    p[68]+p[69]+p[70]+p[71]+p[72]+p[73]+p[74]+p[75]+
		    p[76]+p[77]+p[78]+p[79]+p[80]+p[81]+p[82]+p[83]+
		    p[84]+p[85]+p[86]+p[87]+p[88]+p[89]+p[90]+p[91]+
		    p[92]+p[93]+p[94]+p[95]+p[96]+p[97]+p[98]+p[99]+
		    p[100]+p[101]+p[102]+p[103]+p[104]+p[105]+p[106]+
		    p[107]+p[108]+p[109]+p[110]+p[111]+p[112]+p[113]+
		    p[114]+p[115]+p[116]+p[117]+p[118]+p[119]+p[120]+
		    p[121]+p[122]+p[123]+p[124]+p[125]+p[126]+p[127];
		p += 128;
	}
	return sum;
}

void time_no_open(iter_t iterations, void * cookie)
{
	state_t *state = (state_t *) cookie;
	register TYPE *p = state->buf;
	register TYPE *lastone;
	register int sum = 0;

	lastone = state->lastone;
	while (iterations-- > 0) {
	    sum += doit(p,lastone);
	}
	use_int(sum);
}

void time_with_open(iter_t iterations, void *cookie)
{
	state_t *state    = (state_t *) cookie;
	char 	*filename = state->filename;
	int	nbytes    = state->nbytes;
	int 	fd;
	register TYPE *p, *lastone;
	register int sum = 0;

	while (iterations-- > 0) {
	    CHK(fd = open(filename, 0));
	    CHK(p = (TYPE*)mmap(0, nbytes, PROT_READ, MMAP_FLAGS, fd, 0));
	    lastone = (TYPE*)((char*)p + nbytes - MINSZ);
	    sum += doit(p,lastone);
	    close(fd);
	    munmap((void*)p, nbytes);
	}
	use_int(sum);
}
