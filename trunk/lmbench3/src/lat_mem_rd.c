/*
 * lat_mem_rd.c - measure memory load latency
 *
 * usage: lat_mem_rd size-in-MB stride [stride ...]
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id: s.lat_mem_rd.c 1.13 98/06/30 16:13:49-07:00 lm@lm.bitmover.com $\n";

#include "bench.h"
/*#define N       1000000	/* Don't change this */
#define STRIDE  (512/sizeof(char *))
#define	MEMTRIES	4
#define	LOWER	512
void	loads(int len, int range, int stride);
int	step(int k);

int
main(int ac, char **av)
{
        int     len;
	int	range;
	int	stride;
	int	i;
        char   *addr;

        len = atoi(av[1]) * 1024 * 1024;

	if (av[2] == 0) {
		fprintf(stderr, "\"stride=%d\n", STRIDE);
		for (range = LOWER; range <= len; range = step(range)) {
			loads(len, range, STRIDE);
		}
	} else {
		for (i = 2; i < ac; ++i) {
			stride = bytes(av[i]);
			fprintf(stderr, "\"stride=%d\n", stride);
			for (range = LOWER; range <= len; range = step(range)) {
				loads(len, range, stride);
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

struct _state {
	char*	addr;
	int	len;
	int	range;
	int	stride;
};

void initialize_loads(void* cookie)
{
	struct _state* state = (struct _state*)cookie;
	register char **p = 0 /* lint */;
        int     i;
	int	tries = 0;
	int	result = 0x7fffffff;

        state->addr = (char *)malloc(state->len);

     	if (state->stride & (sizeof(char *) - 1)) {
		printf("lat_mem_rd: stride must be aligned.\n");
		return;
	}
	
	if (state->range < state->stride) {
		return;
	}

	/*
	 * First create a list of pointers.
	 *
	 * This used to go forwards, we want to go backwards to try and defeat
	 * HP's fetch ahead.
	 *
	 * We really need to do a random pattern once we are doing one hit per 
	 * page.
	 */
	for (i = state->range - state->stride; i >= 0; i -= state->stride) {
		char	*next;

		p = (char **)&(state->addr[i]);
		if (i < state->stride) {
			next = &(state->addr[state->range - state->stride]);
		} else {
			next = &(state->addr[i - state->stride]);
		}
		*p = next;
	}
}

void benchmark_loads(uint64 iterations, void *cookie)
{
	struct _state* state = (struct _state*)cookie;
	register char **p = (char**)state->addr;

	while (iterations-- > 0) {
		HUNDRED;
	}

	use_pointer((void *)p);
}

void cleanup_loads(void* cookie)
{
	struct _state* state = (struct _state*)cookie;
	free(state->addr);
	state->addr = NULL;
}


void
loads(int len, int range, int stride)
{
	int result;
	struct _state state;

	state.len = len;
	state.range = range;
	state.stride = stride;

	/*
	 * Now walk them and time it.
	 */
	benchmp(initialize_loads, benchmark_loads, NOCLEANUP, 0, 1, &state);

	/* We want to get to nanoseconds / load. */
	fprintf(stderr,"*****************************************\n");
	result = (int)(((uint64)1000 * gettime()) / ((uint64)100 * get_n()));
	fprintf(stderr, "%.5f %d\n", range / (1024. * 1024), result);

	benchmp(initialize_loads, benchmark_loads, NOCLEANUP, 0, 2, &state);

	/* We want to get to nanoseconds / load. */
	fprintf(stderr,"*****************************************\n");
	result = (int)(((uint64)1000 * gettime()) / ((uint64)100 * get_n()));
	fprintf(stderr, "\t%.5f %d %d\n", range / (1024. * 1024), result, 2);
}

int
step(int k)
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
