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

void compute_times(struct _state* state, double* tlb_time, double* cache_time);
void initialize_tlb(void* cookie);
void initialize_cache(void* cookie);
void benchmark(uint64 iterations, void* cookie);
void cleanup(void* cookie);

#define	ONE	p = (char **)*p;
#define	FIVE	ONE ONE ONE ONE ONE
#define	TEN	FIVE FIVE
#define	FIFTY	TEN TEN TEN TEN TEN
#define	HUNDRED	FIFTY FIFTY

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
			l = strlen(optarg);
			if (optarg[l-1] == 'm' || optarg[l-1] == 'M') {
				maxpages = 1024 * 1024;
				optarg[l-1] = 0;
			} else if (optarg[l-1] == 'k' || optarg[l-1] == 'K') {
				maxpages = 1024;
				optarg[l-1] = 0;
			}
			maxpages *= atoi(optarg);
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

/*
	state.pages = 4;
	initialize_cache(&state);
	benchmark(1000, &state);
	cleanup(&state);
	exit(0);
/**/

	for (i = 2; i <= maxpages; i<<=1) {
		state.pages = i;
		compute_times(&state, &tlb_time, &cache_time);

		if (tlb_time / cache_time > 1.25) {
			i>>=1;
			break;
		}
		tlb = state.pages;
	}

	/* we can't find any tlb effect */
	if (i == maxpages)
		exit(0);

	for (++i; i <= maxpages; ++i) {
		state.pages = i;
		compute_times(&state, &tlb_time, &cache_time);

		if (tlb_time / cache_time > 1.15) {
			if (print_cost) {
				state.pages *= 2;
				compute_times(&state, &tlb_time, &cache_time);
			}
			break;
		}
		tlb = state.pages;
	}

	for (i = 2; i <= maxpages; i<<=1) {
		state.pages = i;
		compute_times(&state, &tlb_time, &cache_time);
	}

	if (print_cost) {
		printf("tlb: %d pages %.5f nanoseconds\n", tlb, tlb_time - cache_time);
	} else {
		printf("tlb: %d pages\n", tlb);
	}

	return(0);
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

	fprintf(stderr, "%d %.5f %.5f\n", state->pages, *tlb_time, *cache_time);
}

/*
 * This will access one word per page, for a total of len bytes of accessed
 * memory.
 */
void
initialize_tlb(void* cookie)
{
	int i, nwords, nlines, npages;
	unsigned int r;
	char ***pages;
	int    *lines;
	int    *words;
	struct _state* state = (struct _state*)cookie;
	register char *p = 0 /* lint */;

	nwords = state->line / sizeof(char*);
	nlines = state->pagesize / state->line;
	npages = state->pages;

	words = (int*)malloc(nwords * sizeof(int));
	lines = (int*)malloc(nlines * sizeof(int));
	pages = (char***)malloc(npages * sizeof(char**));
	state->p = state->addr = (char*)malloc((state->pages + 2) * state->pagesize);

	if (state->addr == NULL || pages == NULL || lines == NULL) {
		exit(0);
	}

	if ((unsigned long)state->p % state->pagesize) {
		state->p += state->pagesize - (unsigned long)state->p % state->pagesize;
	}

	srand(getpid());

	/* first, layout the sequence of page accesses */
	p = state->p;
	for (i = 0; i < state->pages; ++i) {
		pages[i] = (char**)p;
		p += state->pagesize;
	}

	/* randomize the page sequences (except for zeroth page) */
	r = (rand() << 15) ^ rand();
	for (i = state->pages - 2; i > 0; --i) {
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

	/* now setup run through the pages */
	for (i = 0; i < state->pages - 1; ++i) {
		pages[i][lines[i % nlines]+words[i%nwords]] = (char*)(pages[i+1] + lines[(i+1) % nlines] + words[(i+1) % nwords]);
	}
	pages[i][lines[i % nlines]+words[i%nwords]] = (char*)(pages[0] + lines[0] + words[0]);

	free(pages);
	free(lines);
	free(words);

	/* run through the chain once to clear the cache */
	benchmark((state->pages + 100) / 100, state);
}

/*
 * This will access len bytes
 */
void
initialize_cache(void* cookie)
{
	int i, j, nwords, nlines, npages, npointers;
	unsigned int r;
	char ***pages;
	int    *lines;
	int    *words;
	struct _state* state = (struct _state*)cookie;
	register char *p = 0 /* lint */;

	nwords = state->line / sizeof(char*);
	nlines = state->pagesize / state->line;
	npages = state->pages / nlines;

	if (state->pages % nlines)
		npages++;

	words = (int*)malloc(nwords * sizeof(int));
	lines = (int*)malloc(nlines * sizeof(int));
	pages = (char***)malloc(npages * sizeof(char**));
	state->p = state->addr = (char*)malloc((npages + 2) * state->pagesize);

	if ((unsigned long)state->p % state->pagesize) {
		state->p += state->pagesize - (unsigned long)state->p % state->pagesize;
	}

	if (state->addr == NULL || pages == NULL) {
		exit(0);
	}

	srand(getpid());

	/* first, layout the sequence of page accesses */
	p = state->p;
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
	for (i = 0, npointers = 0; i < npages; ++i) {
		for (j = 0; j < nlines - 1 && npointers < state->pages - 1; ++j) {
			pages[i][lines[j]+words[npointers%nwords]] = (char*)(pages[i] + lines[j+1] + words[(npointers+1)%nwords]);
			npointers++;
		}
		if (i == npages - 1 || npointers == state->pages - 1) {
			pages[i][lines[j]+words[npointers%nwords]] = (char*)(pages[0] + lines[0] + words[0]);
		} else {
			pages[i][lines[j]+words[npointers%nwords]] = (char*)(pages[i+1] + lines[0] + words[0]);
		}
		npointers++;
	}

	free(pages);
	free(lines);
	free(words);

	/* run through the chain once to clear the cache */
	benchmark((npages * nlines + 100) / 100, state);
}


void benchmark(uint64 iterations, void *cookie)
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

void cleanup(void* cookie)
{
	struct _state* state = (struct _state*)cookie;
	free(state->addr);
	state->addr = NULL;
	state->p = NULL;
}



