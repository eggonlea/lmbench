/*
 * lat_pagefault.c - time a page fault in
 *
 * Usage: lat_pagefault [-W <warmup>] [-N <repetitions>] file 
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

#define	CHK(x)	if ((x) == -1) { perror("x"); exit(1); }

typedef struct _state {
	int fd;
	int size;
	int npages;
	int page;
	char* file;
	char* where;
	char** pages;
} state_t;

void	initialize(void *cookie);
void	cleanup(void *cookie);
void	benchmark(iter_t iterations, void * cookie);

int
main(int ac, char **av)
{
#ifdef	MS_INVALIDATE
	int parallel = 1;
	int warmup = 0;
	int repetitions = TRIES;
	int c;
	struct _state state;
	char buf[2048];
	char* usage = "[-W <warmup>] [-N <repetitions>] file\n";

	while (( c = getopt(ac, av, "W:N:")) != EOF) {
		switch(c) {
		case 'P':
			/*
			 * don't allow this for now.  Not sure how to
			 * manage parallel processes to ensure that they
			 * each have to pagefault on each page access
			 */
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
	if (optind != ac - 1 ) {
		lmbench_usage(ac, av, usage);
	}
	
	state.file = av[optind];
	benchmp(initialize, benchmark, cleanup, 0, parallel, 
		warmup, repetitions, &state);
	sprintf(buf, "Pagefaults on %s", state.file);
	micro(buf, get_n());
#endif
}

void
initialize(void* cookie)
{
	int 		i, npages, pagesize;
	unsigned int	r;
	char		*p;
	struct stat 	sbuf;
	state_t 	*state = (state_t *) cookie;

	CHK(state->fd = open(state->file, 0));
	CHK(fstat(state->fd, &sbuf));

	srand(getpid());
	pagesize = getpagesize();
	state->size = sbuf.st_size;
	state->size -= state->size % pagesize;
	state->npages = state->size / pagesize;
	state->pages = (char**)malloc(state->npages * sizeof(char*));
	state->page = 0;

	if (state->size < 1024*1024) {
		fprintf(stderr, "lat_pagefault: %s too small\n", state->file);
		exit(1);
	}
	state->where = mmap(0, state->size, PROT_READ, MAP_SHARED, state->fd, 0);

	/* first, layout the sequence of page accesses */
	p = state->where;
	for (i = 0; i < state->npages; ++i) {
		state->pages[i] = (char*)p;
		p += pagesize;
	}

	/* randomize the page sequences */
	r = (rand() << 15) ^ rand();
	for (i = state->npages - 1; i > 0; --i) {
		char *l;
		r = (r << 1) ^ (rand() >> 4);
		l = state->pages[r % i];
		state->pages[r % i] = state->pages[i];
		state->pages[i] = l;
	}

#ifdef	MS_INVALIDATE
	if (msync(state->where, state->size, MS_INVALIDATE) != 0) {
		perror("msync");
		exit(1);
	}
#endif
}

void
cleanup(void* cookie)
{	
	state_t *state = (state_t *) cookie;

	munmap(state->where, state->size);
	free(state->pages);
}

void
benchmark(iter_t iterations, void* cookie)
{
	int	sum = 0;
	state_t *state = (state_t *) cookie;

	while (iterations-- > 0) {
		sum += *(state->pages[state->page++]);
		if (state->page >= state->npages) {
			state->page = 0;
#ifdef	MS_INVALIDATE
			if (msync(state->where, state->size, MS_INVALIDATE) != 0) {
				perror("msync");
				exit(1);
			}
#endif
		}
	}
	use_int(sum);
}

