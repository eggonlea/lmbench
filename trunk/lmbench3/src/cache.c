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

struct cache_results {
	int	len;
	int	line;
	double	latency;
	double	variation;
	double	parallelism;
};


int collect_data(int start, int line, int maxlen, 
		 int warmup, int repetitions, struct cache_results** pdata);
int find_cache(int start, int line, 
	       int maxlen, int warmup, int repetitions, double* time);
double measure(int size, int line, int maxlen, int warmup, 
	       int repetitions, double* variation);

#define THRESHOLD 2.5

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
	int	line, l1_cache, l2_cache, l3_cache, c;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	print_cost = 0;
	int	maxlen = 32 * 1024 * 1024;
	double	l1_time, l2_time, l3_time, mem_time, variation;
	char   *usage = "[-c] [-L <line size>] [-M len[K|M]] [-W <warmup>] [-N <repetitions>]\n";
	int	n;
	struct cache_results* r;

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

#if 1
	n = collect_data(512, line, maxlen, warmup, repetitions, &r);
#else
	l1_cache = find_cache(512, line, maxlen, warmup, repetitions, &l1_time);
	l2_cache = maxlen;
	l3_cache = maxlen;

	if (l1_cache < maxlen) {
		int	start;
		for (start = 512; start < l1_cache; start<<=1)
			;
		l2_cache = find_cache(start, line, 
				      maxlen, warmup, repetitions, &l2_time);
	}

	if (l2_cache < maxlen) {
		int	start;
		for (start = 512; start < l2_cache; start<<=1)
			;
		l3_cache = find_cache(start, line, 
				      maxlen, warmup, repetitions, &l3_time);
	}

	mem_time = measure(maxlen, line, maxlen, warmup, repetitions, &variation);

	if (l1_cache < maxlen) {
		fprintf(stderr, "L1 cache: %d bytes %.2f nanoseconds\n", 
			l1_cache, l1_time);
	}

	if (l2_cache < maxlen) {
		fprintf(stderr, "L2 cache: %d bytes %.2f nanoseconds\n", 
			l2_cache, l2_time);
	}

	if (l3_cache < maxlen) {
		fprintf(stderr, "L3 cache: %d bytes %.2f nanoseconds\n", 
			l3_cache, l3_time);
	}

	fprintf(stderr, "Memory latency: %.2f nanoseconds\n", mem_time);
#endif
	return(0);
}

int
collect_data(int start, int line, int maxlen, 
	     int warmup, int repetitions, struct cache_results** pdata)
{
	int	i;
	int	idx = 0;
	int	len = start;
	int	incr = start / 4;
	double	latency;
	double	variation;
	struct mem_state state;
	struct cache_results* p;

	state.width = 1;
	state.pagesize = getpagesize();

	*pdata = (struct cache_results*)malloc(sizeof(struct cache_results));

	for (len = start, incr = start / 4; len <= maxlen; incr<<=1) {
		for (i = 0; i < 4 && len <= maxlen; ++i, ++idx, len += incr) {
			state.line = sizeof(char*);
			state.len = len;
			state.maxlen = len;

			*pdata = (struct cache_results*)
				realloc(*pdata, (idx+1) * sizeof(struct cache_results));
			p = &((*pdata)[idx]);

			p->len = len;
			p->line = line_find(len, 
					    warmup, repetitions, &state);

			p->latency = measure(len, line, maxlen, warmup,
					     repetitions, &p->variation);

			p->parallelism = par_mem(len, 
						 warmup, repetitions, &state);

			/**/
			fprintf(stderr, "%.6f\t%d\t%.5f\t%.5f\t%.5f\n", 
				p->len / (1000. * 1000.), p->line, 
				p->latency, p->variation, p->parallelism);
			/**/
		}
	}
	return idx;
}

int
find_cache(int start, int line, 
	   int maxlen, int warmup, int repetitions, double *time)
{
	int	i, len, maxsize, incr;
	double	baseline = -1.;
	double	current;
	double	max_variation, variation;

	/* get the baseline access time */
	i = 2 * start;

search:
	for (; i <= maxlen; i<<=1) {
		current = measure(i, line, (2*i) < maxlen ? (2*i) : maxlen, 
				  warmup, repetitions, &max_variation);

		if (baseline < 0.)
			baseline = current;

		/* we have crossed a cache boundary */
		if (current / baseline > THRESHOLD) {
			break;
		}
		len = i;
	}
	if (i >= maxlen)
		return i;

	incr = i>>3;
	maxsize = i;
	i = (i>>1) + incr;

	/*
	fprintf(stderr, "find_cache: baseline=%.2f, current=%.2f, ratio=%.2f, i=%d\n", baseline, current, current/baseline, i);
	/**/

	for (i; i <= maxsize; i+=incr) {
		current = measure(i, line, (2*i) < maxlen ? (2*i) : maxlen, 
				  warmup, repetitions, &variation);

		/* we have crossed a cache boundary */
		if (current / baseline > THRESHOLD)
			break;

		if (variation > max_variation) {
			len = i;
			max_variation = variation;
		}
	}
	if (len >= maxsize) {
		i = len;
		goto search;
	}
	
	*time = baseline;
	return len;
}

double
measure(int size, int line, int maxlen, int warmup, 
	int repetitions, double* variation)
{
	int	i;
	double	time, median;
	result_t *r, *r_save;
	struct mem_state state;

	state.width = 1;
	state.len = size;
	state.maxlen = size;
	state.line = line;
	state.pagesize = getpagesize();

	r_save = get_results();
	r = (result_t*)malloc(sizeof_result(repetitions));
	insertinit(r);

	for (i = 0; i < repetitions; ++i) {
		benchmp(mem_initialize, mem_benchmark_0, mem_cleanup, 
			0, 1, warmup, TRIES, &state);
		save_minimum();
		insertsort(gettime(), get_n(), r);
		state.maxlen = maxlen;
	}

	set_results(r);

	median = (1000. * (double)gettime()) / (100. * (double)get_n());

	save_minimum();
	time = (1000. * (double)gettime()) / (100. * (double)get_n());

	/* Are the results stable, or do they vary? */
	*variation = median / time;

	set_results(r_save);
	free(r);

	/*
	fprintf(stderr, "%.6f %.2f %.2f\n", state.len / (1000. * 1000.), time, *variation);
	/**/

	return time;
}

