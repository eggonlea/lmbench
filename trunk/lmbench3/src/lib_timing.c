/*
 * a timing utilities library
 *
 * Requires 64bit integers to work.
 *
 * %W% %@%
 *
 * Copyright (c) 1994-1998 Larry McVoy.
 */
#define	 _LIB /* bench.h needs this */
#include "bench.h"
#include <setjmp.h>

#define	nz(x)	((x) == 0 ? 1 : (x))

/*
 * I know you think these should be 2^10 and 2^20, but people are quoting
 * disk sizes in powers of 10, and bandwidths are all power of ten.
 * Deal with it.
 */
#define	MB	(1000*1000.0)
#define	KB	(1000.0)

static struct timeval start_tv, stop_tv;
FILE		*ftiming;
uint64		use_result_dummy;	/* !static for optimizers. */
static	uint64	iterations;
static	void	init_timing(void);

#if defined(hpux) || defined(__hpux)
#include <sys/mman.h>
#endif

#if !defined(hpux) && !defined(__hpux) && !defined(WIN32)
#define RUSAGE
#endif
#ifdef	RUSAGE
#include <sys/resource.h>
#define	SECS(tv)	(tv.tv_sec + tv.tv_usec / 1000000.0)
#define	mine(f)		(int)(ru_stop.f - ru_start.f)

static struct rusage ru_start, ru_stop;

void
rusage(void)
{
	double  sys, user, idle;
	double  per;

	sys = SECS(ru_stop.ru_stime) - SECS(ru_start.ru_stime);
	user = SECS(ru_stop.ru_utime) - SECS(ru_start.ru_utime);
	idle = timespent() - (sys + user);
	per = idle / timespent() * 100;
	if (!ftiming) ftiming = stderr;
	fprintf(ftiming, "real=%.2f sys=%.2f user=%.2f idle=%.2f stall=%.0f%% ",
	    timespent(), sys, user, idle, per);
	fprintf(ftiming, "rd=%d wr=%d min=%d maj=%d ctx=%d\n",
	    mine(ru_inblock), mine(ru_oublock),
	    mine(ru_minflt), mine(ru_majflt),
	    mine(ru_nvcsw) + mine(ru_nivcsw));
}

#endif	/* RUSAGE */

void
lmbench_usage(int argc, char *argv[], char* usage)
{
	fprintf(stderr,"Usage: %s %s", argv[0], usage);
	exit(-1);
}

jmp_buf	benchmp_sigchld_env;
pid_t	benchmp_sigalrm_pid;
int	benchmp_sigalrm_timeout;
void (*benchmp_sigchld_handler)(int);
void (*benchmp_sigalrm_handler)(int);

void benchmp_sigchld(int signo)
{
	signal(SIGCHLD, SIG_IGN);
	longjmp(benchmp_sigchld_env, 1);
}

void benchmp_sigalrm(int signo)
{
	signal(SIGALRM, SIG_IGN);
	kill(benchmp_sigalrm_pid, SIGKILL);
	/* 
	 * Since we already waited a full timeout period for the child
	 * to die, we only need to wait a little longer for subsequent
	 * children to die.
	 */
	benchmp_sigalrm_timeout = 1;
}

double bench(bench_f benchmark,
	     uint64 iterations,
	     int enough,
	     void *cookie
	     );
double measure(bench_f benchmark, 
	       uint64 iterations, 
	       void *cookie
	       );

void 
benchmp_child(support_f initialize, 
	      bench_f benchmark,
	      support_f cleanup,
	      int response, 
	      int start_signal, 
	      int exit_signal,
	      int parallel, 
	      uint64 iterations,
	      int enough,
	      void* cookie
	      );
void
benchmp_parent(int response, 
	       int start_signal, 
	       int exit_signal, 
	       int parallel, 
	       uint64 iterations,
	       int enough
	       );

