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
	int	maxlen;
	double	latency;
	double	variation;
	double	ratio;
};

int	find_cache(int start, int n, struct cache_results* p);
int	collect_data(int start, int line, int maxlen, 
		int warmup, int repetitions, struct cache_results** pdata);
int	search(int left, int right, int line, 
		int warmup, int repetitions, struct cache_results* p);
int	collect_sample(int line, 
		int warmup, int repetitions, struct cache_results* p);
double	measure(int size, int line, int maxlen, 
		int warmup, int repetitions, double* variation);

#define THRESHOLD 1.5

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
	int	c;
	int	i, n, start, level, prev;
	int	line;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	print_cost = 0;
	int	maxlen = 32 * 1024 * 1024;
	double	par;
	char   *usage = "[-c] [-L <line size>] [-M len[K|M]] [-W <warmup>] [-N <repetitions>]\n";
	struct cache_results* r;
	struct mem_state state;

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

	n = collect_data(512, line, maxlen, warmup, repetitions, &r);

	for (start = 0, prev = -1, level = 1; 
	     (i = find_cache(start, n, r)) >= 0; 
	     ++level, start = i + 1, prev = i) 
	{
		/* 
		 * performance is not greatly improved over main memory,
		 * so it is likely not a cache boundary
		 */
		if (r[i].latency / r[n-1].latency > 0.5) break;

		/* 
		 * is cache boundary "legal"? (e.g. 2^N or 1.5*2^N) 
		 * cache sizes are "never" 1.25*2^N or 1.75*2^N
		 */
		for (c = r[i].len; c > 0x7; c >>= 1)
			;
		if (c == 5 || c == 7) {
			i++;
			if (i >= n) break;
		}

		/* Compute line size for cache */
		state.width = 1;
		state.len = r[i].len;
		state.maxlen = r[i].len;
		state.pagesize = getpagesize();
		state.line = line_find(2*r[i].len, warmup, repetitions, &state);
		if (state.line == 0) state.line = line;

		/* Compute memory parallelism for cache */
		par = par_mem(r[i].len/2, warmup, repetitions, &state);

		if (prev >= 0) {
			for (prev++; prev < i && r[prev].latency < 0.; prev++)
				;
		}

		fprintf(stderr, 
		    "L%d cache: %d bytes %.2f nanoseconds %d linesize %.2f parallelism\n",
		    level, r[i].len, r[prev >= 0 ? prev : 0].latency, state.line, par);
	}
	fprintf(stderr, "Memory latency: %.2f nanoseconds\n", r[n-1].latency);

	exit(0);
}

int
find_cache(int start, int n, struct cache_results* p)
{
	int	i, j;
	double	max = -1.;

	for (i = start; i < n; ++i) {
		if (p[i].latency < 0.) continue;
		if (p[i].ratio > max) {
			j = i;
			max = p[i].ratio;
		} else if (max > THRESHOLD) {
			return j;
		}
	}
	return -1;
}

int
collect_data(int start, int line, int maxlen, 
	     int warmup, int repetitions, struct cache_results** pdata)
{
	int	i;
	int	samples;
	int	idx;
	int	len = start;
	int	incr = start / 4;
	double	latency;
	double	variation;
	struct mem_state state;
	struct cache_results* p;


	*pdata = (struct cache_results*)malloc(sizeof(struct cache_results));

	/* count the (maximum) number of samples to take */
	for (len = start, incr = start / 4, samples = 0; len <= maxlen; incr<<=1) {
		for (i = 0; i < 4 && len <= maxlen; ++i, len += incr)
			samples++;
	}
	p = (struct cache_results*)
		malloc(samples * sizeof(struct cache_results));
	*pdata = p;
	

	/* initialize the data */
	for (len = start, incr = start / 4, idx = 0; len <= maxlen; incr<<=1) {
		for (i = 0; i < 4 && len <= maxlen; ++i, ++idx, len += incr) {
			p[idx].len = len;
			p[idx].maxlen = maxlen;
			p[idx].latency = -1.;
			p[idx].ratio = -1.;
		}
	}

	collect_sample(line, warmup, repetitions, &p[0]);
	while (collect_sample(line, warmup, repetitions, &p[samples-1]) == 0) {
		--samples;
	}
	search(0, samples - 1, line, warmup, repetitions, p);

	/**/
	fprintf(stderr, "%10.10s %10.10s %10.10s %10.10s\n", "mem size", "latency", "variation", "ratio");
	for (idx = 0; idx < samples; ++idx) {
		if (p[idx].latency < 0.) continue;
		fprintf(stderr, 
			"%10.6f %10.5f %10.5f %10.5f\n", 
			p[idx].len / (1000. * 1000.), 
			p[idx].latency, 
			p[idx].variation, 
			p[idx].ratio);
	}
	/**/

	return samples;
}

int
search(int left, int right, 
       int line, int warmup, int repetitions, struct cache_results* p)
{
	int	middle = left + (right - left) / 2;

	if (p[left].latency != 0.0) {
		p[left].ratio = p[right].latency / p[left].latency;
		/* we probably have a bad data point, so re-test it */
fprintf(stderr, "search(%d, %d, ...): ratio[%d]=%f\n", p[left].len, p[right].len, p[left].len, p[left].ratio);
		if (p[left].ratio < 0.98) {
			fprintf(stderr, "search: retesting %d\n", p[left].len);
			p[left].ratio = -1.;
			collect_sample(line, warmup, repetitions, &p[left]);
			if (p[left].latency != 0.0)
				p[left].ratio = p[right].latency / p[left].latency;
		}
	}

	if (middle == left || middle == right)
		return 0;

	if (p[left].ratio > 1.1 || p[left].ratio < 0.97) {
		collect_sample(line, warmup, repetitions, &p[middle]);
		search(middle, right, line, warmup, repetitions, p);
		search(left, middle, line, warmup, repetitions, p);
	}
}

int
collect_sample(int line, int warmup, int repetitions, struct cache_results* p)
{
	int	len, maxlen;
	struct mem_state state;

	len = p->len;
	maxlen = p->maxlen;

	state.width = 1;
	state.pagesize = getpagesize();
	state.line = line;
	state.len = len;
	state.maxlen = len;

	p->latency = measure(len, line, maxlen, warmup,
			     repetitions, &p->variation);

	if (p->latency > 0) return 1;
	return 0;
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
	if (time != 0.)
		*variation = median / time;
	else
		*variation = -1.0;

	set_results(r_save);
	free(r);

	/*
	fprintf(stderr, "%.6f %.2f %.2f\n", state.len / (1000. * 1000.), time, *variation);
	/**/

	return time;
}

