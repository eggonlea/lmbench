/*
 * cache.c - guess the cache size(s)
 *
 * usage: cache [-c] [-L <line size>] [-M len[K|M]] [-W <warmup>] [-N <repetitions>]
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
	int	len;
	int	line;
	int	pagesize;
};

int find_cache(int start, int line, 
	       int maxlen, int warmup, int repetitions, double* time);
double measure(int size, int line, int warmup, int repetitions);
void initialize(void* cookie);
void benchmark(iter_t iterations, void* cookie);
void cleanup(void* cookie);

#define	ONE	p = (char **)*p;
#define	FIVE	ONE ONE ONE ONE ONE
#define	TEN	FIVE FIVE
#define	FIFTY	TEN TEN TEN TEN TEN
#define	HUNDRED	FIFTY FIFTY

#define THRESHOLD 1.75

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
	int	line, l1_cache, l2_cache;
	int	c;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	print_cost = 0;
	int	maxlen = 32 * 1024 * 1024;
	double	time;
	char   *usage = "[-c] [-L <line size>] [-M len[K|M]] [-W <warmup>] [-N <repetitions>]\n";

	line = getpagesize() / 8;

	while (( c = getopt(ac, av, "cL:M:W:N:")) != EOF) {
		switch(c) {
		case 'c':
			print_cost = 1;
			break;
		case 'L':
			line = atoi(optarg);
			if (line < sizeof(char*))
				line = sizeof(char*);
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

	l1_cache = find_cache(sizeof(char*), 
			      line, maxlen, warmup, repetitions, &time);

	if (l1_cache >= maxlen)
		return (0);

	fprintf(stderr, "L1 cache: %d bytes %.2f nanoseconds\n", l1_cache, time);

	l2_cache = find_cache(l1_cache,
			      line, maxlen, warmup, repetitions, &time);

	if (l2_cache >= maxlen)
		return (0);

	fprintf(stderr, "L2 cache: %d bytes %.2f nanoseconds\n", l2_cache, time);

	return(0);
}

int
find_cache(int start, int line, 
	   int maxlen, int warmup, int repetitions, double *time)
{
	int	i, len, incr;
	double	baseline, current;

	/* get the baseline access time */
	baseline = measure(2 * start, line, warmup, repetitions);

	for (i = 4 * start; i <= maxlen; i<<=1) {
		current = measure(i, line, warmup, repetitions);

		/* we have crossed a cache boundary */
		if (current / baseline > THRESHOLD)
			break;
	}
	
	if (i >= maxlen)
		return i;

	incr = i>>3;
	maxlen = i;
	i>>=1;
	len = i;

	/*
	fprintf(stderr, "find_cache: baseline=%.2f, current=%.2f, ratio=%.2f, i=%d\n", baseline, current, current/baseline, i);
	/**/

	for (i; i <= maxlen; i+=incr) {
		current = measure(i, line, warmup, repetitions);

		/* we have crossed a cache boundary */
		if (current / baseline > THRESHOLD)
			break;
		len = i;
	}
	
	*time = baseline;
	return len;
}

double
measure(int size, int line, int warmup, int repetitions)
{
	int	i;
	double	time;
	result_t *r, *r_save;
	struct _state state;

	state.len = size;
	state.line = line;
	state.pagesize = getpagesize();

	r_save = get_results();
	r = (result_t*)malloc(sizeof_result(repetitions));
	insertinit(r);

	for (i = 0; i < repetitions; ++i) {
		benchmp(initialize, benchmark, cleanup, 0, 1, 
				warmup, TRIES, &state);
		save_minimum();
		insertsort(gettime(), get_n(), r);
	}
	save_minimum();
	set_results(r);

	/* We want nanoseconds / load. */
	time = (1000. * (double)gettime()) / (100. * (double)get_n());

	/*
	fprintf(stderr, "%.6f %.2f\n", state.len / (1000. * 1000.), time);
	/**/

	set_results(r_save);
	free(r);

	return time;
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

	nbytes = state->len;
	npointers = state->len / state->line;
	nwords = state->line / sizeof(char*);
	nlines = state->pagesize / state->line;
	npages = (nbytes + state->pagesize) / state->pagesize;

	words = (int*)malloc(nwords * sizeof(int));
	lines = (int*)malloc(nlines * sizeof(int));
	pages = (char***)malloc(npages * sizeof(char**));
	state->p = state->addr = (char*)malloc(nbytes + 2 * state->pagesize);

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
		lines[i] = i * nwords;
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
		words[i] = i;
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

	/* run through the chain once to clear the cache */
	benchmark((npointers + 100) / 100, state);
}


void benchmark(iter_t iterations, void *cookie)
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



