/*
 * loads.c - guess the cache size(s)
 *
 * usage: loads [-L <line size>] [-M len[K|M]] [-W <warmup>] [-N <repetitions>]
 *
 * Copyright (c) 2000 Carl Staelin.
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"

struct _state {
	char*	addr;
	char*	p[8];
	int	len;
	int	line;
	int	pagesize;
};

void compute_times(struct _state* state, double* tlb_time, double* cache_time);
void initialize(void* cookie);
void benchmark(uint64 iterations, void* cookie);
void cleanup(void* cookie);

#define	FIVE(m)		m m m m m
#define	TEN(m)		FIVE(m) FIVE(m)
#define	FIFTY(m)	TEN(m) TEN(m) TEN(m) TEN(m) TEN(m)
#define	HUNDRED(m)	FIFTY(m) FIFTY(m)

#define REPEAT(m) m(0) m(1) m(2) m(3) m(4) m(5) m(6) m(7)
#define INIT(N) p##N = (addr_save==state->addr) ? sp##N : (char**)state->p[N];
#define SAVE(N) sp##N = p##N;
#define BENCHMARK(N,body) \
void benchmark_##N(uint64 iterations, void *cookie) \
{ \
	struct _state* state = (struct _state*)cookie; \
	static char *addr_save = NULL; \
	static char **sp0, **sp1, **sp2, **sp3, **sp4, **sp5, **sp6, **sp7; \
	register char **p0, **p1, **p2, **p3, **p4, **p5, **p6, **p7; \
\
	REPEAT(INIT); \
	while (iterations-- > 0) { \
		HUNDRED(body); \
	} \
\
	REPEAT(SAVE); \
	addr_save = state->addr; \
}

BENCHMARK(0, p0 = (char **)*p0;)
BENCHMARK(1, p0 = (char **)*p0; p4 = (char **)*p4;)
BENCHMARK(2, p0 = (char **)*p0; p4 = (char **)*p4; p2 = (char **)*p2;)
BENCHMARK(3, p0 = (char **)*p0; p4 = (char **)*p4; p2 = (char **)*p2; p6 = (char **)*p6;)
BENCHMARK(4, p0 = (char **)*p0; p4 = (char **)*p4; p2 = (char **)*p2; p6 = (char **)*p6; p1 = (char **)*p1;)
BENCHMARK(5, p0 = (char **)*p0; p4 = (char **)*p4; p2 = (char **)*p2; p6 = (char **)*p6; p1 = (char **)*p1; p5 = (char **)*p5;)
BENCHMARK(6, p0 = (char **)*p0; p4 = (char **)*p4; p2 = (char **)*p2; p6 = (char **)*p6; p1 = (char **)*p1; p5 = (char **)*p5; p3 = (char **)*p3;)
BENCHMARK(7, p0 = (char **)*p0; p4 = (char **)*p4; p2 = (char **)*p2; p6 = (char **)*p6; p1 = (char **)*p1; p5 = (char **)*p5; p3 = (char **)*p3; p7 = (char **)*p7;)

bench_f benchmarks[] = {
	benchmark_0,
	benchmark_1,
	benchmark_2,
	benchmark_3,
	benchmark_4,
	benchmark_5,
	benchmark_6,
	benchmark_7
};

/*
 * Assumptions:
 *
 * 1) Cache lines are a multiple of pointer-size words
 * 2) Cache lines are no larger than 1/8 of a page (typically 512 bytes)
 * 3) Pages are an even multiple of cache lines
 */
