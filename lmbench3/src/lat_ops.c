/*
 * lat_ops.c - benchmark of simple operations
 *
 * Copyright (c) 1996-2000 Carl Staelin and Larry McVoy.  
 */
char	*id = "$Id$\n";

#include "bench.h"

struct _state {
	int	N;
	int	M;
	int	K;
	double*	data;
};

#define TEN(a) a a a a a a a a a a
#define HUNDRED(a) TEN(TEN(a))

void
do_integer_bitwise(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register int r = pState->N;

	while (iterations-- > 0) {
		for (i = 0; i < 1000; ++i) {
			HUNDRED(r^=i;r<<=1;)
		}
	}
	use_int(r);
}

void
do_integer_add(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register int r = pState->N;

	while (iterations-- > 0) {
		for (i = 1; i < 1001; ++i) {
#ifndef __GNUC__
			/* required because of an HP ANSI/C compiler bug */
			HUNDRED(r=(r+i)^r;)
#else
			TEN(r=r+r+i;)
#endif
		}
	}
	use_int(r);
}

void
do_integer_mul(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register int r = pState->N;

	while (iterations-- > 0) {
		for (i = 1; i < 1001; ++i) {
			HUNDRED(r=(r*i)^r;)
		}
	}
	use_int(r);
}

void
do_integer_div(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register int r = pState->N;

	while (iterations-- > 0) {
		for (i = 1; i < 1001; ++i) {
			HUNDRED(r=(r/i)^r;)
		}
	}
	use_int(r);
}

void
do_integer_mod(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register int r = pState->N;

	while (iterations-- > 0) {
		for (i = 1; i < 1001; ++i) {
			HUNDRED(r=(r%i)^r;)
		}
	}
	use_int(r);
}

void
do_uint64_bitwise(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register uint64 i;
	register uint64 r = pState->N;

	while (iterations-- > 0) {
		for (i = 0; i < 1000; ++i) {
			HUNDRED(r^=i;r<<=1;)
		}
	}
	use_int((int)r);
}

void
do_uint64_add(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register uint64 i;
	register uint64 r = pState->N;

	while (iterations-- > 0) {
		for (i = 1; i < 1001; ++i) {
#ifndef __GNUC__
			/* required because of an HP ANSI/C compiler bug */
			HUNDRED(r=(r+i)^r;)
#else
			TEN(r=r+r+i;)
#endif
		}
	}
	use_int((int)r);
}

void
do_uint64_mul(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register uint64 i;
	register uint64 r = pState->N;

	while (iterations-- > 0) {
		for (i = 1; i < 1001; ++i) {
			HUNDRED(r=(r*i)^r;)
		}
	}
	use_int((int)r);
}

void
do_uint64_div(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register uint64 i;
	register uint64 r = pState->N;

	while (iterations-- > 0) {
		for (i = 1; i < 1001; ++i) {
			HUNDRED(r=(r/i)^r;)
		}
	}
	use_int((int)r);
}

void
do_uint64_mod(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register uint64 i;
	register uint64 r = pState->N;

	while (iterations-- > 0) {
		for (i = 1; i < 1001; ++i) {
			HUNDRED(r=(r%i)^r;)
		}
	}
	use_int((int)r);
}

void
do_float_add(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register float f = (float)pState->N;
	register float g = (float)pState->K;

	while (iterations-- > 0) {
		for (i = 0; i < 1000; ++i) {
			TEN(f+=f;)
			f+=g;
		}
	}
	use_int((int)f);
	use_int((int)g);
}

void
do_float_mul(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register float f = (float)pState->N;
	register float g = (float)pState->M;

	while (iterations-- > 0) {
		for (i = 0; i < 1000; ++i) {
#ifndef __GNUC__
			HUNDRED(f*=f;)
#else
			/* required because of GCC bug */
			TEN(f*=f;)
#endif
		}
	}
	use_int((int)f);
	use_int((int)g);
}

void
do_float_div(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register float f = (float)pState->N;
	register float g = (float)pState->M;

	while (iterations-- > 0) {
		for (i = 0; i < 1000; ++i) {
#ifndef __GNUC__
			HUNDRED(f=g/f;)
#else
			/* required because of GCC bug */
			TEN(f=g/f;)
#endif
		}
	}
	use_int((int)f);
	use_int((int)g);
}

void
do_double_add(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register double f = (double)pState->N;
	register double g = (double)pState->K;

	while (iterations-- > 0) {
		for (i = 0; i < 1000; ++i) {
			TEN(f+=f;)
			f+=g;
		}
	}
	use_int((int)f);
	use_int((int)g);
}

void
do_double_mul(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register double f = (double)pState->N;
	register double g = (double)pState->M;

	while (iterations-- > 0) {
		for (i = 0; i < 1000; ++i) {
#ifndef __GNUC__
			HUNDRED(f*=f;)
#else
			/* required because of GCC bug */
			TEN(f*=f;)
#endif
		}
	}
	use_int((int)f);
	use_int((int)g);
}

void
do_double_div(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register double f = (double)pState->N;
	register double g = (double)pState->M;

	while (iterations-- > 0) {
		for (i = 0; i < 1000; ++i) {
#ifndef __GNUC__
			HUNDRED(f=g/f;)
#else
			/* required because of GCC bug */
			TEN(f=g/f;)
#endif
		}
	}
	use_int((int)f);
	use_int((int)g);
}

void
float_initialize(void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	float* x = (float*)malloc(pState->M * sizeof(float));;

	pState->data = (double*)x;
	for (i = 0; i < pState->M; ++i) {
		x[i] = 1.;
	}
}

void
double_initialize(void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;

	pState->data = (double*)malloc(pState->M * sizeof(double));
	for (i = 0; i < pState->M; ++i) {
		pState->data[i] = 1.;
	}
}

