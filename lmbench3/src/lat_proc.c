/*
 * lat_proc.c - process creation tests
 *
 * Usage: lat_proc [-P <parallelism] procedure|fork|exec|shell
 *
 * TODO - linux clone, plan9 rfork, IRIX sproc().
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"


#ifdef STATIC
#define	PROG "/tmp/hello-s"
#define STATIC_PREFIX "Static "
#else
#define	PROG "/tmp/hello"
#define STATIC_PREFIX ""
#endif

void do_shell(uint64 iterations, void* cookie);
void do_forkexec(uint64 iterations,void* cookie);
void do_fork(uint64 iterations, void* cookie);
void do_procedure(uint64 iterations, void* cookie);
	
int
main(int ac, char **av)
{
	int parallel = 1;
	int c;
	char* usage = "[-P <parallelism>] procedure|fork|exec|shell\n";

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

	if (optind + 1 != ac) { /* should have one argument left */
		lmbench_usage(ac, av, usage);
	}

#define NOINIT NULL
#define NOCLEANUP NULL

	if (!strcmp("procedure", av[optind])) {
		benchmp(NOINIT,do_procedure,NOCLEANUP, 0, parallel, &ac);
		micro("Procedure call", get_n());
	} else if (!strcmp("fork", av[optind])) {
		benchmp(NOINIT,do_fork,NOCLEANUP, 0, parallel, NULL);
		micro(STATIC_PREFIX "Process fork+exit", get_n());
	} else if (!strcmp("exec", av[optind])) {
		benchmp(NOINIT,do_forkexec,NOCLEANUP,0,parallel,NULL);
		micro(STATIC_PREFIX "Process fork+execve", get_n());
	} else if (!strcmp("shell", av[optind])) {
		benchmp(NOINIT,do_shell,NOCLEANUP,0,parallel,NULL);
		micro(STATIC_PREFIX "Process fork+/bin/sh -c", get_n());
	} else {
		lmbench_usage(ac, av, usage);
	}
	return(0);
}

void do_shell(uint64 iterations, void* cookie)
{
	int	pid;
	while (iterations-- > 0) {
	  switch (pid = fork()) {
	    case -1:
	      perror("fork");
	      exit(1);
	    
	    case 0:	/* child */
	      close(1);
	      execlp("/bin/sh", "sh", "-c", PROG, 0);
	      exit(1);

	    default:
	      while (wait(0) != pid)
		;
	  }
	}
}

void do_forkexec(uint64 iterations,void* cookie)
{
	int	pid;
	char	*nav[2];

	while (iterations-- > 0) {
	  nav[0] = PROG;
	  nav[1] = 0;
	  switch (pid = fork()) {
	    case -1:
		perror("fork");
		exit(1);

	    case 0: 	/* child */
		close(1);
		execve(PROG, nav, 0);
		exit(1);

	    default:
		while (wait(0) != pid)
			;
	  }
	}
}
	
void do_fork(uint64 iterations, void* cookie)
{
	int pid;
	int i;

	while (iterations-- > 0) {
	  switch (pid = fork()) {
	    case -1:
	      perror("fork");
	      exit(1);
	
	    case 0:	/* child */
	      exit(1);
	
	    default:
	      while (wait(0) != pid);
	  }
	}
}
	
void do_procedure(uint64 iterations, void* cookie)
{
	int r = *(int *) cookie;
	while (iterations-- > 0) {
		use_int(r);
	}
}
