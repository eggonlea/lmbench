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
mem_initialize(void* cookie)
{
	int i, j, k, nwords, nlines, nbytes, npages, npointers;
	unsigned int r;
	char ***pages;
	int    *lines;
	int    *words;
	struct mem_state* state = (struct mem_state*)cookie;
	register char *p = 0 /* lint */;

	npointers = state->len / state->line;
	nbytes = state->len;
	nwords = state->line / sizeof(char*);
	nlines = state->pagesize / state->line;
	npages = (nbytes + state->pagesize) / state->pagesize;

	srand(getpid());

	words = permutation(nwords);
	lines = permutation(nlines);
	pages = (char***)malloc(npages * sizeof(char**));
	state->p[0] = state->addr = (char*)malloc(nbytes + 2 * state->pagesize);

	if ((unsigned long)state->p[0] % state->pagesize) {
		state->p[0] += state->pagesize;
		state->p[0] -= (unsigned long)state->p[0] % state->pagesize;
	}

	if (state->addr == NULL || pages == NULL) {
		exit(0);
	}

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
		lines[i] *= state->pagesize / (nlines * sizeof(char*));
	}

	/* layout the sequence of word accesses */
	for (i = 0; i < nwords; ++i) {
		words[i] *= state->line / (nwords * sizeof(char*));
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

	state->p[0] = (char*)(pages[0] + lines[0] + words[0]);
	for (p = state->p[0], i = 0; i < k; ++i) {
		if (i % (k/state->width) == 0) {
			state->p[i / (k/state->width)] = p;
		}
		p = *(char**)p;
	}
}

void
line_initialize(void* cookie)
{
	int i, j, k, last, line, nlines, npages;
	unsigned int r;
	char ***pages;
	int    *lines;
	struct mem_state* state = (struct mem_state*)cookie;
	register char *p = 0 /* lint */;

	state->width = 1;
	last = state->line - 1;
	line = state->line * sizeof(char*);
	nlines = state->pagesize / line;
	npages = state->len / state->pagesize;

	srand(getpid());

	state->p[0] = state->addr = (char*)valloc(state->len + state->pagesize);
	pages = (char***)malloc(npages * sizeof(char**));
	lines = permutation(nlines);

	if (state->addr == NULL || lines == NULL || pages == NULL) {
		exit(0);
	}

	if ((unsigned long)state->p[0] % state->pagesize) {
		state->p[0] += state->pagesize;
		state->p[0] -= (unsigned long)state->p[0] % state->pagesize;
	}

	/* first, layout the sequence of page accesses */
	p = state->p[0];
	for (i = 0; i < npages; ++i) {
		pages[i] = (char**)p;
		p += state->pagesize;
	}

	/* randomize the page sequences */
	r = (rand() << 15) ^ rand();
	for (i = npages - 1; i > 0; --i) {
		char **l;
		r = (r << 1) ^ (rand() >> 4);
		l = pages[r % i];
		pages[r % i] = pages[i];
		pages[i] = l;
	}

	for (i = 0; i < nlines; ++i)
		lines[i] *= state->line;

	/* new setup runs through the lines */
	for (i = 0; i < npages; ++i) {
		/* sequence through the first word of each line */
		for (j = 0; j < nlines - 1; ++j) {
			pages[i][lines[j]] = (char*)(pages[i] + lines[j+1]);
		}

		/* jump to the fist word of the first line on next page */
		if (i < npages - 1) {
			pages[i][lines[j]] = (char*)(pages[i+1] + lines[0]);
		} else {
			pages[i][lines[j]] = (char*)(pages[0] + lines[0]);
		}
	}
	state->p[0] = (char*)(pages[0] + lines[0]);

	free(lines);
	free(pages);

	/* now, run through the chain once to clear the cache */
	mem_benchmark_0((nlines * npages + 100) / 100, state);
}

/*
 * This will access one word per page, for a total of 
 * (line * pages) of data loaded into cache.
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

	words = permutation(nwords);
	lines = permutation(nlines);
	pages = (char***)malloc(npages * sizeof(char**));
	state->addr = (char*)valloc(pagesize);

	if (state->addr == NULL || pages == NULL || lines == NULL) {
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


void
mem_cleanup(void* cookie)
{
	struct mem_state* state = (struct mem_state*)cookie;
	free(state->addr);
	state->addr = NULL;
}

