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
	int	line;
	int	mline;
	double	latency;
	double	variation;
	double	ratio;
	double	slope;
};

int	find_cache(int start, int n, struct cache_results* p);
int	collect_data(int start, int line, int maxlen, 
		     int repetitions, struct cache_results** pdata);
void	search(int left, int right, int repetitions, 
	       struct mem_state* state, struct cache_results* p);
int	collect_sample(int repetitions, struct mem_state* state, 
			struct cache_results* p);
double	measure(int size, int repetitions, 
		double* variation, struct mem_state* state);
double	remove_chunk(int i, int chunk, int npages, int* pages, 
		       int len, int repetitions, struct mem_state* state);

#define THRESHOLD 1.5

#define	FIVE(m)		m m m m m
#define	TEN(m)		FIVE(m) FIVE(m)
#define	FIFTY(m)	TEN(m) TEN(m) TEN(m) TEN(m) TEN(m)
#define	HUNDRED(m)	FIFTY(m) FIFTY(m)
#define DEREF		p = (char**)*p;

static char **addr_save = NULL;

void
mem_benchmark(iter_t iterations, void *cookie)
{
	register char **p;
	struct mem_state* state = (struct mem_state*)cookie;

	p = addr_save ? addr_save : (char**)state->p[0];
	while (iterations-- > 0) {
		HUNDRED(DEREF);
	}
	addr_save = p;
}


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
	int	i, j, n, start, level, prev, min;
	int	line = -1;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	print_cost = 0;
	int	maxlen = 32 * 1024 * 1024;
	double	par, maxpar;
	char   *usage = "[-c] [-L <line size>] [-M len[K|M]] [-W <warmup>] [-N <repetitions>]\n";
	struct cache_results* r;
	struct mem_state state;

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

	if (line <= 0) {
		state.width = 1;
		state.len = maxlen;
		state.maxlen = maxlen;
		state.pagesize = getpagesize();
		line = line_find(maxlen, warmup, repetitions, &state);
		if (line <= 0)
			line = getpagesize() / 16;
		state.line = line;
	}

	n = collect_data(512, line, maxlen, repetitions, &r);

	for (start = 0, prev = 0, level = 1; 
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

		/* locate most likely cache latency */
		for (j = prev, min = prev; j < i; ++j) {
			if (r[j].latency <= 0.) continue;
			if (abs(r[j].slope) < abs(r[min].slope))
				min = j;
		}

		/* Compute line size */
		if (i + 3 < n) {
			if (r[i+3].line <= 0 || line <= r[i+3].line) {
				state.width = 1;
				state.len = r[i+3].len;
				state.maxlen = r[i+3].maxlen;
				state.pagesize = getpagesize();
				r[i+3].line = line_find(r[i+3].len, warmup, 
							repetitions, &state);
			}
			if (0 < r[i+3].line && r[i+3].line < line)
				line = r[i+3].line;
		}

		/* Compute memory parallelism for cache */
		maxpar = par_mem(r[i].len, warmup, repetitions, &state);
		for (j = prev; j < i; ++j) {
			if (r[j].latency <= 0.) continue;
			if (r[j].len < 2 * r[prev].len)
				continue;
			par = par_mem(r[j].len, warmup, repetitions, &state);
			if (par > maxpar) {
				maxpar = par;
			}
		}

		fprintf(stderr, 
		    "L%d cache: %d bytes %.2f nanoseconds %d linesize %.2f parallelism\n",
		    level, r[i].len, r[min].latency, line, maxpar);
	}

	/* Compute memory parallelism for main memory */
	j = n - 1;
	for (i = n - 1; i >= 0; i--) {
		if (r[i].latency < 0.) continue;
		if (r[i].latency > 0.99 * r[n-1].latency)
			j = i;
	}
	par = par_mem(r[j].len, warmup, repetitions, &state);

	fprintf(stderr, "Memory latency: %.2f nanoseconds %.2f parallelism\n",
		r[n-1].latency, par);

	exit(0);
}

int
find_cache(int start, int n, struct cache_results* p)
{
	int	i, j, prev;
	double	max = -1.;

	for (prev = (start == 0 ? start : start - 1); prev > 0; prev--) {
		if (p[prev].ratio > 0.0) break;
	}

	for (i = start, j = -1; i < n; ++i) {
		if (p[i].latency < 0.) continue;
		if (p[prev].ratio <= p[i].ratio && p[i].ratio > max) {
			j = i;
			max = p[i].ratio;
		} else if (p[i].ratio < max && THRESHOLD < max) {
			return j;
		}
		prev = i;
	}
	return -1;
}

