/*
 * tlb.c - guess the cache line size
 *
 * usage: tlb [-c] [-L <line size>] [-M len[K|M]] [-W <warmup>] [-N <repetitions>]
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
	char*	p;
	int	pages;
	int	line;
	int	pagesize;
	int	warmup;
	int	repetitions;
};

int find_tlb(int start, int maxpages, double* tlb_time, double* cache_time, 
	     struct _state* state);
void compute_times(struct _state* state, double* tlb_time, double* cache_time);
void initialize_tlb(void* cookie);
void initialize_cache(void* cookie);
void benchmark(iter_t iterations, void* cookie);
void cleanup(void* cookie);

#define	ONE	p = (char **)*p;
#define	FIVE	ONE ONE ONE ONE ONE
#define	TEN	FIVE FIVE
#define	FIFTY	TEN TEN TEN TEN TEN
#define	HUNDRED	FIFTY FIFTY

#define THRESHOLD 1.15

/*
 * Assumptions:
 *
 * 1) Cache lines are a multiple of pointer-size words
 * 2) Cache lines no larger than 1/8 a page size
 * 3) Pages are an even multiple of cache lines
 */
int
main(int ac, char **av)
{
	int	i, l, len, tlb, maxpages;
	int	c;
	int	print_cost = 0;
	int	maxline = getpagesize() / sizeof(char*);
	double	tlb_time, cache_time, diff;
	struct _state state;
	char   *usage = "[-c] [-L <line size>] [-M len[K|M]] [-W <warmup>] [-N <repetitions>]\n";

	maxpages = 16 * 1024;
	state.line = state.pagesize / 8;
	state.pagesize = getpagesize();
	state.warmup = 0;
	state.repetitions = TRIES;

	tlb = 2;

	while (( c = getopt(ac, av, "cL:M:W:N:")) != EOF) {
		switch(c) {
		case 'c':
			print_cost = 1;
			break;
		case 'L':
			state.line = atoi(optarg);
			break;
		case 'M':
			maxpages = bytes(optarg);	/* max in bytes */
			maxpages /= getpagesize();	/* max in pages */
			break;
		case 'W':
			state.warmup = atoi(optarg);
			break;
		case 'N':
			state.repetitions = atoi(optarg);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}

	/* assumption: no TLB will have less than 16 entries */
	tlb = find_tlb(8, maxpages, &tlb_time, &cache_time, &state);

	if (print_cost) {
		state.pages *= 2;
		compute_times(&state, &tlb_time, &cache_time);
		fprintf(stderr, "tlb: %d pages %.5f nanoseconds\n", tlb, tlb_time - cache_time);
	} else {
		fprintf(stderr, "tlb: %d pages\n", tlb);
	}

	for (i = tlb<<1; i <= maxpages; i<<=1) {
		state.pages = i;
		compute_times(&state, &tlb_time, &cache_time);
	}

	return(0);
}

int
find_tlb(int start, int maxpages, double* tlb_time, double* cache_time, struct _state* state)
{
	int	i, lower, upper;

	for (i = start; i <= maxpages; i<<=1) {
		state->pages = i;
		compute_times(state, tlb_time, cache_time);

		if (*tlb_time / *cache_time > THRESHOLD) {
			lower = i>>1;
			upper = i;
			i = lower + (upper - lower) / 2;
			break;
		}
	}

	/* we can't find any tlb effect */
	if (i >= maxpages) {
		state->pages = 0;
		return (0);
	}

	/* use a binary search to locate point at which TLB effects start */
	while (lower + 1 < upper) {
		state->pages = i;
		compute_times(state, tlb_time, cache_time);

		if (*tlb_time / *cache_time > THRESHOLD) {
			upper = i;
		} else {
			lower = i;
		}
		i = lower + (upper - lower) / 2;
	}
	state->pages = lower;
	return (lower);
}

void
compute_times(struct _state* state, double* tlb_time, double* cache_time)
{
	int i;
	result_t tlb_results, cache_results, *r_save;

	r_save = get_results();
	insertinit(&tlb_results);
	insertinit(&cache_results);

	for (i = 0; i < TRIES; ++i) {
		benchmp(initialize_tlb, benchmark, cleanup, 0, 1, 
			state->warmup, state->repetitions, state);
		insertsort(gettime(), get_n(), &tlb_results);
	
		benchmp(initialize_cache, benchmark, cleanup, 0, 1,
			state->warmup, state->repetitions, state);
		insertsort(gettime(), get_n(), &cache_results);
	}

	/* We want nanoseconds / load. */
	set_results(&tlb_results);
	*tlb_time = (1000. * (double)gettime()) / (100. * (double)get_n());

	/* We want nanoseconds / load. */
	set_results(&cache_results);
	*cache_time = (1000. * (double)gettime()) / (100. * (double)get_n());
	set_results(r_save);

	/**/
	fprintf(stderr, "%d %.5f %.5f\n", state->pages, *tlb_time, *cache_time);
	/**/
}

/*
 * This will access one word per page, for a total of len bytes of accessed
 * memory.
 */
void
initialize_tlb(void* cookie)
{
	int i, nwords, nlines, npages, pagesize;
	unsigned int r;
	char ***pages;
	int    *lines;
	int    *words;
	struct _state* state = (struct _state*)cookie;
	register char *p = 0 /* lint */;

	pagesize = state->pagesize;
	nwords   = state->line / sizeof(char*);
	nlines   = pagesize / state->line;
	npages   = state->pages;

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
	state->p = (char*)(pages[0] + lines[0] + words[0]);

	free(pages);
	free(lines);
	free(words);

	/* run through the chain once to clear the cache */
	benchmark((npages + 100) / 100, state);
}

/*
 * This will access len bytes
 */
void
initialize_cache(void* cookie)
{
	int i, j, nwords, nlines, npages, npointers, pagesize;
	unsigned int r;
	char ***pages;
	int    *lines;
	int    *words;
	struct _state* state = (struct _state*)cookie;
	register char *p = 0 /* lint */;

	pagesize = state->pagesize;
	nwords   = state->line / sizeof(char*);
	nlines   = pagesize / state->line;
	npages   = state->pages / nlines;

	srand(getpid());

	if (state->pages % nlines)
		npages++;

	words = permutation(nwords);
	lines = permutation(nlines);
	pages = (char***)malloc(npages * sizeof(char**));
	state->addr = (char*)valloc((npages + 2) * pagesize);

	if (state->addr == NULL || pages == NULL) {
		exit(0);
	}

	/* first, layout the sequence of page accesses */
	p = state->addr;
	for (i = 0; i < npages; ++i, p += pagesize) {
		pages[i] = (char**)p;
	}
	state->p = (char*)pages[0];

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

	/* setup the run through the pages */
	for (i = 0, npointers = 0; i < npages; ++i) {
		for (j = 0; j < nlines - 1 && npointers < npages - 1; ++j) {
			pages[i][lines[j]+words[npointers%nwords]] = (char*)(pages[i] + lines[j+1] + words[(npointers+1)%nwords]);
			npointers++;
		}
		if (i == npages - 1 || npointers == npages - 1) {
			pages[i][lines[j]+words[npointers%nwords]] = (char*)(pages[0] + lines[0] + words[0]);
		} else {
			pages[i][lines[j]+words[npointers%nwords]] = (char*)(pages[i+1] + lines[0] + words[0]);
		}
		npointers++;
	}
	state->p = (char*)(pages[0] + lines[0] + words[0]);

	free(pages);
	free(lines);
	free(words);

	/* run through the chain once to clear the cache */
	benchmark((npages * nlines + 100) / 100, state);
}


void
benchmark(iter_t iterations, void *cookie)
{
	struct _state* state = (struct _state*)cookie;
	static char *addr_save = NULL;
	static char **p_save = NULL;
	register char **p = addr_save == state->addr ? p_save : (char**)state->p;

	while (iterations-- > 0) {
		HUNDRED;
	}

	use_pointer((void *)p);
	p_save = p;
	addr_save = state->addr;
}

void
cleanup(void* cookie)
{
	struct _state* state = (struct _state*)cookie;
	free(state->addr);
	state->addr = NULL;
	state->p = NULL;
}



