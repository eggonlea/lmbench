/*
 * lat_usleep.c - usleep duration/latency
 *
 * The interval API for usleep(3) and the getitimer(2) and 
 * setitimer(2) interval timers support resolutions down to 
 * a micro-second.  However, implementations do not support 
 * such resolution.  Most current implementations (as of Fall 
 * 2002) simply put the process back on the run queue and the 
 * process may get run on the next scheduler time slice (10-20 
 * milli-second resolution).
 *
 * This benchmark measures the true latency from the timer/sleep
 * call to the resumption of program execution.  If the timers
 * actually worked properly, then the latency would be identical
 * to the requested duration, or a little longer, so the input
 * and output figures would be nearly identical.  In most current
 * implementations the value is rounded up to the next scheduler
 * timeslice (e.g., a resolution of 20 milli-seconds, with all
 * values rounded up).
 *
 * usage: lat_usleep [-u | -i] [-W <warmup>] [-N <repetitions>] usecs
 *
 * Copyright (c) 2002 Carl Staelin.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char           *id = "$Id$\n";

#include "bench.h"

uint64          caught,
                n;
struct itimerval value;

void
bench_usleep(iter_t iterations, void *cookie)
{
    unsigned long  *usecs = (unsigned long *) cookie;

    while (iterations-- > 0) {
	usleep(*usecs);
    }
}

void
interval()
{
    if (++caught == n) {
	caught = 0;
	n = benchmp_interval(benchmp_getstate());
    }

    setitimer(ITIMER_REAL, &value, NULL);
}

void
initialize(void *cookie)
{
    unsigned long  *usecs = (unsigned long *) cookie;
    struct sigaction sa;

    value.it_interval.tv_sec = 0;
    value.it_interval.tv_usec = *usecs;
    value.it_value.tv_sec = 0;
    value.it_value.tv_usec = *usecs;

    sa.sa_handler = interval;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, 0);
}

void
bench_itimer(iter_t iterations, void *cookie)
{
    n = iterations;
    caught = 0;

    /*
     * start the first timing interval 
     */
    start(0);

    /*
     * create the first timer, causing us to jump to interval() 
     */
    setitimer(ITIMER_REAL, &value, NULL);

    while (1) {
	sleep(100000);
    }
}

int
main(int ac, char **av)
{
    int             u = 1;
    int             warmup = 0;
    int             repetitions = TRIES;
    int             c;
    char            buf[512];
    char           *usage = "[-u|i] [-W <warmup>] [-N <repetitions>] usecs\n";
    unsigned long   usecs;

    while ((c = getopt(ac, av, "iuW:N:")) != EOF) {
	switch (c) {
	case 'i':
	    u = 0;
	    break;
	case 'u':
	    u = 1;
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
    if (optind != ac - 1) {
	lmbench_usage(ac, av, usage);
    }

    usecs = bytes(av[optind]);
    if (u) {
	benchmp(NULL, bench_usleep, NULL, 0, 1, warmup, repetitions, &usecs);
	sprintf(buf, "usleep %lu microseconds", usecs);
    } else {
	benchmp(initialize, bench, NULL, 0, 1, warmup, repetitions, &usecs);
	sprintf(buf, "itimer %lu microseconds", usecs);
    }
    micro(buf, get_n());
    return (0);
}
