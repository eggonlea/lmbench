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
	int	width;
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

#define MAX_LOAD_PARALLELISM 16

#define REPEAT_0(m)	m(0)
#define REPEAT_1(m)	REPEAT_0(m) m(1)
#define REPEAT_2(m)	REPEAT_1(m) m(2)
#define REPEAT_3(m)	REPEAT_2(m) m(3)
#define REPEAT_4(m)	REPEAT_3(m) m(4)
#define REPEAT_5(m)	REPEAT_4(m) m(5)
#define REPEAT_6(m)	REPEAT_5(m) m(6)
#define REPEAT_7(m)	REPEAT_6(m) m(7)
#define REPEAT_8(m)	REPEAT_7(m) m(8)
#define REPEAT_9(m)	REPEAT_8(m) m(9)
#define REPEAT_10(m)	REPEAT_9(m) m(10)
#define REPEAT_11(m)	REPEAT_10(m) m(11)
#define REPEAT_12(m)	REPEAT_11(m) m(12)
#define REPEAT_13(m)	REPEAT_12(m) m(13)
#define REPEAT_14(m)	REPEAT_13(m) m(14)
#define REPEAT_15(m)	REPEAT_14(m) m(15)

#define DEREF(N)	p##N = (char**)*p##N;
#define DECLARE(N)	static char **sp##N; register char **p##N;
#define INIT(N)		p##N = (addr_save==state->addr) ? sp##N : (char**)state->p[N];
#define SAVE(N)		sp##N = p##N;

#define BENCHMARK(N,repeat,body) 					\
void benchmark_##N(iter_t iterations, void *cookie) 			\
{									\
	struct _state* state = (struct _state*)cookie;			\
	static char *addr_save = NULL;					\
	repeat(DECLARE);						\
									\
	repeat(INIT);							\
	while (iterations-- > 0) {					\
		HUNDRED(repeat(body));					\
	}								\
									\
	repeat(SAVE);							\
	addr_save = state->addr;					\
}

BENCHMARK(0, REPEAT_0, DEREF)
BENCHMARK(1, REPEAT_1, DEREF)
BENCHMARK(2, REPEAT_2, DEREF)
BENCHMARK(3, REPEAT_3, DEREF)
BENCHMARK(4, REPEAT_4, DEREF)
BENCHMARK(5, REPEAT_5, DEREF)
BENCHMARK(6, REPEAT_6, DEREF)
BENCHMARK(7, REPEAT_7, DEREF)
BENCHMARK(8, REPEAT_8, DEREF)
BENCHMARK(9, REPEAT_9, DEREF)
BENCHMARK(10, REPEAT_10, DEREF)
BENCHMARK(11, REPEAT_11, DEREF)
BENCHMARK(12, REPEAT_12, DEREF)
BENCHMARK(13, REPEAT_13, DEREF)
BENCHMARK(14, REPEAT_14, DEREF)
BENCHMARK(15, REPEAT_15, DEREF)

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
	int	i, j, k, l, len;
	int	c;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	print_cost = 0;
	int	maxlen = 32 * 1024 * 1024;
	double	baseline, max_load_parallelism, load_parallelism;
	result_t **results, *r_save;
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
			maxlen = bytes(optarg);
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

	results = (result_t**)malloc(MAX_LOAD_PARALLELISM * sizeof(result_t*));
	for (i = 0; i < MAX_LOAD_PARALLELISM; ++i) {
		results[i] = (result_t*)malloc(sizeof(result_t));
	}

	for (i = MAX_LOAD_PARALLELISM * state.line; i <= maxlen; i<<=1) { 
		state.len = i;
		r_save = get_results();
		
		for (k = 0; k < MAX_LOAD_PARALLELISM; ++k) {
			insertinit(results[k]);
		}

		for (j = 0; j < TRIES; ++j) {
			for (k = 0; k < MAX_LOAD_PARALLELISM; ++k) {
				state.width = k + 1;
				benchmp(initialize, benchmarks[k], cleanup, 
					0, 1, warmup, repetitions, &state);
				insertsort(gettime(), state.width * get_n(), results[k]);
			}
		}
		set_results(results[0]);
		baseline = (double)gettime() / (double)get_n();
		max_load_parallelism = 1.;

		for (k = 1; k < MAX_LOAD_PARALLELISM; ++k) {
			set_results(results[k]);
			load_parallelism = baseline;
			load_parallelism /= (double)gettime() / (double)get_n();
			if (load_parallelism > max_load_parallelism) {
				max_load_parallelism = load_parallelism;
			}
		}

		fprintf(stderr, "%.6f %.2f\n", 
			state.len / (1000. * 1000.), max_load_parallelism);

		set_results(r_save);
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
		if (i % (k/state->width) == 0) {
			state->p[i / (k/state->width)] = p;
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
