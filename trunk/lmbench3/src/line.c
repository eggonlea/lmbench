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

double	line_test(int l, int warmup, int repetitions, struct mem_state* state);
int	line_find(int len, int warmup, int repetitions, struct mem_state* state);

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
	struct mem_state state;
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
line_find(int len, int warmup, int repetitions, struct mem_state* state)
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
line_test(int len, int warmup, int repetitions, struct mem_state* state)
{
	int	i;
	double	t;
	result_t r, *r_save;

	state->line = len;
	r_save = get_results();
	insertinit(&r);
	for (i = 0; i < 5; ++i) {
		benchmp(line_initialize, mem_benchmark_0, mem_cleanup, 
			0, 1, warmup, repetitions, state);
		insertsort(gettime(), get_n(), &r);
	}
	set_results(&r);
	t = 10. * (double)gettime() / (double)get_n();
	set_results(r_save);
	
	/*
	fprintf(stderr, "%d\t%.5f\t%d\n", len * sizeof(char*), t, state->len); 
	/**/

	return (t);
}




