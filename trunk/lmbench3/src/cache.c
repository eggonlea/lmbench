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
	int	maxlen;
	int	line;
	int	pagesize;
};

int find_cache(int start, int line, 
	       int maxlen, int warmup, int repetitions, double* time);
double measure(int size, int line, int maxlen, int warmup, int repetitions);
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
	int	line, l1_cache, l2_cache, c;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	print_cost = 0;
	int	maxlen = 32 * 1024 * 1024;
	double	l1_time, l2_time, mem_time;
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

	l2_cache = maxlen;

	l1_cache = find_cache(512, line, maxlen, warmup, repetitions, &l1_time);

	if (l1_cache < maxlen) {
		l2_cache = find_cache(l1_cache, line, 
				      maxlen, warmup, repetitions, &l2_time);
	}

	mem_time = measure(maxlen, line, maxlen, warmup, repetitions);

	if (l1_cache < maxlen) {
		fprintf(stderr, "L1 cache: %d bytes %.2f nanoseconds\n", 
			l1_cache, l1_time);
	}

	if (l2_cache < maxlen) {
		fprintf(stderr, "L2 cache: %d bytes %.2f nanoseconds\n", 
			l2_cache, l2_time);
	}

	fprintf(stderr, "Memory latency: %.2f nanoseconds\n", mem_time);

	return(0);
}

int
find_cache(int start, int line, 
	   int maxlen, int warmup, int repetitions, double *time)
{
	int	i, len, maxsize, incr;
	double	baseline = -1.;
	double	current;

	/* get the baseline access time */
	i = 2 * start;

search:
	for (; i <= maxlen; i<<=1) {
		current = measure(i, line, (2*i) < maxlen ? (2*i) : maxlen, 
				  warmup, repetitions);

		if (baseline < 0.)
			baseline = current;

		/* we have crossed a cache boundary */
		if (current / baseline > THRESHOLD) {
			break;
		}
	}
	if (i >= maxlen)
		return i;

	incr = i>>3;
	maxsize = i;
	i>>=1;
	len = i;

	/**/
	fprintf(stderr, "find_cache: baseline=%.2f, current=%.2f, ratio=%.2f, i=%d\n", baseline, current, current/baseline, i);
	/**/

	for (i; i <= maxsize; i+=incr) {
		current = measure(i, line, (2*i) < maxlen ? (2*i) : maxlen, 
				  warmup, repetitions);

		/* we have crossed a cache boundary */
		if (current / baseline > THRESHOLD)
			break;
		len = i;
	}
	if (len >= maxsize) {
		i = len;
		goto search;
	}
	
	*time = baseline;
	return len;
}

double
measure(int size, int line, int maxlen, int warmup, int repetitions)
{
	int	i;
	double	time;
	result_t *r, *r_save;
	struct _state state;

	state.len = size;
	state.maxlen = maxlen;
	state.line = line;
	state.pagesize = getpagesize();

	r_save = get_results();
	r = (result_t*)malloc(sizeof_result(repetitions + 1));
	insertinit(r);

	/* 
	 * Get the system to allocate more pages than we need, and
	 * we will choose a random subset.  This is to try and improve
	 * accuracy on systems without page coloring.  We do a sort
	 * of randomized page coloring...
	 */
	for (i = 0; i < repetitions - 1; ++i) {
		benchmp(initialize, benchmark, cleanup, 0, 1, 
				warmup, TRIES, &state);
		save_minimum();
		insertsort(gettime(), get_n(), r);
	}

	state.maxlen = size;
	benchmp(initialize, benchmark, cleanup, 0, 1, warmup, 5, &state);
	save_minimum();
	insertsort(gettime(), get_n(), r);

	set_results(r);
	save_minimum();

	/* We want nanoseconds / load. */
	time = (1000. * (double)gettime()) / (100. * (double)get_n());

	/**/
	fprintf(stderr, "%.6f %.2f\n", state.len / (1000. * 1000.), time);
	print_results();
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
	npages = (state->maxlen + state->pagesize) / state->pagesize;

	words = (int*)malloc(nwords * sizeof(int));
	lines = (int*)malloc(nlines * sizeof(int));
	pages = (char***)malloc(npages * sizeof(char**));
	state->p = state->addr = (char*)malloc(state->maxlen + 2 * state->pagesize);

	if ((unsigned long)state->p % state->pagesize) {
		state->p += state->pagesize - (unsigned long)state->p % state->pagesize;
	}

	if (state->addr == NULL || pages == NULL) {
		exit(0);
	}
	
	srand((int)now() ^ getpid() ^ (getpid()<<16));

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
	for (i = 0, k = 0; i < npages && k < npointers; ++i) {
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
/*
fprintf(stderr, "initialize: i=%d, k=%d, npointers=%d, len=%d, line=%d\n", i, k, npointers, state->len, state->line);
fprintf(stderr, "pages={");
for (j = 0; j < i; ++j) {
	u_long base = (u_long)state->addr / state->pagesize;
	u_long page = (u_long)pages[j] / state->pagesize;
	fprintf(stderr, "%lu", page - base);
	if (j < i - 1)
		fprintf(stderr, ", ");
}
fprintf(stderr, "}\n");
/**/

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



