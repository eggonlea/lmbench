/*
 * memsize.c - figure out how much memory we have to use.
 *
 * Usage: memsize [max_wanted_in_MB]
 *
 * Copyright (c) 1995 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 */
char	*id = "$Id$\n";

#include "bench.h"

#define	CHK(x)	if ((x) == -1) { perror("x"); exit(1); }

#ifndef	TOO_LONG
#define	TOO_LONG	10	/* usecs */
#endif

void	timeit(char *where, int size);
static	void touchRange(char *p, char *end, int range);

int
main(int ac, char **av)
{
	char	*where;
	char	*tmp;
	size_t	size;
	size_t	max;

	if (ac == 2) {
		max = size = atoi(av[1]) * 1024 * 1024;
	} else {
		max = size = 1024 * 1024 * 1024;
	}
	/*
	 * Binary search down and then linear search up
	 */
	for (where = 0; !where; where = malloc(size)) {
		size >>= 1;
	}
	while (size < max) {
		free(where);
		size += 1024*1024;
		where = malloc(size);
		if (!where) {
			size -= 1024*1024;
			where = malloc(size);
			if (!where) {
				perror("malloc");
				exit(1);
			}
			break;
		}
	} 
	timeit(where, size);
	return (0);
}

void
timeit(char *where, int size)
{
	int	sum = 0;
	int	n;
	char	*end = where + size;
	int	range;

	if (size < 1024*1024 - 16*1024) {
		fprintf(stderr, "Bad size\n");
		return;
	}

	/* Go up in 1MB chunks until we find one too big */
	for (range = 2*1024*1024; range <= size; range += 1<<20) {
		touchRange(where, end, range);
		start(0);
		touchRange(where, end, range);
		sum = stop(0, 0);
		n = range / 4096;
		if ((sum / n) > TOO_LONG) {
			fprintf(stderr, "\n");
			printf("%d\n", (range>>20) - 1);
			exit(0);
		}
		fprintf(stderr, "%dMB OK\r", range/(1024*1024));
	}
	fprintf(stderr, "\n");
	printf("%d\n", (size>>20));
}

static void
touchRange(char *p, char *end, int range)
{
	char	*tmp = p;

	while (range > 0 && (tmp < end)) {
		*tmp = 0;
		tmp += 4096;
		range -= 4096;
	}
}

#undef	malloc
#undef	free

char	*
Malloc(int n)
{
	char	*p = malloc(n);

	fprintf(stderr, "malloc(%d) = %x\n", n, p);
	return (p);
}

Free(char *p)
{
	fprintf(stderr, "free(%x)\n", p);
	free(p);
}
