/*
 * lib_mem.c - library of routines used to analyze the memory hierarchy
 *
 * %W% %@%
 *
 * Copyright (c) 2000 Carl Staelin.
 * Copyright (c) 1994 Larry McVoy.  
 * Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */

#include "bench.h"

#define	FIVE(m)		m m m m m
#define	TEN(m)		FIVE(m) FIVE(m)
#define	FIFTY(m)	TEN(m) TEN(m) TEN(m) TEN(m) TEN(m)
#define	HUNDRED(m)	FIFTY(m) FIFTY(m)

#define DEREF(N)	p##N = (char**)*p##N;
#define DECLARE(N)	static char **sp##N; register char **p##N;
#define INIT(N)		p##N = (addr_save==state->addr) ? sp##N : (char**)state->p[N];
#define SAVE(N)		sp##N = p##N;

#define MEM_BENCHMARK_F(N) mem_benchmark_##N,
bench_f mem_benchmarks[] = {REPEAT_15(MEM_BENCHMARK_F)};

#define MEM_BENCHMARK_DEF(N,repeat,body) 				\
void									\
mem_benchmark_##N(iter_t iterations, void *cookie)			\
{									\
	struct mem_state* state = (struct mem_state*)cookie;		\
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

MEM_BENCHMARK_DEF(0, REPEAT_0, DEREF)
MEM_BENCHMARK_DEF(1, REPEAT_1, DEREF)
MEM_BENCHMARK_DEF(2, REPEAT_2, DEREF)
MEM_BENCHMARK_DEF(3, REPEAT_3, DEREF)
MEM_BENCHMARK_DEF(4, REPEAT_4, DEREF)
MEM_BENCHMARK_DEF(5, REPEAT_5, DEREF)
MEM_BENCHMARK_DEF(6, REPEAT_6, DEREF)
MEM_BENCHMARK_DEF(7, REPEAT_7, DEREF)
MEM_BENCHMARK_DEF(8, REPEAT_8, DEREF)
MEM_BENCHMARK_DEF(9, REPEAT_9, DEREF)
MEM_BENCHMARK_DEF(10, REPEAT_10, DEREF)
MEM_BENCHMARK_DEF(11, REPEAT_11, DEREF)
MEM_BENCHMARK_DEF(12, REPEAT_12, DEREF)
MEM_BENCHMARK_DEF(13, REPEAT_13, DEREF)
MEM_BENCHMARK_DEF(14, REPEAT_14, DEREF)
MEM_BENCHMARK_DEF(15, REPEAT_15, DEREF)


void
mem_cleanup(void* cookie)
{
	struct mem_state* state = (struct mem_state*)cookie;
	free(state->addr);
	state->addr = NULL;
}

/*
 * mem_initialize
 *
 * Create a circular pointer chain that runs through memory.
 *
 * The chain threads through each cache line on a page before
 * moving to the next page.  The order of cache line accesses
 * is randomized to defeat cache prefetching algorithms.  In
 * addition, the order of page accesses is randomized.  Finally,
 * to reduce the impact of incorrect line-size estimates on
 * machines with direct-mapped caches, we randomize which 
 * word in the cache line is used to hold the pointer.
 *
 * It initializes state->width pointers to elements evenly
 * spaced through the chain.
 */
void
mem_initialize(void* cookie)
{
	int i, j, k, w, nwords, nlines, nbytes, npages, nmpages, npointers;
	unsigned int r;
	int    *pages;
	int    *lines;
	int    *words;
	struct mem_state* state = (struct mem_state*)cookie;
	register char *p = 0 /* lint */;

	npointers = state->len / state->line;
	nbytes = state->len;
	nwords = state->line / sizeof(char*);
	nlines = state->pagesize / state->line;
	npages = (nbytes + state->pagesize - 1) / state->pagesize;
	nmpages= (state->maxlen + state->pagesize - 1) / state->pagesize;

	srand(getpid());

	words = permutation(nwords, sizeof(char*));
	lines = permutation(nlines, state->line);
	pages = permutation(nmpages, state->pagesize);
	p = state->addr = (char*)malloc(state->maxlen + 2 * state->pagesize);

	if (state->addr == NULL \
	    || pages == NULL || lines == NULL || words == NULL) {
		exit(0);
	}

	if ((unsigned long)p % state->pagesize) {
		p += state->pagesize;
		p -= (unsigned long)p % state->pagesize;
	}

	/* setup the run through the pages */
	for (i = 0, k = 0, w = 0; i < npages; ++i) {
		for (j = 0; j < nlines - 1 && k < npointers - 1; ++j) {
			*(char**)(p + pages[i] + lines[j] + words[w]) =
			    p + pages[i] + lines[j+1] + words[w];
			if (k % (npointers/state->width) == 0
			    && k / (npointers/state->width) < MAX_MEM_PARALLELISM) {
				state->p[k / (npointers/state->width)] = 
					p + pages[i] + lines[j] + words[w];
			}
			k++;
		}
		if (i == npages - 1 || k == npointers - 1) {
			*(char**)(p + pages[i] + lines[j] + words[w]) =
			    p + pages[0] + lines[0] + words[0];
		} else {
			*(char**)(p + pages[i] + lines[j] + words[w]) =
			    p + pages[i+1] + lines[0] + words[(w+1)%nwords];
		}
		if (k % (npointers/state->width) == 0
		    && k / (npointers/state->width) < MAX_MEM_PARALLELISM) {
			state->p[k / (npointers/state->width)] = 
				p + pages[i] + lines[j] + words[w];
		}
		k++;
		w = (w+1) % nwords;
	}

	free(pages);
	free(lines);
	free(words);

	/* now, run through the chain once to clear the cache */
	(*mem_benchmarks[state->width-1])((npointers + 100) / 100, state);
}

/*
 * line_initialize
 *
 * This is very similar to mem_initialize, except that we always use
 * the first element of the cache line to hold the pointer.
 *
 */
void
line_initialize(void* cookie)
{
	int i, j, k, line, nlines, npages;
	unsigned int r;
	int    *pages;
	int    *lines;
	struct mem_state* state = (struct mem_state*)cookie;
	register char *p = 0 /* lint */;

	state->width = 1;
	nlines = state->pagesize / state->line;
	npages = (state->len + state->pagesize - 1) / state->pagesize;

	srand(getpid());

	p     = state->addr = (char*)valloc(state->len + state->pagesize);
	pages = permutation(npages, state->pagesize);
	lines = permutation(nlines, state->line);

	if (state->addr == NULL || lines == NULL || pages == NULL) {
		exit(0);
	}

	if ((unsigned long)p % state->pagesize) {
		p += state->pagesize;
		p -= (unsigned long)p % state->pagesize;
	}

	/* new setup runs through the lines */
	for (i = 0; i < npages; ++i) {
		/* sequence through the first word of each line */
		for (j = 0; j < nlines - 1; ++j) {
			*(char**)(p + pages[i] + lines[j]) = 
				p + pages[i] + lines[j+1];
		}

		/* jump to the fist word of the first line on next page */
		*(char**)(p + pages[i] + lines[j]) = 
			p + pages[(i < npages-1) ? i+1 : 0] + lines[0];
	}
	state->p[0] = p + pages[0] + lines[0];

	free(lines);
	free(pages);

	/* now, run through the chain once to clear the cache */
	mem_benchmark_0((nlines * npages + 100) / 100, state);
}

/*
 * tlb_initialize
 *
 * Build a pointer chain which accesses one word per page, for a total
 * of (line * pages) bytes of data loaded into cache.  
 *
 * If the number of elements in the chain (== #pages) is larger than the
 * number of pages addressed by the TLB, then each access should cause
 * a TLB miss (certainly as the number of pages becomes much larger than
 * the TLB-addressed space).
 *
 * In addition, if we arrange the chain properly, each word we access
 * will be in the cache.
 *
 * This means that the average access time for each pointer dereference
 * should be a cache hit plus a TLB miss.
 *
 */
void
tlb_initialize(void* cookie)
{
	int i, nwords, nlines, npages, pagesize;
	unsigned int r;
	char ***pages;
	int    *lines;
	int    *words;
	struct mem_state* state = (struct mem_state*)cookie;
	register char *p = 0 /* lint */;

	pagesize = state->pagesize;
	nwords   = state->line / sizeof(char*);
	nlines   = pagesize / state->line;
	npages   = state->len / pagesize;

	srand(getpid());

	words = permutation(nwords, 1);
	lines = permutation(nlines, 1);
	pages = (char***)malloc(npages * sizeof(char**));
	state->addr = (char*)valloc(pagesize);

	if (state->addr == NULL || pages == NULL || lines == NULL || words == NULL) {
		exit(0);
	}

	/* first, layout the sequence of page accesses */
	for (i = 0; i < npages; ++i) {
		p = (char*)valloc(pagesize);
		if ((unsigned long)state->p % pagesize) {
			free(p);
			p = (char*)valloc(2 * pagesize);
			p += pagesize - (unsigned long)p % pagesize;
		}
		pages[i] = (char**)p;
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

	for (i = 0; i < nlines; ++i)
		lines[i] *= nwords;

	/* now setup run through the pages */
	for (i = 0; i < npages - 1; ++i) {
		pages[i][lines[i % nlines]+words[i%nwords]] = (char*)(pages[i+1] + lines[(i+1) % nlines] + words[(i+1) % nwords]);
	}
	pages[i][lines[i % nlines]+words[i%nwords]] = (char*)(pages[0] + lines[0] + words[0]);
	state->p[0] = (char*)(pages[0] + lines[0] + words[0]);

	free(pages);
	free(lines);
	free(words);

	/* run through the chain once to clear the cache */
	mem_benchmark_0((npages + 100) / 100, state);
}

int
line_find(int len, int warmup, int repetitions, struct mem_state* state)
{
	int 	i, j, big_jump;
	int	maxline = getpagesize() / 8;
	double	baseline, t;

	state->len = len;
	big_jump = 0;

	for (i = sizeof(char*); i <= maxline; i<<=1) {
		t = line_test(i, warmup, repetitions, state);

		if (i > sizeof(char*)) {
			if (t > 1.3 * baseline) {
				big_jump = 1;
			} else if (big_jump && t < 1.15 * baseline) {
				return (i>>1);
			}
		}
		baseline = t;
	}

	return (0);
}

double
line_test(int line, int warmup, int repetitions, struct mem_state* state)
{
	int	i;
	double	t;
	result_t r, *r_save;

	state->line = line;
	r_save = get_results();
	insertinit(&r);
	for (i = 0; i < 5; ++i) {
		benchmp(line_initialize, mem_benchmark_0, mem_cleanup, 
			0, 1, warmup, repetitions, state);
		insertsort(gettime(), get_n(), &r);
	}
	set_results(&r);
	t = 10. * (double)gettime() / (double)get_n();
	set_results(r_save);
	
	/*
	fprintf(stderr, "%d\t%.5f\t%d\n", line, t, state->len); 
	/**/

	return (t);
}

double
par_mem(int len, int warmup, int repetitions, struct mem_state* state)
{
	int	i;
	double	baseline, max_par, par;

	state->len = len;
	state->maxlen = len;
	max_par = -1.;
		
	for (i = 0; i < MAX_MEM_PARALLELISM; ++i) {
		state->width = i + 1;
		benchmp(mem_initialize, mem_benchmarks[i], mem_cleanup, 
			0, 1, warmup, repetitions, state);
		if (i == 0) {
			baseline = (double)gettime() / (double)get_n();
		} else if (gettime() > 0) {
			par = baseline;
			par /= (double)gettime() / (double)((i + 1) * get_n());
			if (par > max_par) {
				max_par = par;
			}
		}
	}

	return max_par;
}


