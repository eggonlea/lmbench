/*
 * lat_mem_rd.c - measure memory load latency
 *
 * usage: lat_mem_rd [-P <parallelism>] [-W <warmup>] [-N <repetitions>] [-t] size-in-MB [stride ...]
 *
 * Copyright (c) 1994 Larry McVoy.  
 * Copyright (c) 2003, 2004 Carl Staelin.
 *
 * Distributed under the FSF GPL with additional restriction that results 
 * may published only if:
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id: s.lat_mem_rd.c 1.13 98/06/30 16:13:49-07:00 lm@lm.bitmover.com $\n";

#include "bench.h"
#define STRIDE  (512/sizeof(char *))
#define	LOWER	512
void	loads(size_t len, size_t range, size_t stride, 
	      int parallel, int warmup, int repetitions);
size_t	step(size_t k);
void	initialize(iter_t iterations, void* cookie);

benchmp_f	fpInit = initialize;

void
base_initialize(iter_t iterations, void* cookie)
{
	int	nwords, nlines, nbytes, npages, nmpages;
	size_t *pages;
	size_t *lines;
	size_t *words;
	struct mem_state* state = (struct mem_state*)cookie;
	register char *p = 0 /* lint */;

	if (iterations) return;

	state->initialized = 0;

	state->maxlen = state->len;
	nbytes = state->len;
	nwords = state->line / sizeof(char*);
	nlines = state->pagesize / state->line;
	npages = (nbytes + state->pagesize - 1) / state->pagesize;
	nmpages= (state->maxlen + state->pagesize - 1) / state->pagesize;

	srand(getpid());

	words = NULL;
	lines = NULL;
	pages = permutation(nmpages, state->pagesize);
	p = state->addr = (char*)malloc(state->maxlen + 2 * state->pagesize);

	state->nwords = nwords;
	state->nlines = nlines;
	state->npages = npages;
	state->lines = lines;
	state->pages = pages;
	state->words = words;

	if (state->addr == NULL) {
		return;
	}

	if ((unsigned long)p % state->pagesize) {
		p += state->pagesize - (unsigned long)p % state->pagesize;
	}
	state->base = p;
	state->initialized = 1;
}

void
initialize(iter_t iterations, void* cookie)
{
	struct mem_state* state = (struct mem_state*)cookie;
	size_t	i;
	size_t	range = state->len;
	size_t	stride = state->line;
	char*	addr;

	base_initialize(iterations, cookie);
	addr = state->base;

	/*
	 * Create a circular list of pointers using a simple striding
	 * algorithm.  
	 * 
	 * This access pattern corresponds to many array/matrix
	 * algorithms.  It should be easily and correctly predicted
	 * by any decent hardware prefetch algorithm.
	 */
	for (i = stride; i < range; i += stride) {
		*(char **)&addr[i - stride] = (char*)&addr[i];
	}
	*(char **)&addr[i - stride] = (char*)&addr[0];
	state->p[0] = addr;
}

void
initialize_thrash(iter_t iterations, void* cookie)
{
	struct mem_state* state = (struct mem_state*)cookie;
	int	npages = (state->len + state->pagesize - 1) / state->pagesize;
	size_t *pages;
	size_t	i;
	size_t	j;
	size_t	cur;
	size_t	next;
	size_t	start;
	size_t	range = state->len;
	size_t	stride = state->line;
	char*	addr;

	base_initialize(iterations, cookie);
	addr = state->base;

	/*
	 * Create a circular list of pointers with a random access
	 * pattern.
	 *
	 * This stream corresponds more closely to linked list
	 * memory access patterns.  For large data structures each
	 * access will likely cause both a cache miss and a TLB miss.
	 * 
	 * Access a different page each time.  This will eventually
	 * cause a tlb miss each page.  It will also cause maximal
	 * thrashing in the cache between the user data stream and
	 * the page table entries.
	 */
	pages = state->pages;
	cur = pages[0];
	for (i = 0; i < state->nlines; ++i) {
	    for (j = 0; j < state->npages; ++j) {
		if (j < state->npages - 1) {
		    next = pages[j + 1] + ((i + j + 1) % state->nlines) * state->line;
		} else {
		    next = pages[0] + ((i + 1) % state->nlines) * state->line;
		}
		*(char **)&addr[cur] = (char*)&addr[next];
		cur = next;
	    }
	}
	state->p[0] = (char*)&addr[pages[0]];
}

