/*
 * line.c - guess the cache line size
 *
 * usage: line
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

double	line_test(int l, int warmup, int repetitions, struct _state* state);
int	line_find(int len, int warmup, int repetitions, struct _state* state);
void	line_initialize(void* cookie);
void	line_benchmark(iter_t iterations, void* cookie);
void	line_cleanup(void* cookie);

#define	ONE	p = (char **)*p; 
/*	fprintf(stderr, "0x%x:\t%d\t%d\t%d\n", (unsigned int)p, (unsigned int)p / state->pagesize, (((unsigned int)p % state->pagesize) * 4) / state->pagesize, (unsigned int)p % state->pagesize); */
#define	FIVE	ONE ONE ONE ONE ONE
#define	TEN	FIVE FIVE
#define	FIFTY	TEN TEN TEN TEN TEN
#define	HUNDRED	FIFTY FIFTY

/*
 * Assumptions:
 *
 * 1) Cache lines are a multiple of pointer-size words
 * 2) Cache lines are no larger than 1/4 a page size
 * 3) Pages are an even multiple of cache lines
 */
int
main(int ac, char **av)
{
	int	i, j, l;
	int	find_all = 0;
	int	verbose = 0;
	int	maxlen = 32 * 1024 * 1024;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	c;
	struct _state state;
	char   *usage = "[-v] [-W <warmup>] [-N <repetitions>][-M len[K|M]]\n";

	state.line = 2;
	state.pagesize = getpagesize();

	while (( c = getopt(ac, av, "avM:W:N:")) != EOF) {
		switch(c) {
		case 'a':
			find_all = 1;
			break;
		case 'v':
			verbose = 1;
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

	if (!find_all) {
		l = line_find(maxlen, warmup, repetitions, &state);
		if (verbose) {
			printf("cache line size: %d bytes\n", l);
		} else {
			printf("%d\n", l);
		}
	} else {
		int len = 0;
		int level = 1;

		for (i = getpagesize(); i <= maxlen; i<<=1) {
			l = line_find(i, warmup, repetitions, &state);
			if ((i<<1) <= maxlen && l != 0 &&
			    (len == 0 || len != 0 && l != len)) {
				/*
				 * near edge of cache, move away from edge
				 * to get more reliable reading
				 */
				l = line_find(i<<=1, warmup, repetitions, &state);
				printf("L%d cache line size: %d bytes\n", level, l);
				level++;
			}
		}
	}

	return (0);
}

int
line_find(int len, int warmup, int repetitions, struct _state* state)
{
	int 	i, j;
	int 	l = 0;
	int	maxline = getpagesize() / (8 * sizeof(char*));
	double	t, threshold;

	state->len = len;

	threshold = .85 * line_test(maxline, warmup, repetitions, state);

	for (i = maxline>>1; i >= 2; i>>=1) {
		t = line_test(i, warmup, repetitions, state);

		if (t <= threshold) {
			return ((i<<1) * sizeof(char*));
		}
	}

	return (0);
}

double
line_test(int len, int warmup, int repetitions, struct _state* state)
{
	int	i;
	double	t;
	result_t r, *r_save;

	state->line = len;
	r_save = get_results();
	insertinit(&r);
	for (i = 0; i < 5; ++i) {
		benchmp(line_initialize, line_benchmark, line_cleanup, 
			0, 1, warmup, repetitions, state);
		insertsort(gettime(), get_n(), &r);
	}
	set_results(&r);
	t = 10. * (double)gettime() / (double)get_n();
	set_results(r_save);
	
	/**/
	fprintf(stderr, "%d\t%.5f\t%d\n", len * sizeof(char*), t, state->len); 
	/**/

	return (t);
}

void
line_initialize(void* cookie)
{
	int i, j, k, last, line, nlines, npages;
	unsigned int r;
	char ***pages;
	int    *lines;
	struct _state* state = (struct _state*)cookie;
	register char *p = 0 /* lint */;

	last = state->line - 1;
	line = state->line * sizeof(char*);
	nlines = state->pagesize / line;
	npages = state->len / state->pagesize;

	srand(getpid());

	state->p = state->addr = (char*)valloc(state->len + state->pagesize);
	pages = (char***)malloc(npages * sizeof(char**));
	lines = permutation(nlines);

	if (state->addr == NULL || lines == NULL || pages == NULL) {
		exit(0);
	}

	if ((unsigned long)state->p % state->pagesize) {
		state->p += state->pagesize - (unsigned long)state->p % state->pagesize;
	}

	/* first, layout the sequence of page accesses */
	p = state->p;
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
	state->p = (char*)(pages[0] + lines[0]);

	free(lines);
	free(pages);

	/* now, run through the chain once to clear the cache */
	line_benchmark((nlines * npages + 100) / 100, state);
}


void
line_benchmark(iter_t iterations, void *cookie)
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
line_cleanup(void* cookie)
{
	struct _state* state = (struct _state*)cookie;
	free(state->addr);
	state->addr = NULL;
	state->p = NULL;
}



