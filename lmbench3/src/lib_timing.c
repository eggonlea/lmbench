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

/* #define _DEBUG */

#define	nz(x)	((x) == 0 ? 1 : (x))

/*
 * I know you think these should be 2^10 and 2^20, but people are quoting
 * disk sizes in powers of 10, and bandwidths are all power of ten.
 * Deal with it.
 */
#define	MB	(1000*1000.0)
#define	KB	(1000.0)

static struct timeval 	start_tv, stop_tv;
FILE			*ftiming;
static volatile uint64	use_result_dummy;
static		uint64	iterations;
static		void	init_timing(void);

#if defined(hpux) || defined(__hpux)
#include <sys/mman.h>
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

static pid_t	benchmp_sigalrm_pid;
static int	benchmp_sigalrm_timeout;
static int	benchmp_child_died;
void (*benchmp_sigchld_handler)(int);
void (*benchmp_sigalrm_handler)(int);

void benchmp_sigchld(int signo)
{
	signal(SIGCHLD, SIG_IGN);
	benchmp_child_died = 1;
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

void 
benchmp_child(support_f initialize, 
	      bench_f benchmark,
	      support_f cleanup,
	      int response, 
	      int start_signal, 
	      int result_signal, 
	      int exit_signal,
	      int parallel, 
	      iter_t iterations,
	      int repetitions,
	      int enough,
	      void* cookie
	      );
void
benchmp_parent(int response, 
	       int start_signal, 
	       int result_signal, 
	       int exit_signal, 
	       pid_t* pids,
	       int parallel, 
	       iter_t iterations,
	       int warmup,
	       int repetitions,
	       int enough
	       );

int
sizeof_result(int repetitions);

void 
benchmp(support_f initialize, 
	bench_f benchmark,
	support_f cleanup,
	int enough, 
	int parallel,
	int warmup,
	int repetitions,
	void* cookie)
{
	iter_t		iterations = 1;
	double		result = 0.;
	double		usecs;
	long		i, j;
	pid_t		pid;
	pid_t		*pids = NULL;
	int		response[2];
	int		start_signal[2];
	int		result_signal[2];
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
		benchmp(initialize, benchmark, cleanup, 
			enough, 1, warmup, repetitions, cookie);

		/* if we can't even do a single job, then give up */
		if (gettime() == 0)
			return;

		/* calculate iterations for 1sec runtime */
		iterations = get_n();
		if (enough < SHORT) {
			double tmp = (double)SHORT * (double)get_n();
			tmp /= (double)gettime();
			iterations = (iter_t)tmp + 1;
		}
		settime(0);
		save_n(1);
	}

	/* Create the necessary pipes for control */
	if (pipe(response) < 0
	    || pipe(start_signal) < 0
	    || pipe(result_signal) < 0
	    || pipe(exit_signal) < 0) {
#ifdef _DEBUG
		fprintf(stderr, "BENCHMP: Could not create control pipes\n");
#endif /* _DEBUG */
		return;
	}

	/* fork the necessary children */
	benchmp_child_died = 0;
	signal(SIGTERM, SIG_IGN);
	benchmp_sigchld_handler = signal(SIGCHLD, benchmp_sigchld);
	pids = (pid_t*)malloc(parallel * sizeof(pid_t));
	bzero((void*)pids, parallel * sizeof(pid_t));

	for (i = 0; i < parallel; ++i) {
#ifdef _DEBUG
		fprintf(stderr, "benchmp(0x%x, 0x%x, 0x%x, %d, %d, 0x%x): creating child %d\n", (unsigned int)initialize, (unsigned int)benchmark, (unsigned int)cleanup, enough, parallel, (unsigned int)cookie, i);
#endif
		switch(pid = fork()) {
		case -1:
			/* could not open enough children! */
#ifdef _DEBUG
			fprintf(stderr, "BENCHMP: fork() failed!\n");
#endif /* _DEBUG */
			/* give the children a chance to clean up gracefully */
			signal(SIGCHLD, SIG_IGN);
			for (j = 0; j < i; ++j) {
				kill(pids[j], SIGTERM);
			}
			goto error_exit;
		case 0:
			/* If child */
			close(response[0]);
			close(start_signal[1]);
			close(result_signal[1]);
			close(exit_signal[1]);
			benchmp_child(initialize, 
				      benchmark, 
				      cleanup, 
				      response[1], 
				      start_signal[0], 
				      result_signal[0], 
				      exit_signal[0],
				      enough,
				      iterations,
				      parallel,
				      repetitions,
				      cookie
				);
			exit(0);
		default:
			pids[i] = pid;
			break;
		}
	}
	close(response[1]);
	close(start_signal[0]);
	close(result_signal[0]);
	close(exit_signal[0]);
	benchmp_parent(response[0], 
		       start_signal[1], 
		       result_signal[1], 
		       exit_signal[1],
		       pids,
		       parallel, 
		       iterations,
		       warmup,
		       repetitions,
		       enough
		);

error_exit:
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
	benchmp_sigalrm_timeout = (int)((2 * enough)/1000000) + 2;
	if (benchmp_sigalrm_timeout < 5)
		benchmp_sigalrm_timeout = 5;
	signal(SIGCHLD, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	while (i-- > 0) {
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

void
benchmp_parent(	int response, 
		int start_signal, 
		int result_signal, 
		int exit_signal,
		pid_t* pids,
		int parallel, 
	        iter_t iterations,
		int warmup,
		int repetitions,
		int enough
		)
{
	int		i,j,k,l;
	int		bytes_read;
	result_t*	results;
	result_t*	merged_results;
	unsigned char*	buf;
	fd_set		fds_read, fds_error;
	struct timeval	timeout;

	if (benchmp_child_died) {
		goto error_exit;
	}

	results = (result_t*)malloc(parallel * sizeof_result(repetitions));;
	bzero(results, parallel * sizeof_result(repetitions));
	FD_ZERO(&fds_read);
	FD_ZERO(&fds_error);
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	merged_results = (result_t*)malloc(sizeof_result(parallel * repetitions));
	bzero(merged_results, sizeof_result(parallel * repetitions));
	insertinit(merged_results);

	/* Collect 'ready' signals */
	for (i = 0; i < parallel * sizeof(char); i += bytes_read) {
		bytes_read = 0;
		FD_SET(response, &fds_read);
		FD_SET(response, &fds_error);

		select(response+1, &fds_read, NULL, &fds_error, &timeout);
		if (benchmp_child_died || FD_ISSET(response, &fds_error)) {
			goto error_exit;
		}
		if (!FD_ISSET(response, &fds_read)) {
			continue;
		}

		bytes_read = read(response, results, parallel * sizeof(char) - i);
		if (bytes_read < 0) {
			goto error_exit;
		}
	}

	/* let the children run for warmup microseconds */
	if (warmup > 0) {
		struct timeval timeout;
		timeout.tv_sec = warmup / 1000000;
		timeout.tv_usec = warmup % 1000000;

		select(0, NULL, NULL, NULL, &timeout);
	}

	/* send 'start' signal */
	write(start_signal, results, parallel * sizeof(char));

	/* Collect 'done' signals */
	for (i = 0; i < parallel * sizeof(char); i += bytes_read) {
		bytes_read = 0;
		FD_SET(response, &fds_read);
		FD_SET(response, &fds_error);

		select(response+1, &fds_read, NULL, &fds_error, &timeout);
		if (benchmp_child_died || FD_ISSET(response, &fds_error)) {
			goto error_exit;
		}
		if (!FD_ISSET(response, &fds_read)) {
			continue;
		}

		bytes_read = read(response, results, parallel * sizeof(char) - i);
		if (bytes_read < 0) {
			goto error_exit;
		}
	}

	/* collect results */
	for (i = 0; i < parallel; ++i) {
		int n = sizeof_result(repetitions);
		buf = (unsigned char*)results + i * n;

		/* tell one child to report its results */
		write(result_signal, buf, sizeof(char));

		for (; n > 0; n -= bytes_read, buf += bytes_read) {
			bytes_read = 0;
			FD_SET(response, &fds_read);
			FD_SET(response, &fds_error);

			select(response+1, &fds_read, NULL, &fds_error, &timeout);
			if (benchmp_child_died || FD_ISSET(response, &fds_error)) {
				goto error_exit;
			}
			if (!FD_ISSET(response, &fds_read)) {
				continue;
			}

			bytes_read = read(response, buf, n);
			if (bytes_read < 0) {
				goto error_exit;
			}
		}
	}

	/* we allow children to die now, without it causing an error */
	signal(SIGCHLD, SIG_IGN);
	
	/* send 'exit' signals */
	write(exit_signal, results, parallel * sizeof(char));

	/* Compute median time; iterations is constant! */
	for (i = 0; i < parallel; ++i) {
		result_t* r = (result_t*)((char*)results + i * sizeof_result(repetitions));
#ifdef _DEBUG
		set_results(r);
		fprintf(stderr, "\tresults[%d]: N=%d, gettime()=%lu, get_n()=%lu\n", i, r->N, (unsigned long)gettime(), (unsigned long)get_n());
		if (r->N < 0 || r->N > repetitions) 
			fprintf(stderr, "***Bad results!***\n");
		{
			for (k = 0; k < r->N; ++k) {
				fprintf(stderr, "\t\t{%lu, %lu}\n", (unsigned long)r->v[k].u, (unsigned long)r->v[k].n);
			}
		}
#endif //_DEBUG
		for (j = 0; j < r->N; ++j) {
			insertsort(r->v[j].u, r->v[j].n, merged_results);
		}
	}
	set_results(merged_results);

#ifdef _DEBUG
	i = 0;
	fprintf(stderr, "*** Saving merged_results: N=%d, gettime()=%lu, get_n()=%lu\n", get_results()->N, (unsigned long)gettime(), (unsigned long)get_n());
	{
		int k;
		for (k = 0; k < get_results()->N; ++k) {
			fprintf(stderr, "\t{%lu, %lu}\n", (unsigned long)get_results()->v[k].u, (unsigned long)get_results()->v[k].n);
		}
	}
#endif
	goto cleanup_exit;
error_exit:
	signal(SIGCHLD, SIG_IGN);
	for (i = 0; i < parallel; ++i) {
		kill(pids[i], SIGTERM);
	}
	free(merged_results);
cleanup_exit:
	close(response);
	close(start_signal);
	close(result_signal);
	close(exit_signal);

	free(results);
}


typedef enum { warmup, timing_interval, cooldown } benchmp_state;

typedef struct {
	benchmp_state	state;
	support_f	initialize;
	bench_f		benchmark;
	support_f	cleanup;
	int		response;
	int		start_signal;
	int		result_signal;
	int		exit_signal;
	int		enough;
        iter_t		iterations;
	int		parallel;
        int		repetitions;
	void*		cookie;
	int		iterations_batch;
	int		need_warmup;
	long		i;
	int		r_size;
	result_t*	r;
} benchmp_child_state;

static benchmp_child_state _benchmp_child_state;

void*
benchmp_getstate()
{
	return ((void*)&_benchmp_child_state);
}

void 
benchmp_child(support_f initialize, 
		bench_f benchmark,
		support_f cleanup,
		int response, 
		int start_signal, 
		int result_signal, 
		int exit_signal,
		int enough,
	        iter_t iterations,
		int parallel, 
	        int repetitions,
		void* cookie
		)
{
	iter_t		iterations_batch = (parallel > 1) ? get_n() : 1;
	double		result = 0.;
	double		usecs;
	long		i = 0;
	int		need_warmup;
	fd_set		fds;
	struct timeval	timeout;

	_benchmp_child_state.state = warmup;
	_benchmp_child_state.initialize = initialize;
	_benchmp_child_state.benchmark = benchmark;
	_benchmp_child_state.cleanup = cleanup;
	_benchmp_child_state.response = response;
	_benchmp_child_state.start_signal = start_signal;
	_benchmp_child_state.result_signal = result_signal;
	_benchmp_child_state.exit_signal = exit_signal;
	_benchmp_child_state.enough = enough;
	_benchmp_child_state.iterations = iterations;
	_benchmp_child_state.iterations_batch = iterations_batch;
	_benchmp_child_state.parallel = parallel;
	_benchmp_child_state.repetitions = repetitions;
	_benchmp_child_state.cookie = cookie;
	_benchmp_child_state.need_warmup = 1;
	_benchmp_child_state.i = 0;
	_benchmp_child_state.r_size = sizeof_result(repetitions);
	_benchmp_child_state.r = (result_t*)malloc(_benchmp_child_state.r_size);

	insertinit(_benchmp_child_state.r);
	set_results(_benchmp_child_state.r);

	need_warmup = 1;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	signal(SIGCHLD, SIG_DFL);

	if (initialize)
		(*initialize)(cookie);

	/* start experiments, collecting results */
	insertinit(_benchmp_child_state.r);

	start(0);
	while (1) {
		(*benchmark)(benchmp_interval(&_benchmp_child_state), cookie);
	}
}

iter_t
benchmp_interval(void* _state)
{
	char		c;
	iter_t		iterations;
	double		result;
	fd_set		fds;
	struct timeval	timeout;
	benchmp_child_state* state = (benchmp_child_state*)_state;

	result = stop(0,0);
	save_n(state->iterations);
	result -= t_overhead() + get_n() * l_overhead();
	settime(result >= 0. ? (uint64)result : 0.);

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO(&fds);

	switch (state->state) {
	case warmup:
		iterations = state->iterations_batch;
		if (state->need_warmup) {
			state->need_warmup = 0;
			/* send 'ready' */
			write(state->response, &c, sizeof(char));
		}
		FD_SET(state->start_signal, &fds);
		select(state->start_signal+1, &fds, NULL,
		       NULL, &timeout);
		if (FD_ISSET(state->start_signal, &fds)) {
			state->state = timing_interval;
			read(state->start_signal, &c, sizeof(char));
			iterations = state->iterations;
		}
		break;
	case timing_interval:
		iterations = state->iterations;
		if (state->parallel > 1 || result > 0.95 * state->enough) {
			insertsort(gettime(), get_n(), get_results());
			state->i++;
			/* we completed all the experiments, return results */
			if (state->i >= state->repetitions) {
				state->state = cooldown;
			}
		}
		if (state->parallel == 1 
		    && (result < 0.99 * state->enough || result > 1.2 * state->enough)) {
			if (result > 150.) {
				double tmp = iterations / result;
				tmp *= 1.1 * state->enough;
				iterations = (iter_t)(tmp + 1);
			} else {
				iterations <<= 3;
				if (iterations > 1<<27
				    || result < 0. && iterations > 1<<20) {
					state->state = cooldown;
				}
			}
		}
		if (state->state != cooldown) {
			state->iterations = iterations;
		} else {
			/* send 'done' */
			write(state->response, (void*)&c, sizeof(char));
			iterations = state->iterations_batch;
		}
		break;
	case cooldown:
		iterations = state->iterations_batch;
		FD_SET(state->result_signal, &fds);
		select(state->result_signal+1, &fds, NULL, NULL, &timeout);
		if (FD_ISSET(state->result_signal, &fds)) {
			/* 
			 * At this point all children have stopped their
			 * measurement loops, so we can block waiting for
			 * the parent to tell us to send our results back.
			 * From this point on, we will do no more "work".
			 */
			read(state->result_signal, (void*)&c, sizeof(char));
			write(state->response, (void*)get_results(), state->r_size);
			if (state->cleanup)
				(*state->cleanup)(state->cookie);

			/* Now wait for signal to exit */
			read(state->exit_signal, (void*)&c, sizeof(char));
			exit(0);
		}
	};
	start(0);
	return (iterations);
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
	return (tvdelta(begin, end));
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
	if (s == 0.0) return;
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
	if (s == 0.0) return;
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
	if (s == 0.0) return;
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
	if (s == 0.0) return;
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
	if (micro == 0.0) return;
	if (!ftiming) ftiming = stderr;
	fprintf(ftiming, "%s: %.2f nanoseconds\n", s, micro / n);
}

void
micro(char *s, uint64 n)
{
	struct timeval td;
	double	micro;

	tvsub(&td, &stop_tv, &start_tv);
	micro = td.tv_sec * 1000000 + td.tv_usec;
	micro /= n;
	if (micro == 0.0) return;
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
	if (micro == 0.0) return;
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
	if (milli == 0.0) return;
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
	if (s == 0.0) return;
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
	return (usecs);
}

void
tvsub(struct timeval * tdiff, struct timeval * t1, struct timeval * t0)
{
	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0 && tdiff->tv_sec > 0) {
		tdiff->tv_sec--;
		tdiff->tv_usec += 1000000;
		assert(tdiff->tv_usec >= 0);
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

int
sizeof_result(int repetitions)
{
	if (repetitions <= TRIES)
		return (sizeof(result_t));
	return (sizeof(result_t) + (repetitions - TRIES) * sizeof(value_t));
}

void
insertinit(result_t *r)
{
	int	i;

	r->N = 0;
}

/* biggest to smallest */
void
insertsort(uint64 u, uint64 n, result_t *r)
{
	int	i, j;

	if (u == 0) return;

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

static result_t  _results;
static result_t* results = &_results;

void
print_results(void)
{
	int	i;

	fprintf(stderr, "N=%d, t={", results->N);
	for (i = 0; i < results->N; ++i) {
		fprintf(stderr, "%.2f", (double)results->v[i].u/results->v[i].n);
		if (i < results->N - 1) 
			fprintf(stderr, ", ");
	}
	fprintf(stderr, "}\n");
}

result_t*
get_results()
{
	return (results);
}

void
set_results(result_t *r)
{
	results = r;
	save_median();
}

void
save_minimum()
{
	if (results->N == 0) {
		save_n(1);
		settime(0);
	} else {
		save_n(results->v[results->N - 1].n);
		settime(results->v[results->N - 1].u);
	}
}

void
save_median()
{
	int	i = results->N / 2;
	uint64	u, n;

	if (results->N == 0) {
		n = 1;
		u = 0;
	} else if (results->N % 2) {
		n = results->v[i].n;
		u = results->v[i].u;
	} else {
		n = (results->v[i].n + results->v[i-1].n) / 2;
		u = (results->v[i].u + results->v[i-1].u) / 2;
	}
#ifdef _DEBUG
	fprintf(stderr, "save_median: N=%d, n=%lu, u=%lu\n", results->N, (unsigned long)n, (unsigned long)u);
#endif /* _DEBUG */
	save_n(n); settime(u);
}

/*
 * The inner loop tracks bench.h but uses a different results array.
 */
static long *
one_op(register long *p)
{
	BENCH_INNER(p = (long *)*p;, 0);
	return (p);
}

static long *
two_op(register long *p)
{
	BENCH_INNER(p = (long *)*p; p = (long*)*p;, 0);
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
	result_t one, two, *r_save;

	init_timing();
	if (initialized) return (overhead);

	initialized = 1;
	if (getenv("LOOP_O")) {
		overhead = atof(getenv("LOOP_O"));
	} else {
		r_save = get_results(); N_save = get_n(); u_save = gettime(); 
		insertinit(&one);
		insertinit(&two);
		for (i = 0; i < TRIES; ++i) {
			use_pointer((void*)one_op(p));
			if (gettime() > t_overhead())
				insertsort(gettime() - t_overhead(), get_n(), &one);
			use_pointer((void *)two_op(p));
			if (gettime() > t_overhead())
				insertsort(gettime() - t_overhead(), get_n(), &two);
		}
		/*
		 * u1 = (n1 * (overhead + work))
		 * u2 = (n2 * (overhead + 2 * work))
		 * ==> overhead = 2. * u1 / n1 - u2 / n2
		 */
		set_results(&one); 
		save_minimum();
		overhead = 2. * gettime() / (double)get_n();
		
		set_results(&two); 
		save_minimum();
		overhead -= gettime() / (double)get_n();
		
		if (overhead < 0.) overhead = 0.;	/* Gag */

		set_results(r_save); save_n(N_save); settime(u_save); 
	}
	return (overhead);
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
	result_t	*r_save;

	init_timing();
	if (initialized) return (overhead);

	initialized = 1;
	if (getenv("TIMING_O")) {
		overhead = atof(getenv("TIMING_O"));
	} else if (get_enough(0) <= 50000) {
		/* it is not in the noise, so compute it */
		int		i;
		result_t	r;

		r_save = get_results(); N_save = get_n(); u_save = gettime(); 
		insertinit(&r);
		for (i = 0; i < TRIES; ++i) {
			BENCH_INNER(gettimeofday(&tv, 0), 0);
			insertsort(gettime(), get_n(), &r);
		}
		set_results(&r);
		save_minimum();
		overhead = gettime() / get_n();

		set_results(r_save); save_n(N_save); settime(u_save); 
	}
	return (overhead);
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
	return (p);
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
	return (usecs);
}

/*
 * find the minimum time that work "N" takes in "tries" tests
 */
static uint64
time_N(iter_t N)
{
	int     i;
	uint64	usecs;
	result_t r, *r_save;

	r_save = get_results();
	insertinit(&r);
	for (i = 1; i < TRIES; ++i) {
		usecs = duration(N);
		insertsort(usecs, N, &r);
	}
	set_results(&r);
	save_minimum();
	usecs = gettime();
	set_results(r_save);
	return (usecs);
}

/*
 * return the amount of work needed to run "enough" microseconds
 */
static long
find_N(int enough)
{
	int		tries;
	static iter_t	N = 10000;
	static uint64	usecs = 0;

	if (!usecs) usecs = time_N(N);

	for (tries = 0; tries < 10; ++tries) {
		if (0.98 * enough < usecs && usecs < 1.02 * enough)
			return (N);
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
	return (-1);
}

/*
 * We want to verify that small modifications proportionally affect the runtime
 */
static double test_points[] = {1.015, 1.02, 1.035};
static int
test_time(int enough)
{
	int     i;
	iter_t	N;
	uint64	usecs, expected, baseline, diff;

	if ((N = find_N(enough)) <= 0)
		return (0);

	baseline = time_N(N);

	for (i = 0; i < sizeof(test_points) / sizeof(double); ++i) {
		usecs = time_N((int)((double) N * test_points[i]));
		expected = (uint64)((double)baseline * test_points[i]);
		diff = expected > usecs ? expected - usecs : usecs - expected;
		if (diff / (double)expected > 0.0025)
			return (0);
	}
	return (1);
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
			return (possibilities[i]);
	}

	/* 
	 * if we can't find a timing interval that is sufficient, 
	 * then use SHORT as a default.
	 */
	return (SHORT);
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
	return (sysconf(_SC_PAGE_SIZE));
}
#endif

#ifdef WIN32
int
getpagesize()
{
	SYSTEM_INFO s;

	GetSystemInfo(&s);
	return ((int)s.dwPageSize);
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
	return (t);
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
	return (0);
}
#endif