int
collect_data(int start, int line, int maxlen, 
	     int repetitions, struct cache_results** pdata)
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


	state.width = 1;
	state.len = maxlen;
	state.maxlen = maxlen;
	state.line = line;
	state.pagesize = getpagesize();
	state.addr = NULL;

	/* count the (maximum) number of samples to take */
	for (len = start, incr = start / 4, samples = 0; len <= maxlen; incr<<=1) {
		for (i = 0; i < 4 && len <= maxlen; ++i, len += incr)
			samples++;
	}
	*pdata = (struct cache_results*)
		malloc(samples * sizeof(struct cache_results));

	p = *pdata;

	/* initialize the data */
	for (len = start, incr = start / 4, idx = 0; len <= maxlen; incr<<=1) {
		for (i = 0; i < 4 && len <= maxlen; ++i, ++idx, len += incr) {
			p[idx].len = len;
			p[idx].line = -1;
			p[idx].mline = -1;
			p[idx].latency = -1.;
			p[idx].ratio = -1.;
			p[idx].slope = -1.;
		}
	}

	/* make sure we have enough memory for the scratch data */
	while (state.addr == NULL) {
		mem_initialize(&state);
		if (state.addr == NULL) {
			maxlen /= 2;
			state.len = state.maxlen = maxlen;
			while (p[samples-1].len > maxlen)
				samples--;
		}
	}
	for (i = 0; i < samples; ++i)
		p[i].maxlen = maxlen;

	collect_sample(repetitions, &state, &p[0]);
	while (collect_sample(repetitions, &state, &p[samples-1]) == 0) {
		--samples;
	}
	search(0, samples - 1, repetitions, &state, p);

	/**/
	fprintf(stderr, "%10.10s %8.8s %8.8s %8.8s %8.8s %5.5s %5.5s\n", 
		"mem size", "latency", "variation", "ratio", "slope", 
		"line", "mline");
	for (idx = 0; idx < samples; ++idx) {
		if (p[idx].latency < 0.) continue;
		fprintf(stderr, 
			"%10.6f %8.3f %8.3f %8.3f %8.3f %4d %4d\n", 
			p[idx].len / (1000. * 1000.), 
			p[idx].latency, 
			p[idx].variation, 
			p[idx].ratio,
			p[idx].slope,
			p[idx].line,
			p[idx].mline);
	}
	/**/
	mem_cleanup(&state);

	return samples;
}

void
search(int left, int right, int repetitions, 
       struct mem_state* state, struct cache_results* p)
{
	int	middle = left + (right - left) / 2;

	if (p[left].latency > 0.0) {
		p[left].ratio = p[right].latency / p[left].latency;
		p[left].slope = (p[left].ratio - 1.) / (double)(right - left);
		/* we probably have a bad data point, so ignore it */
		if (p[left].ratio < 0.98) {
			p[left].latency = p[right].latency;
			p[left].ratio = 1.;
			p[left].slope = 0.;
		}
	}

	if (middle == left || middle == right)
		return;

	/*
	fprintf(stderr, "search(%d, %d, ...): middle=%d\n", p[left].len, p[right].len, p[middle].len);
	/**/

	if (p[left].ratio > 1.05 || p[left].ratio < 0.97) {
		collect_sample(repetitions, state, &p[middle]);
		search(middle, right, repetitions, state, p);
		search(left, middle, repetitions, state, p);
	}
	return;
}

int
collect_sample(int repetitions, struct mem_state* state, 
	       struct cache_results* p)
{
	int	i, j, k, chunk, page, npages, ntotalpages, nsparepages;
	int	modified, swapped, iters;
	int	*pages, *pageset;
	static int	available_index = 0;
	double	baseline, t, tt, low, var, new_baseline, nodiff_chunk_baseline;

	p->latency = measure(p->len, repetitions, &p->variation, state);
	baseline = p->latency;

	npages = p->len / getpagesize();
	ntotalpages = state->maxlen / getpagesize();
	nsparepages = ntotalpages - npages;
	pages = state->pages;
	pageset = state->pages + npages;
	chunk = (npages + 9) / 10;
	if (chunk > 10)
		chunk = 10;
	
	if (npages < 2 || nsparepages <= npages)
		return (p->latency > 0);

