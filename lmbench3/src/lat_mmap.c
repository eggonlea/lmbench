/*
 * lat_mmap.c - time how fast a mapping can be made and broken down
 *
 * Usage: mmap [-P <parallelism>] [-W <warmup>] [-N <repetitions>] size file
 *
 * XXX - If an implementation did lazy address space mapping, this test
 * will make that system look very good.  I haven't heard of such a system.
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"

#define	PSIZE	(16<<10)
#define	N	10
#define	STRIDE	(10*PSIZE)
#define	MINSIZE	(STRIDE*2)

#define	CHK(x)	if ((x) == -1) { perror("x"); exit(1); }


typedef struct _state {
	int	size;
	int	fd;
	int	random;
	char	*name;
	char	myname[256];
} state_t;

void	init(void *cookie);
void	cleanup(void *cookie);
void	domapping(iter_t iterations, void * cookie);

int main(int ac, char **av)
{
	state_t state;
	int	parallel = 1;
	int	warmup = 0;
	int	repetitions = TRIES;
	char	buf[256];
	int	c;
	char	*usage = "[-r] [-P <parallelism>] [-W <warmup>] [-N <repetitions>] size file\n";
	

	state.random = 0;
	while (( c = getopt(ac, av, "rP:W:N:")) != EOF) {
		switch(c) {
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0)
				lmbench_usage(ac, av, usage);
			break;
		case 'W':
			warmup = atoi(optarg);
			break;
		case 'N':
			repetitions = atoi(optarg);
			break;
		case 'r':
			state.random = 1;
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}

	if (optind + 2 != ac) {
		lmbench_usage(ac, av, usage);
	}

	state.size = bytes(av[optind]);
	if (state.size < MINSIZE) {
		return (1);
	}
	state.name = av[optind+1];

	benchmp(init, domapping, cleanup, 0, parallel, 
		warmup, repetitions, &state);
	micromb(state.size, get_n());
	return (0);
}

void init(void * cookie)
{
	state_t *state = (state_t *) cookie;
	
	sprintf(state->myname,"%s.%d",state->name,getpid());
	CHK(state->fd = open(state->myname, O_CREAT|O_RDWR, 0666));
	CHK(ftruncate(state->fd, state->size));
}

void cleanup(void * cookie)
{
	state_t *state = (state_t *) cookie;
	close(state->fd);
	unlink(state->myname);
}

/*
 * This alg due to Linus.  The goal is to have both sparse and full
 * mappings reported.
 */
void domapping(iter_t iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
	register int fd = state->fd;
	register int size = state->size;
	register int random = state->random;
	register char	*p, *where, *end;
	register char	c = size & 0xff;

	while (iterations-- > 0) {

#ifdef	MAP_FILE
		where = mmap(0, size, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, fd, 0);
#else
		where = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
#endif
		if ((int)where == -1) {
			perror("mmap");
			exit(1);
		}
		if (random) {
			end = where + size;
			for (p = where; p < end; p += STRIDE) {
				*p = c;
			}
		} else {
			end = where + (size / N);
			for (p = where; p < end; p += PSIZE) {
				*p = c;
			}
		}
		munmap(where, size);
	}
}