int
main(int ac, char **av)
{
	int	i;
	int	c;
	int	parallel = 1;
	int	warmup = 0;
	int	repetitions = TRIES;
        size_t	len;
	size_t	range;
	size_t	stride;
	char   *usage = "[-P <parallelism>] [-W <warmup>] [-N <repetitions>] [-t] len [stride...]\n";

	while (( c = getopt(ac, av, "tP:W:N:")) != EOF) {
		switch(c) {
		case 't':
			fpInit = initialize_thrash;
			break;
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
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}
	if (optind == ac) {
		lmbench_usage(ac, av, usage);
	}

        len = atoi(av[optind]) * 1024 * 1024;

	if (optind == ac - 1) {
		fprintf(stderr, "\"stride=%d\n", STRIDE);
		for (range = LOWER; range <= len; range = step(range)) {
			loads(len, range, STRIDE, parallel, 
			      warmup, repetitions);
		}
	} else {
		for (i = optind + 1; i < ac; ++i) {
			stride = bytes(av[i]);
			fprintf(stderr, "\"stride=%d\n", stride);
			for (range = LOWER; range <= len; range = step(range)) {
				loads(len, range, stride, parallel, 
				      warmup, repetitions);
			}
			fprintf(stderr, "\n");
		}
	}
	return(0);
}

#define	ONE	p = (char **)*p;
#define	FIVE	ONE ONE ONE ONE ONE
#define	TEN	FIVE FIVE
#define	FIFTY	TEN TEN TEN TEN TEN
#define	HUNDRED	FIFTY FIFTY


void
benchmark_loads(iter_t iterations, void *cookie)
{
	struct mem_state* state = (struct mem_state*)cookie;
	register char **p = (char**)state->p[0];
	register size_t i;
	register size_t count = state->len / (state->line * 100) + 1;

	while (iterations-- > 0) {
		for (i = 0; i < count; ++i) {
			HUNDRED;
		}
	}

	use_pointer((void *)p);
	state->p[0] = (char*)p;
}


void
loads(size_t len, size_t range, size_t stride, 
	int parallel, int warmup, int repetitions)
{
	double result;
	size_t count;
	struct mem_state state;

	state.width = 1;
	state.len = range;
	state.maxlen = len;
	state.line = stride;
	state.pagesize = getpagesize();
	count = 100 * (state.len / (state.line * 100) + 1);

#if 0
	(*fpInit)(0, &state);
	fprintf(stderr, "loads: after init\n");
	(*benchmark_loads)(1, &state);
	fprintf(stderr, "loads: after benchmark\n");
	mem_cleanup(0, &state);
	fprintf(stderr, "loads: after cleanup\n");
#endif

	/*
	 * Now walk them and time it.
	 */
	benchmp(fpInit, benchmark_loads, mem_cleanup, 
		100000, parallel, warmup, repetitions, &state);

	/* We want to get to nanoseconds / load. */
	save_minimum();
	result = (1000. * (double)gettime()) / (double)(count * get_n());
	fprintf(stderr, "%.5f %.3f\n", range / (1024. * 1024.), result);

}

size_t
step(size_t k)
{
	if (k < 1024) {
		k = k * 2;
        } else if (k < 4*1024) {
		k += 1024;
        } else if (k < 32*1024) {
		k += 2048;
        } else if (k < 64*1024) {
		k += 4096;
        } else if (k < 128*1024) {
		k += 8192;
        } else if (k < 256*1024) {
		k += 16384;
        } else if (k < 512*1024) {
		k += 32*1024;
	} else if (k < 4<<20) {
		k += 512 * 1024;
	} else if (k < 8<<20) {
		k += 1<<20;
	} else if (k < 20<<20) {
		k += 2<<20;
	} else {
		k += 10<<20;
	}
	return (k);
}