	nodiff_chunk_baseline = baseline;
	iters = 0;
	do {
		modified = 0;
		for (i = 0; i < npages; i+=chunk) {
			if (i + chunk > npages) chunk = npages - i;

			t = remove_chunk(i, chunk, npages, pages, 
					 p->len, repetitions, state);

			if (t >= 0.99 * baseline) continue;
			if (t >= 0.999 * nodiff_chunk_baseline) continue;

			/*
			fprintf(stderr, "collect_sample(...): faster chunk: baseline=%G, t=%G, len=%d, chunk=%d, i=%d\n", baseline, t, p->len, chunk, i);
			/**/

			swapped = 0;
			for (j = 0; j < chunk; ++j) {
				page = pages[i + j];
				tt = remove_chunk(i + j, 1, npages, pages, 
						  p->len, repetitions, state);
				/*
				fprintf(stderr, 
					"\ti = %d\tpage = %d\tbase = %G\ttt = %G\n", 
					i + j, page, baseline, tt);
				/**/
				if (tt >= 0.995 * baseline) continue;
				
				tt = remove_chunk(i + j, 1, npages, pages, 
						  p->len, repetitions, state);

				if (tt >= 0.995 * baseline) continue;

				low = tt;
				new_baseline = baseline;

				/* page is no good, find a substitute! */
				for (k = available_index; k < nsparepages && k - available_index < 2 * npages; ++k) {
					pages[i + j] = pageset[nsparepages - k - 1];
					tt = measure(p->len, repetitions, 
						     &var, state);
					/*
					fprintf(stderr, 
						"\t\tk = %d\tpage = %d\ttt = %G\tbase = %G\n", 
						k, pageset[k], tt, new_baseline);
					/**/

					/* this is the minimum so far */
					if (tt < new_baseline) {
						p->latency = tt;
						new_baseline = tt;
						p->variation = var;
						pageset[nsparepages - k - 1] = page;
						page = pages[i + j];
						++swapped;
						++modified;
					}

					/* pageset[k] is OK */
					if (tt < 0.995 * baseline
					    && low >= 0.995 * tt)
						break;
				}
				baseline = new_baseline;
				nodiff_chunk_baseline = new_baseline;
				available_index = (k < nsparepages) ? k : 0;
				if (k == nsparepages) 
					pages[i + j] = page;
			}
			if (swapped == 0 && t < nodiff_chunk_baseline)
				nodiff_chunk_baseline = t;
		}
		++iters;
	} while (modified && iters < 4);

	return (p->latency > 0);
}

double
measure(int size, int repetitions, 
	double* variation, struct mem_state* state)
{
	int	i, j, npages, nlines;
	int	*pages;
	double	time, median;
	result_t *r, *r_save;
	char	*p;

	pages = state->pages;
	npages = (size + getpagesize() - 1) / getpagesize();
	nlines = state->nlines;

	if (size % getpagesize())
		nlines = (size % getpagesize()) / state->line;

	r_save = get_results();
	r = (result_t*)malloc(sizeof_result(repetitions));
	insertinit(r);

	/* 
	 * assumes that you have used mem_initialize() to setup the memory
	 */
	p = state->base;
	for (i = 0; i < npages - 1; ++i) {
		for (j = 0; j < state->nwords; ++j) {
			*(char**)(p + pages[i] + state->lines[state->nlines - 1] + state->words[j]) = 
			p + pages[i+1] + state->lines[0] + state->words[j];
		}
	}
	for (j = 0; j < state->nwords; ++j) {
		*(char**)(p + pages[npages - 1] + state->lines[nlines - 1] + state->words[j]) = 
			p + pages[0] + state->lines[0] + state->words[(j+1)%state->nwords];
	}

	addr_save = NULL;
	state->p[0] = p + pages[0] + state->lines[0] + state->words[0];
	/* now, run through the chain once to clear the cache */
	mem_benchmark((size / sizeof(char*) + 100) / 100, state);

	for (i = 0; i < repetitions; ++i) {
		BENCH1(mem_benchmark(__n, state); __n = 1;, 0)
		insertsort(gettime(), get_n(), r);
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

	if (nlines < state->nlines) {
		for (j = 0; j < state->nwords; ++j) {
			*(char**)(p + pages[npages - 1] + state->lines[nlines - 1] + state->words[j]) = 
				p + pages[npages - 1] + state->lines[nlines] + state->words[j];
		}
	}
	/*
	fprintf(stderr, "%.6f %.2f\n", state->len / (1000. * 1000.), median);
	/**/

	return median;
}


double
remove_chunk(int i, int chunk, int npages, int* pages, 
	       int len, int repetitions, struct mem_state* state)
{
	int	n, j;
	double	t, var;

	if (i + chunk < npages) {
		for (j = 0; j < chunk; ++j) {
			n = pages[i+j];
			pages[i+j] = pages[npages-1-j];
			pages[npages-1-j] = n;
		}
	}
	t = measure(len - chunk * getpagesize(), repetitions, &var, state);
	if (i + chunk < npages) {
		for (j = 0; j < chunk; ++j) {
			n = pages[i+j];
			pages[i+j] = pages[npages-1-j];
			pages[npages-1-j] = n;
		}
	}
	
	return t;
}