int
main(int ac, char **av)
{
	int	i, j, l, len;
	int	c;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	print_cost = 0;
	int	maxlen = 32 * 1024 * 1024;
	double	time;
	struct _state state;
	char   *usage = "[-c] [-L <line size>] [-M len[K|M]] [-W <warmup>] [-N <repetitions>]\n";

	state.line = getpagesize() / 8;
	state.pagesize = getpagesize();

	while (( c = getopt(ac, av, "cL:M:W:N:")) != EOF) {
		switch(c) {
		case 'c':
			print_cost = 1;
			break;
		case 'L':
			state.line = atoi(optarg);
			break;
		case 'M':
			l = strlen(optarg);
			if (optarg[l-1] == 'm' || optarg[l-1] == 'M') {
				maxlen = 1024 * 1024;
				optarg[l-1] = 0;
			} else if (optarg[l-1] == 'k' || optarg[l-1] == 'K') {
				maxlen = 1024;
				optarg[l-1] = 0;
			}
			maxlen *= atoi(optarg);
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

	for (i = sizeof(char*); i <= maxlen; i<<=1) { 
		state.len = i;
		benchmp(initialize, benchmarks[0], cleanup, 0, 1, 
			warmup, repetitions, &state);

		/* We want nanoseconds / load. */
		time = (1000. * (double)gettime()) / (100. * (double)get_n());

		benchmp(initialize, benchmarks[7], cleanup, 0, 1, 
			warmup, repetitions, &state);

		time /= (1000. * (double)gettime()) / (100. * (double)(8 * get_n()));
		j = (int)(time + 0.5);

		fprintf(stderr, "loads: %d loads %d bytes\n", j, state.len);
	}

#if 0
	if (print_cost) {
		fprintf(stderr, "cache: %d bytes %.5f nanoseconds\n", len, time);
	} else {
		fprintf(stderr, "cache: %d bytes\n", len);
	}
#endif

	return(0);
}


/*
 * This will access len bytes
 */
void
initialize(void* cookie)
{
	int i, j, k, nwords, nlines, nbytes, npages;
	unsigned int r;
	char ***pages;
	int    *lines;
	int    *words;
	struct _state* state = (struct _state*)cookie;
	register char *p = 0 /* lint */;

	nbytes = state->len;
	nwords = state->line / sizeof(char*);
	nlines = state->pagesize / state->line;
	npages = (nbytes + state->pagesize) / state->pagesize;

	words = (int*)malloc(nwords * sizeof(int));
	lines = (int*)malloc(nlines * sizeof(int));
	pages = (char***)malloc(npages * sizeof(char**));
	state->p[0] = state->addr = (char*)malloc(nbytes + 2 * state->pagesize);

	if ((unsigned long)state->p[0] % state->pagesize) {
		state->p[0] += state->pagesize - (unsigned long)state->p[0] % state->pagesize;
	}

	if (state->addr == NULL || pages == NULL) {
		exit(0);
	}

	srand(getpid());

	/* first, layout the sequence of page accesses */
	p = state->p[0];
	for (i = 0; i < npages; ++i) {
		pages[i] = (char**)p;
		p += state->pagesize;
	}

	/* randomize the page sequences (except for zeroth page) */
	r = (rand() << 15) ^ rand();
	for (i = npages - 2; i > 0; --i) {
		char** l;
		r = (r << 1) ^ (rand() >> 4);
		l = pages[(r % i) + 1];
		pages[(r % i) + 1] = pages[i + 1];
		pages[i + 1] = l;
	}

	/* layout the sequence of line accesses */
	for (i = 0; i < nlines; ++i) {
		lines[i] = i * state->pagesize / (nlines * sizeof(char*));
	}

	/* randomize the line sequences */
	for (i = nlines - 2; i > 0; --i) {
		int l;
		r = (r << 1) ^ (rand() >> 4);
		l = lines[(r % i) + 1];
		lines[(r % i) + 1] = lines[i];
		lines[i] = l;
	}

	/* layout the sequence of word accesses */
	for (i = 0; i < nwords; ++i) {
		words[i] = i * state->line / (nwords * sizeof(char*));
	}

	/* randomize the word sequences */
	for (i = nwords - 2; i > 0; --i) {
		int l;
		r = (r << 1) ^ (rand() >> 4);
		l = words[(r % i) + 1];
		words[(r % i) + 1] = words[i];
		words[i] = l;
	}

	/* setup the run through the pages */
	for (i = 0, k = 0; i < npages; ++i) {
		for (j = 0; j < nlines - 1; ++j) {
			pages[i][lines[j]+words[k%nwords]] = (char*)(pages[i] + lines[j+1] + words[(k+1)%nwords]);
			k++;
		}
		if (i == npages - 1) {
			pages[i][lines[j]+words[k%nwords]] = (char*)(pages[0] + lines[0] + words[0]);
		} else {
			pages[i][lines[j]+words[k%nwords]] = (char*)(pages[i+1] + lines[0] + words[(k+1)%nwords]);
		}
		k++;
	}

	free(pages);
	free(lines);
	free(words);

	for (p = state->p[0], i = 0; i < k; ++i) {
		if (i % (k/8) == 0) {
			state->p[i / (k/8)] = p;
		}
		p = *(char**)p;
	}
}

void cleanup(void* cookie)
{
	struct _state* state = (struct _state*)cookie;
	free(state->addr);
	state->addr = NULL;
}



