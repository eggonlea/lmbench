/*
 * lat_select.c - time select system call
 *
 * usage: lat_select [-P <parallelism>] [-W <warmup>] [-N <repetitions>] [n]
 *
 * Copyright (c) 1996 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 */
char	*id = "$Id$\n";

#include "bench.h"

void initialize(void *cookie);
void cleanup(void *cookie);
void doit(uint64 iterations,void *cookie);
void writer(int w, int r);

typedef struct _state {
	int	num;
	int	max;
	fd_set  set;
} state_t;

int main(int ac, char **av)
{
	state_t state;
	int parallel = 1;
	int warmup = 0;
	int repetitions = TRIES;
	int c;
	char* usage = "[-P <parallelism>] [-W <warmup>] [-N <repetitions>] [n]\n";
	char	buf[256];

	morefds();  /* bump fd_cur to fd_max */
	while (( c = getopt(ac, av, "P:W:N:")) != EOF) {
		switch(c) {
		case 'P':
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
	state.num = 200;
	if (optind + 1 == ac) {
		state.num = atoi(av[optind]);
		puts("num bumped");
	} else if (optind < ac) {
		lmbench_usage(ac, av, usage);
	}

	benchmp(initialize, doit, cleanup, 0, parallel, 
		warmup, repetitions, &state);
	sprintf(buf, "Select on %d fd's", state.num);
	micro(buf, get_n());
}


void doit(uint64 iterations, void * cookie)
{
	state_t * 	state = (state_t *)cookie;
	fd_set		nosave;
	static struct timeval tv;
	static count = 0;
	
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	while (iterations-- > 0) {
		nosave = state->set;
		select(state->num, 0, &nosave, 0, &tv);
	}
}

void initialize(void *cookie)
{
	char	c;
	state_t * state = (state_t *)cookie;

	int	i, last = 0 /* lint */;
	int	N = state->num, fd;

	state->max = 0;
	FD_ZERO(&(state->set));
	for (fd = 0; fd < N; fd++) {
		i = dup(0);
		if (i == -1) break;
		if (i > state->max)
			state->max = i;
		FD_SET(i, &(state->set));
	}
}

void cleanup(void *cookie)
{
	int	i;
	state_t * state = (state_t *)cookie;

	for (i = 0; i <= state->max; ++i) {
		if (FD_ISSET(i, &(state->set)))
			close(i);
	}
	FD_ZERO(&(state->set));
}

	     
