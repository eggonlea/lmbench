/*
 * lat_select.c - time select system call
 *
 * usage: lat_select [n]
 *
 * Copyright (c) 1996 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 */
char	*id = "$Id$\n";

#include "bench.h"

void
doit(int n, fd_set *set)
{
	fd_set	nosave = *set;
	static	struct timeval tv;
	select(n, 0, &nosave, 0, &tv);
}

int
main(int ac, char **av)
{
	int	i, last = 0 /* lint */;
	int	N = 200, fd;
	fd_set	set;
	char	buf[256];

	morefds();
	if (ac > 1) N = atoi(av[1]);
	FD_ZERO(&set);
	for (i = 3; i < 50; ++i) close(i);
	for (fd = 0; fd < N; fd++) {
		i = dup(0);
		if (i == -1) break;
		last = i;
		FD_SET(i, &set);
	}
	last++;
	BENCH(doit(last, &set), 0);
	sprintf(buf, "Select on %d fd's", fd);
	micro(buf, get_n());
	return(0);
}
