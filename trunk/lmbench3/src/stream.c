/*
 * steam.c - lmbench version of John McCalpin's STREAM benchmark
 *
 * usage: stream
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
	double*	a;
	double*	b;
	double*	c;
	double	scalar;
	int	len;
};

void initialize(void* cookie);
void copy(iter_t iterations, void* cookie);
void scale(iter_t iterations, void* cookie);
void sum(iter_t iterations, void* cookie);
void triad(iter_t iterations, void* cookie);
void cleanup(void* cookie);

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
	int	verbose = 0;
	int	warmup = 0;
	int	repetitions = TRIES;
	int	c;
	struct _state state;
	char   *usage = "[-v] [-W <warmup>] [-N <repetitions>][-M len[K|M]]\n";

        state.len = 1000 * 1000;
	state.scalar = 3.0;

	while (( c = getopt(ac, av, "vM:W:N:")) != EOF) {
		switch(c) {
		case 'v':
			verbose = 1;
			break;
		case 'M':
			state.len = bytes(optarg);
			/* convert from bytes to array length */
			state.len /= 3 * sizeof(double);
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

	benchmp(initialize, copy, cleanup, 0, 1, warmup, repetitions, &state);
	save_minimum();
	nano("STREAM copy latency", state.len * get_n());
	fprintf(stderr, "STREAM copy bandwidth: ");
	mb(2 * sizeof(double) * state.len * get_n());

	benchmp(initialize, scale, cleanup, 0, 1, warmup, repetitions, &state);
	save_minimum();
	nano("STREAM scale latency", state.len * get_n());
	fprintf(stderr, "STREAM scale bandwidth: ");
	mb(2 * sizeof(double) * state.len * get_n());

	benchmp(initialize, sum, cleanup, 0, 1, warmup, repetitions, &state);
	save_minimum();
	nano("STREAM sum latency", state.len * get_n());
	fprintf(stderr, "STREAM sum bandwidth: ");
	mb(3 * sizeof(double) * state.len * get_n());

	benchmp(initialize, triad, cleanup, 0, 1, warmup, repetitions, &state);
	save_minimum();
	nano("STREAM triad latency", state.len * get_n());
	fprintf(stderr, "STREAM triad bandwidth: ");
	mb(3 * sizeof(double) * state.len * get_n());

	return(0);
}

void
initialize(void* cookie)
{
	int i;
	struct _state* state = (struct _state*)cookie;
	
	state->a = (double*)malloc(sizeof(double) * state->len);
	state->b = (double*)malloc(sizeof(double) * state->len);
	state->c = (double*)malloc(sizeof(double) * state->len);

	if (state->a == NULL || state->b == NULL || state->c == NULL) {
		exit(1);
	}

	for (i = 0; i < state->len; ++i) {
		state->a[i] = 0.;
		state->b[i] = 0.;
		state->c[i] = 0.;
	}
}

#define BODY(expr)							\
{									\
	register int i;							\
	register int N = state->len;					\
	register double* a = state->a;					\
	register double* b = state->b;					\
	register double* c = state->c;					\
	register double scalar = state->scalar;				\
									\
	state->a = state->b;						\
	state->b = state->c;						\
	state->c = a;							\
									\
	for (i = 0; i < N; ++i) {					\
		expr;							\
	}								\
}

void copy(iter_t iterations, void *cookie)
{
	struct _state* state = (struct _state*)cookie;

	while (iterations-- > 0) {
		BODY(c[i] = a[i];)
	}
}

void scale(iter_t iterations, void *cookie)
{
	struct _state* state = (struct _state*)cookie;

	while (iterations-- > 0) {
		BODY(b[i] = scalar * c[i];)
	}
}

void sum(iter_t iterations, void *cookie)
{
	struct _state* state = (struct _state*)cookie;

	while (iterations-- > 0) {
		BODY(c[i] = a[i] + b[i];)
	}
}

void triad(iter_t iterations, void *cookie)
{
	struct _state* state = (struct _state*)cookie;

	while (iterations-- > 0) {
		BODY(a[i] = b[i] + scalar * c[i];)
	}
}

void cleanup(void* cookie)
{
	struct _state* state = (struct _state*)cookie;
	free(state->a);
	free(state->b);
	free(state->c);
}



