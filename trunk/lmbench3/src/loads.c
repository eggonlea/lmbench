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
	char*	p[16];
	int	len;
	int	line;
	int	pagesize;
};

void compute_times(struct _state* state, double* tlb_time, double* cache_time);
void initialize(void* cookie);
void benchmark(iter_t iterations, void* cookie);
void cleanup(void* cookie);

#define	FIVE(m)		m m m m m
#define	TEN(m)		FIVE(m) FIVE(m)
#define	FIFTY(m)	TEN(m) TEN(m) TEN(m) TEN(m) TEN(m)
#define	HUNDRED(m)	FIFTY(m) FIFTY(m)

#define REPEAT(m) m(0) m(1) m(2) m(3) m(4) m(5) m(6) m(7) m(8) m(9) m(10) m(11) m(12) m(13) m(14) m(15)
#define DEREF(N) p##N = (char**)*p##N
#define DECLARE(N) static char **sp##N; register char **p##N;
#define INIT(N) p##N = (addr_save==state->addr) ? sp##N : (char**)state->p[N];
#define SAVE(N) sp##N = p##N;
#define BENCHMARK(N,body) \
void benchmark_##N(iter_t iterations, void *cookie) \
{ \
	struct _state* state = (struct _state*)cookie; \
	static char *addr_save = NULL; \
	REPEAT(DECLARE); \
\
	REPEAT(INIT); \
	while (iterations-- > 0) { \
		HUNDRED(body); \
	} \
\
	REPEAT(SAVE); \
	addr_save = state->addr; \
}

BENCHMARK(0, DEREF(0);)
BENCHMARK(1, DEREF(0); DEREF(8);)
BENCHMARK(2, DEREF(0); DEREF(8); DEREF(4);)
BENCHMARK(3, DEREF(0); DEREF(8); DEREF(4); DEREF(12);)
BENCHMARK(4, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2);)
BENCHMARK(5, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2); DEREF(10);)
BENCHMARK(6, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2); DEREF(10); DEREF(6);)
BENCHMARK(7, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2); DEREF(10); DEREF(6); DEREF(14);)
BENCHMARK(8, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2); DEREF(10); DEREF(6); DEREF(14); DEREF(1);)
BENCHMARK(9, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2); DEREF(10); DEREF(6); DEREF(14); DEREF(1); DEREF(11);)
BENCHMARK(10, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2); DEREF(10); DEREF(6); DEREF(14); DEREF(1); DEREF(11); DEREF(3);)
BENCHMARK(11, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2); DEREF(10); DEREF(6); DEREF(14); DEREF(1); DEREF(11); DEREF(3); DEREF(9);)
BENCHMARK(12, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2); DEREF(10); DEREF(6); DEREF(14); DEREF(1); DEREF(11); DEREF(3); DEREF(9); DEREF(15);)
BENCHMARK(13, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2); DEREF(10); DEREF(6); DEREF(14); DEREF(1); DEREF(11); DEREF(3); DEREF(9); DEREF(15); DEREF(5);)
BENCHMARK(14, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2); DEREF(10); DEREF(6); DEREF(14); DEREF(1); DEREF(11); DEREF(3); DEREF(9); DEREF(15); DEREF(5); DEREF(13);)
BENCHMARK(15, DEREF(0); DEREF(8); DEREF(4); DEREF(12); DEREF(2); DEREF(10); DEREF(6); DEREF(14); DEREF(1); DEREF(11); DEREF(3); DEREF(9); DEREF(15); DEREF(5); DEREF(13); DEREF(7);)

bench_f benchmarks[] = {
	benchmark_0,
	benchmark_1,
	benchmark_2,
	benchmark_3,
	benchmark_4,
	benchmark_5,
	benchmark_6,
	benchmark_7,
	benchmark_8,
	benchmark_9,
	benchmark_10,
	benchmark_11,
	benchmark_12,
	benchmark_13,
	benchmark_14,
	benchmark_15
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
	double	load_parallelism;
	result_t base, wide, *r_save;
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
			if (state.line < sizeof(char*))
				state.line = sizeof(char*);
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

	for (i = 16 * state.line; i <= maxlen; i<<=1) { 
		state.len = i;

		r_save = get_results();
		insertinit(&base);
		insertinit(&wide);
		
		for (j = 0; j < TRIES; ++j) {
			benchmp(initialize, benchmarks[0], cleanup, 0, 1, 
				warmup, repetitions, &state);
			insertsort(gettime(), get_n(), &base);

			benchmp(initialize, benchmarks[15], cleanup, 0, 1, 
				warmup, repetitions, &state);
			insertsort(gettime(), 16 * get_n(), &wide);
		}
		set_results(&base);
		load_parallelism = (double)gettime() / (double)get_n();

		set_results(&wide);
		load_parallelism /= (double)gettime() / get_n();
		set_results(r_save);

		fprintf(stderr, "%.5f %.3f\n", 
			state.len / (1000. * 1000.), load_parallelism);
	}

	return(0);
}


/*
 * This will access len bytes
 */
void
initialize(void* cookie)
{
	int i, j, k, nwords, nlines, nbytes, npages, npointers;
	unsigned int r;
	char ***pages;
	int    *lines;
	int    *words;
	struct _state* state = (struct _state*)cookie;
	register char *p = 0 /* lint */;

	npointers = state->len / state->line;
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
		for (j = 0; j < nlines - 1 && k < npointers - 1; ++j) {
			pages[i][lines[j]+words[k%nwords]] = (char*)(pages[i] + lines[j+1] + words[(k+1)%nwords]);
			k++;
		}
		if (i == npages - 1 || k == npointers - 1) {
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
		if (i % (k/16) == 0) {
			state->p[i / (k/16)] = p;
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
