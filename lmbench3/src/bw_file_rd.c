/*
 * bw_file_rd.c - time reading & summing of a file
 *
 * Usage: bw_file_rd [-P <parallelism] size file
 *
 * The intent is that the file is in memory.
 * Disk benchmarking is done with lmdd.
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"

#define	CHK(x)		if ((int)(x) == -1) { perror(#x); exit(1); }
#ifndef	MIN
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#define	TYPE	int
#define	MINSZ	(sizeof(TYPE) * 128)

TYPE	*buf;		/* do the I/O here */
TYPE	*lastone;	/* I/O ends here + MINSZ */
int	xfersize;	/* do it in units of this */
int	count;		/* bytes to move (can't be modified) */

typedef struct _state {
	char filename[256];
	int fd;
} state_t;

void doit(int fd)
{
	int	sum = 0, size;
	register TYPE *p, *end;

	size = count;
	end = lastone;
	while (size >= 0) {
		if (read(fd, buf, MIN(size, xfersize)) <= 0) {
			break;
		}
		for (p = buf; p <= end; ) {
			sum +=
			p[0]+p[1]+p[2]+p[3]+p[4]+p[5]+p[6]+p[7]+
			p[8]+p[9]+p[10]+p[11]+p[12]+p[13]+p[14]+
			p[15]+p[16]+p[17]+p[18]+p[19]+p[20]+p[21]+
			p[22]+p[23]+p[24]+p[25]+p[26]+p[27]+p[28]+
			p[29]+p[30]+p[31]+p[32]+p[33]+p[34]+p[35]+
			p[36]+p[37]+p[38]+p[39]+p[40]+p[41]+p[42]+
			p[43]+p[44]+p[45]+p[46]+p[47]+p[48]+p[49]+
			p[50]+p[51]+p[52]+p[53]+p[54]+p[55]+p[56]+
			p[57]+p[58]+p[59]+p[60]+p[61]+p[62]+p[63]+
			p[64]+p[65]+p[66]+p[67]+p[68]+p[69]+p[70]+
			p[71]+p[72]+p[73]+p[74]+p[75]+p[76]+p[77]+
			p[78]+p[79]+p[80]+p[81]+p[82]+p[83]+p[84]+
			p[85]+p[86]+p[87]+p[88]+p[89]+p[90]+p[91]+
			p[92]+p[93]+p[94]+p[95]+p[96]+p[97]+p[98]+
			p[99]+p[100]+p[101]+p[102]+p[103]+p[104]+
			p[105]+p[106]+p[107]+p[108]+p[109]+p[110]+
			p[111]+p[112]+p[113]+p[114]+p[115]+p[116]+
			p[117]+p[118]+p[119]+p[120]+p[121]+p[122]+
			p[123]+p[124]+p[125]+p[126]+p[127];
			p += 128;
		}
		size -= xfersize;
	}
	use_int(sum);
}

void init_open(void * cookie)
{
	state_t	*state = (state_t *) cookie;
	int	ofd;

	CHK(ofd = open(state->filename, O_RDONLY));
	state->fd = ofd;
}

void cleanup_io(void * cookie)
{
	state_t *state = (state_t *) cookie;
	close(state->fd);
}

void time_with_open(uint64 iterations, void * cookie)
{
	state_t	*state = (state_t *) cookie;
	char	*filename = state->filename;
	int	fd;

	while (iterations-- > 0) {
	  fd= open(filename, O_RDONLY);
	  doit(fd);
	  close(fd);
	}
}

void time_io_only(uint64 iterations,void * cookie)
{
	state_t *state = (state_t *) cookie;
	int fd = state->fd;

	while (iterations-- > 0) {
	  lseek(fd, 0, 0);
	  doit(fd);
	}
}

int main(int ac, char **av)
{
	int	fd;
	state_t state;
	int	parallel = 1;
	int	c;
	char	usage[132];
	
	sprintf(usage,"[-P <parallelism>] <size> open2close|io_only <filename>"
		"\nmin size=%d\n",(int) (XFERSIZE>>10)) ;

	while (( c = getopt(ac, av, "P:")) != EOF) {
		switch(c) {
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0) lmbench_usage(ac, av, usage);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}

	if (optind + 3 != ac) { /* should have three arguments left */
		lmbench_usage(ac, av, usage);
	}

	strcpy(state.filename,av[optind+2]);
	count = bytes(av[optind]);
	if (count < MINSZ) {
		exit(1);	/* I want this to be quiet */
	}
	if (count < XFERSIZE) {
		xfersize = count;
	} else {
		xfersize = XFERSIZE;
	}
	buf = (TYPE *)valloc(XFERSIZE);
	lastone = (TYPE*)((char*)buf + xfersize - MINSZ);
	bzero((void*)buf, XFERSIZE);

	if (!strcmp("open2close", av[optind+1])) {
	  benchmp(NULL,time_with_open,NULL,0,parallel,&state);
	} else if (!strcmp("io_only", av[optind+1])) {
	  benchmp(init_open,time_io_only,cleanup_io,0,parallel,&state);
	} else lmbench_usage(ac, av, usage);
	bandwidth(count, get_n() * parallel, 0);
	return (0);
}
