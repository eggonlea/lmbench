/*
 * par_mem.c - determine the memory hierarchy parallelism
 *
 * usage: par_mem [-L <line size>] [-M len[K|M]] [-W <warmup>] [-N <repetitions>]
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

void compute_times(struct mem_state* state, double* tlb_time, double* cache_time);


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
	int	i, j, k, l, len;
	int	c;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	print_cost = 0;
	int	maxlen = 32 * 1024 * 1024;
	double	baseline, max_load_parallelism, load_parallelism;
	result_t **results, *r_save;
	struct mem_state state;
	char   *usage = "[-c] [-L <line size>] [-M len[K|M]] [-W <warmup>] [-N <repetitions>]\n";

	state.line = getpagesize() / 8;
	state.pagesize = getpagesize();

	while (( c = getopt(ac, av, "cL:M:W:N:")) != EOF) {
		switch(c) {
		case 'c':
			print_cost = 1;
			break;
		case 'L':
			state.line = atoi(optarg);
			if (state.line < sizeof(char*))
				state.line = sizeof(char*);
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

	results = (result_t**)malloc(MAX_MEM_PARALLELISM * sizeof(result_t*));
	for (i = 0; i < MAX_MEM_PARALLELISM; ++i) {
		results[i] = (result_t*)malloc(sizeof(result_t));
	}

	for (i = MAX_MEM_PARALLELISM * state.line; i <= maxlen; i<<=1) { 
		state.len = i;
		r_save = get_results();
		
		for (k = 0; k < MAX_MEM_PARALLELISM; ++k) {
			insertinit(results[k]);
		}

		for (j = 0; j < TRIES; ++j) {
			for (k = 0; k < MAX_MEM_PARALLELISM; ++k) {
				state.width = k + 1;
				benchmp(mem_initialize, mem_benchmarks[k], mem_cleanup, 
					0, 1, warmup, repetitions, &state);
				insertsort(gettime(), state.width * get_n(), results[k]);
			}
		}
		set_results(results[0]);
		baseline = (double)gettime() / (double)get_n();
		max_load_parallelism = 1.;

		for (k = 1; k < MAX_MEM_PARALLELISM; ++k) {
			set_results(results[k]);
			load_parallelism = baseline;
			load_parallelism /= (double)gettime() / (double)get_n();
			if (load_parallelism > max_load_parallelism) {
				max_load_parallelism = load_parallelism;
			}
		}

		fprintf(stderr, "%.6f %.2f\n", 
			state.len / (1000. * 1000.), max_load_parallelism);

		set_results(r_save);
	}

	return(0);
}


