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

void initialize(void* cookie);
void benchmark(uint64 iterations, void* cookie);
void cleanup(void* cookie);

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
 * 2) Cache lines are smaller than 1/4 a page size
 * 3) Pages are an even multiple of cache lines
 */
int
main(int ac, char **av)
{
	int	i, l;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	c;
	int	maxline = getpagesize() / (4 * sizeof(char*));
	double* times;
	struct _state state;
	char   *usage = "[-W <warmup>] [-N <repetitions>][-M len[K|M]]\n";

        state.len = 32 * 1024 * 1024;
	state.line = 2;
	state.pagesize = getpagesize();

	while (( c = getopt(ac, av, "M:W:N:")) != EOF) {
		switch(c) {
		case 'M':
			l = strlen(optarg);
			if (optarg[l-1] == 'm' || optarg[l-1] == 'M') {
				state.len = 1024 * 1024;
				optarg[l-1] = 0;
			} else if (optarg[l-1] == 'k' || optarg[l-1] == 'K') {
				state.len = 1024;
				optarg[l-1] = 0;
			}
			state.len *= atoi(optarg);
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

/*
	state.line = 2;
	initialize(&state);
	benchmark(1, &state);
	cleanup(&state);
	exit(0);
/**/	

	times = (double*)malloc(maxline * sizeof(double));

	for (i = 2; i < maxline; i<<=1) {
		state.line = i;
		benchmp(initialize, benchmark, cleanup, 0, 1, 
			warmup, repetitions, &state);

		/* We want to get to nanoseconds / load. */
		times[i] = (1000. * (double)gettime()) / (100. * (double)get_n());

		/**/
		fprintf(stderr, "%d %.5f\n", state.line * sizeof(char*), times[i]); 
		/**/
		
		if (i > 2 && times[i] / times[2] > 1.25) {
			fprintf(stderr, "cache line size: %d bytes\n", l);
			break;
		}

		l = state.line * sizeof(char*);
	}

	return(0);
}

void
initialize(void* cookie)
{
	int i, j, k, last, line, nlines, npages;
	unsigned int r;
	char ***pages;
	int    *lines;
	struct _state* state = (struct _state*)cookie;
	register char *p = 0 /* lint */;

	last = state->line - 1;
	line = state->line * sizeof(char*);
	nlines = 4;
	npages = state->len / state->pagesize;

	state->p = state->addr = (char*)malloc(state->len + state->pagesize);
	pages = (char***)malloc(npages * sizeof(char**));
	lines = (int*)malloc(nlines * sizeof(int));

	if (state->addr == NULL || lines == NULL || pages == NULL) {
		exit(0);
	}

	srand(getpid());

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

	/* layout the sequence of line accesses */
	for (i = 0; i < nlines; ++i) {
		lines[i] = i * state->pagesize / (nlines * sizeof(char*));
	}
	
	/* randomize the line sequences */
	for (i = nlines - 1; i > 0; --i) {
		int l;
		r = (r << 1) ^ (rand() >> 4);
		l = lines[r % i];
		lines[r % i] = lines[i];
		lines[i] = l;
	}

	/* new setup runs through the lines */
	for (i = 0; i < npages; ++i) {
		/* sequence through the last word of line j */
		for (j = 0; j < nlines - 1; ++j) {
			pages[i][lines[j]+last] = (char*)(pages[i] + lines[j+1] + last);
		}

		/* jump back to first word on first line of the page */
		pages[i][lines[j]+last] = (char*)(pages[i] + lines[0]);

		/* sequence through first word of line j */
		for (j = 0; j < nlines - 1; ++j) {
			pages[i][lines[j]] = (char*)(pages[i] + lines[j+1]);
		}

		/* jump to the last word of the first line on next page */
		if (i < npages - 1)
			pages[i][lines[j]] = (char*)(pages[i+1] + lines[0] + last);
		else
			pages[i][lines[j]] = (char*)(pages[0] + lines[0] + last);
	}
	free(lines);
	free(pages);
	
	/* now, run through the chain once to clear the cache */
	benchmark((8 * npages + 100) / 100, state);
}


void benchmark(uint64 iterations, void *cookie)
{
	struct _state* state = (struct _state*)cookie;
	static char **p_save = NULL;
	register char **p = p_save ? p_save : (char**)state->p;

	while (iterations-- > 0) {
		HUNDRED;
	}

	use_pointer((void *)p);
	p_save = p;
}

void cleanup(void* cookie)
{
	struct _state* state = (struct _state*)cookie;
	free(state->addr);
	state->addr = NULL;
	state->p = NULL;
}