void
cleanup(void* cookie)
{
	struct _state *pState = (struct _state*)cookie;

	if (pState->data) 
		free(pState->data);
}

void
do_float_bogomflops(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register float *x = (float*)pState->data;

	while (iterations-- > 0) {
		for (i = 0; i < pState->M; ++i) {
			x[i] = (1.0 + x[i]) * (1.5 - x[i]) / x[i];
		}
	}
}

void
do_double_bogomflops(iter_t iterations, void* cookie)
{
	struct _state *pState = (struct _state*)cookie;
	register int i;
	register double *x = (double*)pState->data;

	while (iterations-- > 0) {
		for (i = 0; i < pState->M; ++i) {
			x[i] = (1.0 + x[i]) * (1.5 - x[i]) / x[i];
		}
	}
}

int
main(int ac, char **av)
{
	int	c, i, j;
	int	warmup = 0;
	int	parallel = 1;
	int	repetitions = TRIES;
	uint64	iop_time;
	uint64	iop_N;
	struct _state state;
	char   *usage = "[-W <warmup>] [-N <repetitions>]\n";

	state.N = 1;
	state.M = 1000;
	state.K = -1023;
	state.data = NULL;

	while (( c = getopt(ac, av, "W:N:P:")) != EOF) {
		switch(c) {
		case 'W':
			warmup = atoi(optarg);
			break;
		case 'N':
			repetitions = atoi(optarg);
			break;
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0) lmbench_usage(ac, av, usage);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}

	benchmp(NULL, do_integer_bitwise, NULL, 0, parallel, warmup, repetitions, &state);
	nano("integer bit", get_n() * 100000 * 2);
	iop_time = gettime();
	iop_N = get_n() * 100000 * 2;
	
	benchmp(NULL, do_integer_add, NULL, 0, 1, warmup, repetitions, &state);
#ifndef __GNUC__
	settime(gettime() - (get_n() * 100000 * iop_time) / iop_N);
	nano("integer add", get_n() * 100000);
#else
	nano("integer add", get_n() * 10000 * 2);
#endif
	
	benchmp(NULL, do_integer_mul, NULL, 0, 1, warmup, repetitions, &state);
	settime(gettime() - (get_n() * 100000 * iop_time) / iop_N);
	nano("integer mul", get_n() * 100000);
	
	benchmp(NULL, do_integer_div, NULL, 0, 1, warmup, repetitions, &state);
	settime(gettime() - (get_n() * 100000 * iop_time) / iop_N);
	nano("integer div", get_n() * 100000);
	
	benchmp(NULL, do_integer_mod, NULL, 0, 1, warmup, repetitions, &state);
	settime(gettime() - (get_n() * 100000 * iop_time) / iop_N);
	nano("integer mod", get_n() * 100000);
	
	benchmp(NULL, do_uint64_bitwise, NULL, 0, 1, warmup, repetitions, &state);
	nano("uint64 bit", get_n() * 100000 * 2);
	iop_time = gettime();
	iop_N = get_n() * 100000 * 2;

	benchmp(NULL, do_uint64_add, NULL, 0, 1, warmup, repetitions, &state);
#ifndef __GNUC__
	settime(gettime() - (get_n() * 100000 * iop_time) / iop_N);
	nano("uint64 add", get_n() * 100000);
#else
	nano("uint64 add", get_n() * 10000 * 2);
#endif
	
	benchmp(NULL, do_uint64_mul, NULL, 0, 1, warmup, repetitions, &state);
	settime(gettime() - (get_n() * 100000 * iop_time) / iop_N);
	nano("uint64 mul", get_n() * 100000);
	
	benchmp(NULL, do_uint64_div, NULL, 0, 1, warmup, repetitions, &state);
	settime(gettime() - (get_n() * 100000 * iop_time) / iop_N);
	nano("uint64 div", get_n() * 100000);
	
	benchmp(NULL, do_uint64_mod, NULL, 0, 1, warmup, repetitions, &state);
	settime(gettime() - (get_n() * 100000 * iop_time) / iop_N);
	nano("uint64 mod", get_n() * 100000);
	
	benchmp(NULL, do_float_add, NULL, 0, 1, warmup, repetitions, &state);
	nano("float add", get_n() * 1000 * 11);
	
	benchmp(NULL, do_float_mul, NULL, 0, 1, warmup, repetitions, &state);
#ifndef __GNUC__
	nano("float mul", get_n() * 100000);
#else
	nano("float mul", get_n() * 10000);
#endif
	
	benchmp(NULL, do_float_div, NULL, 0, 1, warmup, repetitions, &state);
#ifndef __GNUC__
	nano("float div", get_n() * 100000);
#else
	nano("float div", get_n() * 10000);
#endif

	benchmp(NULL, do_double_add, NULL, 0, 1, warmup, repetitions, &state);
	nano("double add", get_n() * 1000 * 11);
	
	benchmp(NULL, do_double_mul, NULL, 0, 1, warmup, repetitions, &state);
#ifndef __GNUC__
	nano("double mul", get_n() * 100000);
#else
	nano("double mul", get_n() * 10000);
#endif
	
	benchmp(NULL, do_double_div, NULL, 0, 1, warmup, repetitions, &state);
#ifndef __GNUC__
	nano("double div", get_n() * 100000);
#else
	nano("double div", get_n() * 10000);
#endif

	benchmp(float_initialize, 
		do_float_bogomflops, cleanup, 0, parallel, warmup, repetitions, &state);
	nano("float bogomflops", get_n() * state.M);

	benchmp(double_initialize, 
		do_double_bogomflops, cleanup, 0, parallel, warmup, repetitions, &state);
	nano("double bogomflops", get_n() * state.M);

	return(0);
}