void benchmp(support_f initialize, 
	     bench_f benchmark,
	     support_f cleanup,
	     int enough, 
	     int parallel,
	     void* cookie)
{
	uint64		iterations = 1;
	double		result = 0.;
	double		usecs;
	long		i;
	pid_t		pid;
	pid_t		*pids = NULL;
	int		response[2];
	int		start_signal[2];
	int		exit_signal[2];
	int		need_warmup;
	fd_set		fds;
	struct timeval	timeout;

#ifdef _DEBUG
	fprintf(stderr, "benchmp(0x%x, 0x%x, 0x%x, %d, %d, 0x%x): entering\n", (unsigned int)initialize, (unsigned int)benchmark, (unsigned int)cleanup, enough, parallel, (unsigned int)cookie);
#endif
	enough = get_enough(enough);

	/* initialize results */
	settime(0);
	save_n(1);

	if (parallel > 1) {
		/* Compute the baseline performance */
		benchmp(initialize, benchmark, cleanup, enough, 1, cookie);
	}

	/* Create the necessary pipes for control */
	if (pipe(response) < 0
	    || pipe(start_signal) < 0
	    || pipe(exit_signal) < 0) {
		fprintf(stderr, "BENCHMP: Could not create control pipes\n");
		return;
	}

	/* fork the necessary children */
	signal(SIGTERM, SIG_IGN);
	benchmp_sigchld_handler = signal(SIGCHLD, benchmp_sigchld);
	pids = (pid_t*)malloc(parallel * sizeof(pid_t));
	if (setjmp(benchmp_sigchld_env)) {
		/* error exit, child died unexpectedly */
		fprintf(stderr, "BENCHMP: Child died unexpectedly\n");
		settime(0);
		save_n(1);
		return;
	}

	for (i = 0; i < parallel; ++i) {
#ifdef _DEBUG
		fprintf(stderr, "benchmp(0x%x, 0x%x, 0x%x, %d, %d, 0x%x): creating child %d\n", (unsigned int)initialize, (unsigned int)benchmark, (unsigned int)cleanup, enough, parallel, (unsigned int)cookie, i);
#endif
		switch(pid = fork()) {
		case -1:
			/* could not open enough children! */
			fprintf(stderr, "BENCHMP: fork() failed!\n");
			/* clean up and kill all children */
			signal(SIGCHLD, SIG_IGN);
			while (i > 0) {
				kill(pids[--i], SIGKILL);
				waitpid(pids[i], NULL, 0);
			}
			if (cleanup)
				(*cleanup)(cookie);
			if (pids) free(pids);
			exit(-1);
		case 0:
			/* If child */
			close(response[0]);
			close(start_signal[1]);
			close(exit_signal[1]);
			benchmp_child(initialize, 
				      benchmark, 
				      cleanup, 
				      response[1], 
				      start_signal[0], 
				      exit_signal[0],
				      enough,
				      iterations,
				      parallel,
				      cookie
				);
		default:
			pids[i] = pid;
			break;
		}
	}
	close(response[1]);
	close(start_signal[0]);
	close(exit_signal[0]);
	benchmp_parent(response[0], 
		       start_signal[1], 
		       exit_signal[1], 
		       parallel, 
		       iterations,
		       enough
		);

	/* 
	 * Clean up and kill all children
	 *
	 * NOTE: the children themselves SHOULD exit, and
	 *   Killing them could prevent them from
	 *   cleanup up subprocesses, etc... So, we only
	 *   want to kill child processes when it appears
	 *   that they will not die of their own accord.
	 *   We wait twice the timing interval plus two seconds
	 *   for children to die.  If they haven't died by 
	 *   that time, then we start killing them.
	 */
	benchmp_sigalrm_timeout = (int)((2 * enough)/(uint64)1000000) + 2;
	if (benchmp_sigalrm_timeout < 5)
		benchmp_sigalrm_timeout = 5;
	signal(SIGCHLD, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	while (i > 0) {
		i--;
		/* wait timeout seconds for child to die, then kill it */
		benchmp_sigalrm_pid = pids[i];
		signal(SIGALRM, benchmp_sigalrm);
		alarm(benchmp_sigalrm_timeout); 

		waitpid(pids[i], NULL, 0);

		alarm(0);
		signal(SIGALRM, SIG_IGN);
	}

	if (pids) free(pids);
#ifdef _DEBUG
	fprintf(stderr, "benchmp(0x%x, 0x%x, 0x%x, %d, %d, 0x%x): exiting\n", (unsigned int)initialize, (unsigned int)benchmark, (unsigned int)cleanup, enough, parallel, (unsigned int)cookie);
#endif
}

double bench(bench_f benchmark,
	     uint64 iterations,
		int enough,
		void *cookie
		)
{
	long i, N;
	double result;
	result_t r;

#ifdef _DEBUG
	fprintf(stderr, "bench(0x%x, %d, 0x%x): entering\n", (unsigned int)benchmark, enough, (unsigned int)cookie);
#endif
	insertinit(&r);
	N = (enough == 0 || get_enough(enough) <= 100000) ? TRIES : 1;
	/* warm the cache */
	if (enough < LONGER) {
		result = measure(benchmark, 1, cookie);
	}
	for (i = 0; i < N; ++i) {
		do {
			result = measure(benchmark, iterations, cookie);
			if (result < 0.99 * enough
			    || result > 1.2 * enough) {
				if (result > 150.) {
					double	tmp = iterations / result;
					tmp *= 1.1 * enough;
					iterations = (uint64)(tmp + 1);
				} else {
					if (iterations > (uint64)1<<40) {
						result = 0.;
						break;
					}
					iterations <<= 3;
				}
			}
		} while(result < 0.95 * enough);
		if (gettime() > 0)
			insertsort(gettime(), get_n(), &r);
#ifdef _DEBUG
		fprintf(stderr, "bench(0x%x, %d, 0x%x): i=%d, gettime()=%lu, get_n()=%d\n", (unsigned int)benchmark, enough, (unsigned int)cookie, i, (unsigned long)gettime(), (int)get_n());
#endif
	}
	save_results(&r);
}

double measure(bench_f benchmark, uint64 iterations, void *cookie)
{
	double result = 0.;

	start(0);
	(*benchmark)(iterations, cookie);
	result = stop(0,0);
	save_n(iterations);
	result -= t_overhead() + get_n() * l_overhead();
	settime(result >= 0. ? (uint64)result : 0.);

#ifdef _DEBUG
	fprintf(stderr, "measure(0x%x, %lu, 0x%x): result=%G\n", (unsigned int)benchmark, (unsigned long)iterations, (unsigned int)cookie, result);
#endif

	return result;
}


void 
benchmp_child(support_f initialize, 
		bench_f benchmark,
		support_f cleanup,
		int response, 
		int start_signal, 
		int exit_signal,
		int enough,
	        uint64 iterations,
		int parallel, 
		void* cookie
		)
{
	uint64		iterations_batch = (parallel > 1) ? get_n() : 1;
	double		result = 0.;
	double		usecs;
	long		i;
	result_t 	r;
	int		need_warmup;
	fd_set		fds;
	struct timeval	timeout;

	need_warmup = 1;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	signal(SIGCHLD, SIG_DFL);

	if (initialize)
		(*initialize)(cookie);

	/* work, and poll for 'start'  */
	FD_ZERO(&fds);
	while (!FD_ISSET(start_signal, &fds)) {
		result = measure(benchmark, iterations_batch, cookie);
		if (need_warmup) {
			need_warmup = 0;
			/* send 'ready' */
			write(response, &i, sizeof(int));
		}
		FD_SET(start_signal, &fds);
		select(start_signal+1, &fds, NULL,
		       NULL, &timeout);
	}

	/* find out how much work to do */
	read(start_signal, &iterations, sizeof(uint64));

	/* start experiments, collecting results */
	if (parallel > 1) {
		insertinit(&r);
		for (i = 0; i < TRIES; ++i) {
			result = measure(benchmark, iterations, cookie);
			insertsort(gettime(), get_n(), &r);
		}
	} else {
		bench(benchmark, iterations,enough, cookie);
		get_results(&r);
	}

	/* send results and 'done' */
	write(response, (void*)&r, sizeof(result_t));
	/* keep working, poll for 'exit' */
	FD_ZERO(&fds);
	while (!FD_ISSET(exit_signal, &fds)) {
		result = measure(benchmark, iterations_batch, cookie);
		FD_SET(exit_signal, &fds);
		select(exit_signal+1, &fds, NULL, NULL, &timeout);
	}

	/* exit */
	close(response);
	close(start_signal);
	/* close(exit_signal); */

	if (cleanup)
		(*cleanup)(cookie);

	exit(0);
}

void
benchmp_parent(	int response, 
		int start_signal, 
		int exit_signal, 
		int parallel, 
	        uint64 iterations,
		int enough
		)
{
	int		i,j,k,l;
	int		bytes_read;
	result_t*	results;
	result_t*	merged_results;
	result_t	scratch_results;
	unsigned char*	buf;

	results = (result_t*)malloc(parallel * sizeof(result_t));;
	bzero(results, parallel * sizeof(result_t));

	merged_results = (result_t*)malloc(parallel * sizeof(result_t));;
	bzero(merged_results, parallel * sizeof(result_t));

	/* Collect 'ready' signals */
	for (i = 0; i < parallel * sizeof(int); i += bytes_read) {
		bytes_read = read(response, results, parallel * sizeof(int) - i);
		if (bytes_read < 0) {
			/* error exit */
			break;
		}
	}

	/* calculate iterations for 1sec runtime */
	iterations = get_n();
	if (enough < SHORT) {
		double tmp = (double)SHORT * (double)get_n();
		tmp /= (double)gettime();
		iterations = (uint64)tmp + 1;
	}

	/* send 'start' signal */
	for (i = 0; i < parallel; ++i) {
		((uint64*)results)[i] = iterations;
	}
	write(start_signal, results, parallel * sizeof(uint64));

	/* Collect results and 'done' signals */
	for (i = 0, buf = (unsigned char*)results; 
	     i < parallel * sizeof(result_t); 
	     i += bytes_read, buf += bytes_read) {
		bytes_read = read(response, buf, parallel * sizeof(result_t) - i);
		if (bytes_read < 0) {
			/* error exit */
			fprintf(stderr, "Only read %d/%d bytes of results!\n", i, parallel * sizeof(result_t));
			break;
		}
	}

	/* we allow children to die now, without it causing an error */
	signal(SIGCHLD, SIG_IGN);
	
	/* send 'exit' signals */
	write(exit_signal, results, sizeof(int));

	/* Compute median time; iterations is constant! */
	insertinit(merged_results);
	for (i = 0; i < parallel; ++i) {
#ifdef _DEBUG
		fprintf(stderr, "\tresults[%d]: N=%d, gettime()=%lu, get_n()=%lu\n", i, results[i].N, (unsigned long)gettime(), (unsigned long)get_n());
		if (results[i].N < 0 || results[i].N > TRIES) fprintf(stderr, "***Bad results!***\n");
		{
			for (k = 0; k < results[i].N; ++k) {
				fprintf(stderr, "\t\t{%lu, %lu}\n", (unsigned long)results[i].v[k].u, (unsigned long)results[i].v[k].n);
			}
		}
#endif //_DEBUG
		scratch_results = results[i];
		for (j = 0; j < scratch_results.N; ++j) {
			insertsort(scratch_results.v[j].u,
				   scratch_results.v[j].n,
				   merged_results);
		}
	}
#ifdef _DEBUG
	fprintf(stderr, "Merged results; N=%d\n", merged_results->N);
	for (i = 0; i < merged_results->N; ++i) {
		fprintf(stderr, "\t%lu\t%lu\n", (unsigned long)merged_results->v[i].u, (unsigned long)merged_results->v[i].n);
	}
#endif //_DEBUG
	if (merged_results->N <= TRIES) {
		scratch_results = *merged_results;
	} else {
		scratch_results.N = TRIES;
		for (i = 0; i < TRIES/3; ++i) {
			scratch_results.v[i] = merged_results->v[i];
			scratch_results.v[TRIES - 1 - i] = merged_results->v[merged_results->N - 1 - i];
		}
		for (i = 0; i < TRIES - 2 * (TRIES/3); ++i) {
			scratch_results.v[TRIES/3 + i] = merged_results->v[merged_results->N/2 - (TRIES - 2 * (TRIES/3)) / 2 + i];
		}
	}
	save_results(&scratch_results);

#ifdef _DEBUG
	i = 0;
	fprintf(stderr, "*** Saving scratch_results: N=%d, gettime()=%lu, get_n()=%lu\n", scratch_results.N, (unsigned long)gettime(), (unsigned long)get_n());
	{
		int k;
		for (k = 0; k < scratch_results.N; ++k) {
			fprintf(stderr, "\t\t{%lu, %lu}\n", (unsigned long)scratch_results.v[k].u, (unsigned long)scratch_results.v[k].n);
		}
	}
#endif
	close(response);
	close(start_signal);
	close(exit_signal);

	free(results);
}


/*
 * Redirect output someplace else.
 */
void
timing(FILE *out)
{
	ftiming = out;
}

/*
 * Start timing now.
 */
void
start(struct timeval *tv)
{
	if (tv == NULL) {
		tv = &start_tv;
	}
#ifdef	RUSAGE
	getrusage(RUSAGE_SELF, &ru_start);
#endif
	(void) gettimeofday(tv, (struct timezone *) 0);
}

/*
 * Stop timing and return real time in microseconds.
 */
uint64
stop(struct timeval *begin, struct timeval *end)
{
	if (end == NULL) {
		end = &stop_tv;
	}
	(void) gettimeofday(end, (struct timezone *) 0);
#ifdef	RUSAGE
	getrusage(RUSAGE_SELF, &ru_stop);
#endif

	if (begin == NULL) {
		begin = &start_tv;
	}
	return tvdelta(begin, end);
}

uint64
now(void)
{
	struct timeval t;
	uint64	m;

	(void) gettimeofday(&t, (struct timezone *) 0);
	m = t.tv_sec;
	m *= 1000000;
	m += t.tv_usec;
	return (m);
}

double
Now(void)
{
	struct timeval t;

	(void) gettimeofday(&t, (struct timezone *) 0);
	return (t.tv_sec * 1000000.0 + t.tv_usec);
}

uint64
delta(void)
{
	static struct timeval last;
	struct timeval t;
	struct timeval diff;
	uint64	m;

	(void) gettimeofday(&t, (struct timezone *) 0);
	if (last.tv_usec) {
		tvsub(&diff, &t, &last);
		last = t;
		m = diff.tv_sec;
		m *= 1000000;
		m += diff.tv_usec;
		return (m);
	} else {
		last = t;
		return (0);
	}
}

double
Delta(void)
{
	struct timeval t;
	struct timeval diff;

	(void) gettimeofday(&t, (struct timezone *) 0);
	tvsub(&diff, &t, &start_tv);
	return (diff.tv_sec + diff.tv_usec / 1000000.0);
}

void
save_n(uint64 n)
{
	iterations = n;
}

uint64
get_n(void)
{
	return (iterations);
}

/*
 * Make the time spend be usecs.
 */
void
settime(uint64 usecs)
{
	bzero((void*)&start_tv, sizeof(start_tv));
	stop_tv.tv_sec = usecs / 1000000;
	stop_tv.tv_usec = usecs % 1000000;
}

void
bandwidth(uint64 bytes, uint64 times, int verbose)
{
	struct timeval tdiff;
	double  mb, secs;

	tvsub(&tdiff, &stop_tv, &start_tv);
	secs = tdiff.tv_sec;
	secs *= 1000000;
	secs += tdiff.tv_usec;
	secs /= 1000000;
	secs /= times;
	mb = bytes / MB;
	if (!ftiming) ftiming = stderr;
	if (verbose) {
		(void) fprintf(ftiming,
		    "%.4f MB in %.4f secs, %.4f MB/sec\n",
		    mb, secs, mb/secs);
	} else {
		if (mb < 1) {
			(void) fprintf(ftiming, "%.6f ", mb);
		} else {
			(void) fprintf(ftiming, "%.2f ", mb);
		}
		if (mb / secs < 1) {
			(void) fprintf(ftiming, "%.6f\n", mb/secs);
		} else {
			(void) fprintf(ftiming, "%.2f\n", mb/secs);
		}
	}
}

void
kb(uint64 bytes)
{
	struct timeval td;
	double  s, bs;

	tvsub(&td, &stop_tv, &start_tv);
	s = td.tv_sec + td.tv_usec / 1000000.0;
	bs = bytes / nz(s);
	if (!ftiming) ftiming = stderr;
	(void) fprintf(ftiming, "%.0f KB/sec\n", bs / KB);
}

void
mb(uint64 bytes)
{
	struct timeval td;
	double  s, bs;

	tvsub(&td, &stop_tv, &start_tv);
	s = td.tv_sec + td.tv_usec / 1000000.0;
	bs = bytes / nz(s);
	if (!ftiming) ftiming = stderr;
	(void) fprintf(ftiming, "%.2f MB/sec\n", bs / MB);
}

void
latency(uint64 xfers, uint64 size)
{
	struct timeval td;
	double  s;

	if (!ftiming) ftiming = stderr;
	tvsub(&td, &stop_tv, &start_tv);
	s = td.tv_sec + td.tv_usec / 1000000.0;
	if (xfers > 1) {
		fprintf(ftiming, "%d %dKB xfers in %.2f secs, ",
		    (int) xfers, (int) (size / KB), s);
	} else {
		fprintf(ftiming, "%.1fKB in ", size / KB);
	}
	if ((s * 1000 / xfers) > 100) {
		fprintf(ftiming, "%.0f millisec%s, ",
		    s * 1000 / xfers, xfers > 1 ? "/xfer" : "s");
	} else {
		fprintf(ftiming, "%.4f millisec%s, ",
		    s * 1000 / xfers, xfers > 1 ? "/xfer" : "s");
	}
	if (((xfers * size) / (MB * s)) > 1) {
		fprintf(ftiming, "%.2f MB/sec\n", (xfers * size) / (MB * s));
	} else {
		fprintf(ftiming, "%.2f KB/sec\n", (xfers * size) / (KB * s));
	}
}

void
context(uint64 xfers)
{
	struct timeval td;
	double  s;

	tvsub(&td, &stop_tv, &start_tv);
	s = td.tv_sec + td.tv_usec / 1000000.0;
	if (!ftiming) ftiming = stderr;
	fprintf(ftiming,
	    "%d context switches in %.2f secs, %.0f microsec/switch\n",
	    (int)xfers, s, s * 1000000 / xfers);
}

void
nano(char *s, uint64 n)
{
	struct timeval td;
	double  micro;

	tvsub(&td, &stop_tv, &start_tv);
	micro = td.tv_sec * 1000000 + td.tv_usec;
	micro *= 1000;
	if (!ftiming) ftiming = stderr;
	fprintf(ftiming, "%s: %.0f nanoseconds\n", s, micro / n);
}

void
micro(char *s, uint64 n)
{
	struct timeval td;
	double	micro;

	tvsub(&td, &stop_tv, &start_tv);
	micro = td.tv_sec * 1000000 + td.tv_usec;
	micro /= n;
	if (!ftiming) ftiming = stderr;
	fprintf(ftiming, "%s: %.4f microseconds\n", s, micro);
#if 0
	if (micro >= 100) {
		fprintf(ftiming, "%s: %.1f microseconds\n", s, micro);
	} else if (micro >= 10) {
		fprintf(ftiming, "%s: %.3f microseconds\n", s, micro);
	} else {
		fprintf(ftiming, "%s: %.4f microseconds\n", s, micro);
	}
#endif
}

void
micromb(uint64 sz, uint64 n)
{
	struct timeval td;
	double	mb, micro;

	tvsub(&td, &stop_tv, &start_tv);
	micro = td.tv_sec * 1000000 + td.tv_usec;
	micro /= n;
	mb = sz;
	mb /= MB;
	if (!ftiming) ftiming = stderr;
	if (micro >= 10) {
		fprintf(ftiming, "%.6f %.0f\n", mb, micro);
	} else {
		fprintf(ftiming, "%.6f %.3f\n", mb, micro);
	}
}

void
milli(char *s, uint64 n)
{
	struct timeval td;
	uint64 milli;

	tvsub(&td, &stop_tv, &start_tv);
	milli = td.tv_sec * 1000 + td.tv_usec / 1000;
	milli /= n;
	if (!ftiming) ftiming = stderr;
	fprintf(ftiming, "%s: %d milliseconds\n", s, (int)milli);
}

void
ptime(uint64 n)
{
	struct timeval td;
	double  s;

	tvsub(&td, &stop_tv, &start_tv);
	s = td.tv_sec + td.tv_usec / 1000000.0;
	if (!ftiming) ftiming = stderr;
	fprintf(ftiming,
	    "%d in %.2f secs, %.0f microseconds each\n",
	    (int)n, s, s * 1000000 / n);
}

uint64
tvdelta(struct timeval *start, struct timeval *stop)
{
	struct timeval td;
	uint64	usecs;

	tvsub(&td, stop, start);
	usecs = td.tv_sec;
	usecs *= 1000000;
	usecs += td.tv_usec;
	return usecs;
}

void
tvsub(struct timeval * tdiff, struct timeval * t1, struct timeval * t0)
{
	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	while (tdiff->tv_usec < 0 && tdiff->tv_sec > 0) {
		tdiff->tv_sec--;
		tdiff->tv_usec += 1000000;
	}
	/* time shouldn't go backwards!!! */
	if (tdiff->tv_usec < 0 || t1->tv_sec < t0->tv_sec) {
		tdiff->tv_sec = 0;
		tdiff->tv_usec = 0;
	}
}

uint64
gettime(void)
{
	return (tvdelta(&start_tv, &stop_tv));
}

double
timespent(void)
{
	struct timeval td;

	tvsub(&td, &stop_tv, &start_tv);
	return (td.tv_sec + td.tv_usec / 1000000.0);
}

static	char	p64buf[10][20];
static	int	n;

char	*
p64(uint64 big)
{
	char	*s = p64buf[n++];

	if (n == 10) n = 0;
#ifdef  linux
	{
        int     *a = (int*)&big;

        if (a[1]) {
                sprintf(s, "0x%x%08x", a[1], a[0]);
        } else {
                sprintf(s, "0x%x", a[0]);
        }
	}
#endif
#ifdef	__sgi
        sprintf(s, "0x%llx", big);
#endif
	return (s);
}

char	*
p64sz(uint64 big)
{
	double	d = big;
	char	*tags = " KMGTPE";
	int	t = 0;
	char	*s = p64buf[n++];

	if (n == 10) n = 0;
	while (d > 512) t++, d /= 1024;
	if (d == 0) {
		return ("0");
	}
	if (d < 100) {
		sprintf(s, "%.4f%c", d, tags[t]);
	} else {
		sprintf(s, "%.2f%c", d, tags[t]);
	}
	return (s);
}

char
last(char *s)
{
	while (*s++)
		;
	return (s[-2]);
}

int
bytes(char *s)
{
	int	n = atoi(s);

	if ((last(s) == 'k') || (last(s) == 'K'))
		n *= 1024;
	if ((last(s) == 'm') || (last(s) == 'M'))
		n *= (1024 * 1024);
	return (n);
}

void
use_int(int result) { use_result_dummy += result; }

void
use_pointer(void *result) { use_result_dummy += (int)result; }

void
insertinit(result_t *r)
{
	int	i;

	r->N = 0;
	for (i = 0; i < TRIES; i++) {
		r->v[i].u = 0;
		r->v[i].n = 1;
	}
}

/* biggest to smallest */
void
insertsort(uint64 u, uint64 n, result_t *r)
{
	int	i, j;

	for (i = 0; i < r->N; ++i) {
		if (u/(double)n > r->v[i].u/(double)r->v[i].n) {
			for (j = r->N; j > i; --j) {
				r->v[j] = r->v[j - 1];
			}
			break;
		}
	}
	r->v[i].u = u;
	r->v[i].n = n;
	r->N++;
}

static result_t results;

void
print_results(void)
{
	int	i;

	for (i = 0; i < results.N; ++i) {
		fprintf(stderr, "%.2f ", (double)results.v[i].u/results.v[i].n);
	}
}

void
get_results(result_t *r)
{
	*r = results;
}

void
save_results(result_t *r)
{
	results = *r;
	save_median();
}

void
save_minimum()
{
	if (results.N == 0) {
		save_n(1);
		settime(0);
	} else {
		save_n(results.v[results.N - 1].n);
		settime(results.v[results.N - 1].u);
	}
}

void
save_median()
{
	int	i = results.N / 2;
	uint64	u, n;

	if (results.N == 0) {
		n = 1;
		u = 0;
	} else if (results.N % 2) {
		n = results.v[i].n;
		u = results.v[i].u;
	} else {
		n = (results.v[i].n + results.v[i-1].n) / 2;
		u = (results.v[i].u + results.v[i-1].u) / 2;
	}
	save_n(n); settime(u);
}

/*
 * The inner loop tracks bench.h but uses a different results array.
 */
static long *
one_op(register long *p)
{
	BENCH_INNER(p = (long *)*p, 0);
	return (p);
}

static long *
two_op(register long *p, register long *q)
{
	BENCH_INNER(p = (long *)*q; q = (long*)*p, 0);
	return (p);
}

static long	*p = (long *)&p;
static long	*q = (long *)&q;

double
l_overhead(void)
{
	int	i;
	uint64	N_save, u_save;
	static	double overhead;
	static	int initialized = 0;
	result_t one, two, r_save;

	init_timing();
	if (initialized) return overhead;

	initialized = 1;
	if (getenv("LOOP_O")) {
		overhead = atof(getenv("LOOP_O"));
	} else {
		get_results(&r_save); N_save = get_n(); u_save = gettime(); 
		insertinit(&one);
		insertinit(&two);
		for (i = 0; i < TRIES; ++i) {
			use_pointer((void*)one_op(p));
			if (gettime() > 0 && gettime() > t_overhead())
				insertsort(gettime() - t_overhead(), get_n(), &one);
			use_pointer((void *)two_op(p, q));
			if (gettime() > 0 && gettime() > t_overhead())
				insertsort(gettime() - t_overhead(), get_n(), &two);
		}
		/*
		 * u1 = (n1 * (overhead + work))
		 * u2 = (n2 * (overhead + 2 * work))
		 * ==> overhead = 2. * u1 / n1 - u2 / n2
		 */
		save_results(&one); 
		save_minimum();
		overhead = 2. * gettime() / (double)get_n();
		
		save_results(&two); 
		save_minimum();
		overhead -= gettime() / (double)get_n();
		
		if (overhead < 0.) overhead = 0.;	/* Gag */

		save_results(&r_save); save_n(N_save); settime(u_save); 
	}
	return overhead;
}

/*
 * Figure out the timing overhead.  This has to track bench.h
 */
uint64
t_overhead(void)
{
	uint64		N_save, u_save;
	static int	initialized = 0;
	static uint64	overhead = 0;
	struct timeval	tv;
	result_t	r_save;

	init_timing();
	if (initialized) return overhead;

	initialized = 1;
	if (getenv("TIMING_O")) {
		overhead = atof(getenv("TIMING_O"));
	} else if (get_enough(0) <= 50000) {
		/* it is not in the noise, so compute it */
		int		i;
		result_t	r;

		get_results(&r_save); N_save = get_n(); u_save = gettime(); 
		insertinit(&r);
		for (i = 0; i < TRIES; ++i) {
			BENCH_INNER(gettimeofday(&tv, 0), 0);
			if (gettime() > 0) 
				insertsort(gettime(), get_n(), &r);
		}
		save_results(&r);
		save_minimum();
		overhead = gettime() / get_n();

		save_results(&r_save); save_n(N_save); settime(u_save); 
	}
	return overhead;
}

/*
 * Figure out how long to run it.
 * If enough == 0, then they want us to figure it out.
 * If enough is !0 then return it unless we think it is too short.
 */
static	int	long_enough;
static	int	compute_enough();

int
get_enough(int e)
{
	init_timing();
	return (long_enough > e ? long_enough : e);
}


static void
init_timing(void)
{
	static	int done = 0;

	if (done) return;
	done = 1;
	long_enough = compute_enough();
	t_overhead();
	l_overhead();
}

typedef long TYPE;

static TYPE **
enough_duration(register long N, register TYPE ** p)
{
#define	ENOUGH_DURATION_TEN(one)	one one one one one one one one one one
	while (N-- > 0) {
		ENOUGH_DURATION_TEN(p = (TYPE **) *p;);
	}
	return p;
}

static uint64
duration(long N)
{
	uint64	usecs;
	TYPE   *x = (TYPE *)&x;
	TYPE  **p = (TYPE **)&x;

	start(0);
	p = enough_duration(N, p);
	usecs = stop(0, 0);
	use_pointer((void *)p);
	return usecs;
}

/*
 * find the minimum time that work "N" takes in "tries" tests
 */
static uint64
time_N(long N)
{
	int     i;
	uint64	usecs;
	result_t r;

	insertinit(&r);
	for (i = 1; i < TRIES; ++i) {
		usecs = duration(N);
		if (usecs > 0)
			insertsort(usecs, N, &r);
	}
	save_results(&r);
	/*save_minimum();*/
	return gettime();
}

/*
 * return the amount of work needed to run "enough" microseconds
 */
static long
find_N(int enough)
{
	int		tries;
	static long	N = 10000;
	static uint64	usecs = 0;

	if (!usecs) usecs = time_N(N);

	for (tries = 0; tries < 10; ++tries) {
		if (0.98 * enough < usecs && usecs < 1.02 * enough)
			return N;
		if (usecs < 1000)
			N *= 10;
		else {
			double  n = N;

			n /= usecs;
			n *= enough;
			N = n + 1;
		}
		usecs = time_N(N);
	}
	return -1;
}

/*
 * We want to verify that small modifications proportionally affect the runtime
 */
static double test_points[] = {1.015, 1.02, 1.035};
static int
test_time(int enough)
{
	int     i;
	long	N;
	uint64	usecs, expected, baseline;

	if ((N = find_N(enough)) <= 0)
		return 0;

	baseline = time_N(N);

	for (i = 0; i < sizeof(test_points) / sizeof(double); ++i) {
		usecs = time_N((int)((double) N * test_points[i]));
		expected = (uint64)((double)baseline * test_points[i]);
		if (ABS(expected - usecs) / (double)expected > 0.0025)
			return 0;
	}
	return 1;
}


/*
 * We want to find the smallest timing interval that has accurate timing
 */
static int     possibilities[] = { 5000, 10000, 50000, 100000 };
static int
compute_enough()
{
	int     i;

	if (getenv("ENOUGH")) {
		return (atoi(getenv("ENOUGH")));
	}
	for (i = 0; i < sizeof(possibilities) / sizeof(int); ++i) {
		if (test_time(possibilities[i]))
			return possibilities[i];
	}

	/* 
	 * if we can't find a timing interval that is sufficient, 
	 * then use SHORT as a default.
	 */
	return SHORT;
}

/*
 * This stuff isn't really lib_timing, but ...
 */
void
morefds(void)
{
#ifdef	RLIMIT_NOFILE
	struct	rlimit r;

	getrlimit(RLIMIT_NOFILE, &r);
	r.rlim_cur = r.rlim_max;
	setrlimit(RLIMIT_NOFILE, &r);
#endif
}

void
touch(char *buf, int nbytes)
{
	static	psize;

	if (!psize) {
		psize = getpagesize();
	}
	while (nbytes > 0) {
		*buf = 1;
		buf += psize;
		nbytes -= psize;
	}
}

#if defined(hpux) || defined(__hpux)
int
getpagesize()
{
	return sysconf(_SC_PAGE_SIZE);
}
#endif

#ifdef WIN32
int
getpagesize()
{
	SYSTEM_INFO s;

	GetSystemInfo(&s);
	return (int)s.dwPageSize;
}

LARGE_INTEGER
getFILETIMEoffset()
{
	SYSTEMTIME s;
	FILETIME f;
	LARGE_INTEGER t;

	s.wYear = 1970;
	s.wMonth = 1;
	s.wDay = 1;
	s.wHour = 0;
	s.wMinute = 0;
	s.wSecond = 0;
	s.wMilliseconds = 0;
	SystemTimeToFileTime(&s, &f);
	t.QuadPart = f.dwHighDateTime;
	t.QuadPart <<= 32;
	t.QuadPart |= f.dwLowDateTime;
	return t;
}

int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
	LARGE_INTEGER			t;
	FILETIME			f;
	double					microseconds;
	static LARGE_INTEGER	offset;
	static double			frequencyToMicroseconds;
	static int				initialized = 0;
	static BOOL				usePerformanceCounter = 0;

	if (!initialized) {
		LARGE_INTEGER performanceFrequency;
		initialized = 1;
		usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
		if (usePerformanceCounter) {
			QueryPerformanceCounter(&offset);
			frequencyToMicroseconds = (double)performanceFrequency.QuadPart / 1000000.;
		} else {
			offset = getFILETIMEoffset();
			frequencyToMicroseconds = 10.;
		}
	}
	if (usePerformanceCounter) QueryPerformanceCounter(&t);
	else {
		GetSystemTimeAsFileTime(&f);
		t.QuadPart = f.dwHighDateTime;
		t.QuadPart <<= 32;
		t.QuadPart |= f.dwLowDateTime;
	}

	t.QuadPart -= offset.QuadPart;
	microseconds = (double)t.QuadPart / frequencyToMicroseconds;
	t.QuadPart = microseconds;
	tv->tv_sec = t.QuadPart / 1000000;
	tv->tv_usec = t.QuadPart % 1000000;
	return 0;
}
#endif
