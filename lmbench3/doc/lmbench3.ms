.\" This document is GNU groff -mgs -t -p -R -s
.\" It will not print with normal troffs, it uses groff features, in particular,
.\" long names for registers & strings.
.\" Deal with it and use groff - it makes things portable.
.\"
.\" $X$ xroff -mgs -t -p -R -s $file
.\" $tty$ groff -mgs -t -p -R -s $file | colcrt - | more
.\" $lpr$ groff -mgs -t -p -R -s $file > ${file}.lpr
.VARPS
.\" Define a page top that looks cool
.\" HELLO CARL!  To turn this off, s/PT/oldPT/
.de draftPT
.\" .tl '\fBDRAFT\fP'Printed \\*(DY'\fBDRAFT\fP'
..
.de lmPT
.if \\n%>1 \{\
.	sp -.1i
.	ps 14
.	ft 3
.	nr big 24
.	nr space \\w'XXX'
.	nr titlewid \\w'\\*[title]'
.	nr barwid (\\n[LL]-(\\n[titlewid]+(2*\\n[space])))/2
.	ds ln \\l'\\n[barwid]u'\\h'-\\n[barwid]u'\v'-.25'
.	ds bar \\s(\\n[big]\\*(ln\\*(ln\\*(ln\\*(ln\\*(ln\v'1.25'\\h'\\n[barwid]u'\\s0
.	ce 1
\\*[bar]\h'\\n[space]u'\v'-.15'\\*[title]\v'.15'\h'\\n[space]u'\\*[bar]
.	ps
.	sp -.70
.	ps 12
\\l'\\n[LL]u'
.	ft
.	ps
.\}
..
.\" Define a page bottom that looks cool
.\" HELLO CARL!  To turn this off, s/BT/oldBT/
.de draftBT
.\" .tl '\fBDRAFT\fP'Page %'\fBDRAFT\fP'
..
.de lmBT
.	ps 9
\v'-1'\\l'\\n(LLu'
.	sp -1
.	tl '\(co 2001 \\*[author]'\\*(DY'%'
.	ps
..
.de SP
.	if t .sp .5
.	if n .sp 1
..
.de BU
.	SP
.	ne 2
\(bu\ 
.	if \\n[.$] \fB\\$1\fP\\$2
..
.nr FIGURE 0
.nr TABLE 0
.nr SMALL .25i
.de TSTART
.	KF
.	if \\n[.$] \s(24\\l'\\n[pg@colw]u'\s0
.	ps -1
.	vs -1
..
.de TEND
.	ps +1
.	vs +1
.	if \\n[.$]=2 \{\
.	sp -.5
\s(24\\l'\\n[pg@colw]u'\s0 \}
.	sp .25
.	nr TABLE \\n[TABLE]+1
.	ce 1
\fBTable \\n[TABLE].\ \ \\$1\fP
.	SP
.	KE
..
.de FEND
.	ps +1
.	vs +1
.	if \\n[.$]=2 \{\
.	sp -.5
\s(24\\l'\\n[pg@colw]u'\s0 \}
.	sp .25
.	nr FIGURE \\n[FIGURE]+1
.	ce 1
\fBFigure \\n[FIGURE].\ \ \\$1\fP
.	SP
.	KE
..
.\" Configuration
.nr PI 3n
.nr HM 1i
.nr FM 1i
.nr PO 1i
.if t .po 1i
.nr LL 6.5i
.if n .nr PO 0i
.if n .nr LL 7.5i
.nr PS 10
.nr VS \n(PS+1
.ds title Measuring scalability
.ds author Carl Staelin
.ds lmbench \f(CWlmbench\fP
.ds lmbench2 \f(CWlmbench2\fP
.ds lmbench3 \f(CWlmbench3\fP
.ds lmdd  \f(CWlmdd\fP
.ds bcopy \f(CWbcopy\fP
.ds connect \f(CWconnect\fP
.ds execlp  \f(CWexeclp\fP
.ds exit \f(CWexit\fP
.ds fork \f(CWfork\fP
.ds gcc \f(CWgcc\fP
.ds getpid \f(CWgetpid\fP
.ds getpid \f(CWgetpid\fP
.ds gettimeofday \f(CWgettimeofday\fP
.ds kill \f(CWkill\fP
.ds memmove \f(CWmemmove\fP
.ds mmap \f(CWmmap\fP
.ds popen  \f(CWpopen\fP
.ds read \f(CWread\fP
.ds stream \f(CWstream\fP
.ds system  \f(CWsystem\fP
.ds uiomove \f(CWuiomove\fP
.ds write \f(CWwrite\fP
.ds yield  \f(CWyield\fP
.ds select  \f(CWselect\fP
.ds benchmp  \f(CWbenchmp\fP
.ds mhz  \f(CWmhz\fP
.ds lat_ops  \f(CWlat_ops\fP
.ds lat_connect  \f(CWlat_connect\fP
.ds par_ops  \f(CWpar_ops\fP
.\" References stuff
.de RN  \"Reference Name: .RN $1 -- prints the reference prettily
.\" [\s-2\\$1\s+2]\\$2
[\s-1\\$1\s0]\\$2
..
.\" .R1
.\" sort A+DT
.\" database references
.\" label-in-text
.\" label A.nD.y-2
.\" bracket-label \*([. \*(.] ", "
.\" .R2
.EQ
delim $$
.EN
.TL
\s(14lmbench3: Measuring scalability\s0
.AU
\s+2\fR\*[author]\fP\s0
.AI
\fI\s+2Hewlett-Packard Laboratories Israel\s0\fP
.SP
.AB
\*[lmbench] version 3 incorporates a new timing harness,
benchmp, designed to measure performance at specific levels 
of parallel (simultaneous) load.  It also incorporates some
new micro-benchmarks to measure basic system operations
and scalability, such as arithmetic operations and the
basic elements of the memory hierarchy.
.\" .SP
\*[lmbench] is a micro-benchmark suite designed to focus
attention on the basic building blocks of many
common system applications, such as databases, simulations, 
software development, and networking.  
.AE
.if t .MC 3.05i
.NH 1
Introduction
.LP
An important feature of multi-processor systems is their
ability to scale their performance.  \*[lmbench] version
1 was able to measure various important aspects of 
system performance, except that only one client process
was active at a time
.RN McVoy96 .
\*[lmbench2] introduced a new macro, BENCH(), which
implemented a sophisticated timing harness that
automatically managed nearly all aspects of accurately
timing operations
.RN Staelin98 .
For example, it automatically
detects the minimal timing interval necessary to 
provide timing results within 1% accuracy, and it
automatically repeats most experiments eleven times
and reports the median result.
.LP
However, this timing harness is incapable of measuring
the performance of a system under scalable loads.  
\*[lmbench3] took the ideas and techniques
developed in the earlier versions and extended them
to create a new timing harness which can measure
system performance under parallel, scalable loads.
.LP
\*[lmbench3] also includes a version of John 
McCalpin's STREAM benchmarks.  Essentially the STREAM 
kernels were placed in the new \*[lmbench] timing harness.
Since the new timing harness also measures scalability
under parallel load, the new \*[lmbench] version
includes this capability automatically.  
.LP
Finally, \*[lmbench3] includes a number of new
benchmarks which measure various aspects of the
processor architecture, such as basic operation
latency and parallelism, to provide developers
with a better understanding of system capabilities.
The hope is that better informed developers will
be able to better design and evaluate performance
critical software in light of their increased
understanding of basic system performance.
.NH 1
Prior Work
.LP
Benchmarking is not a new field of endeavor.
There are a wide variety of approaches to 
benchmarking, many of which differ greatly
from that taken by \*[lmbench].  
.LP
One common form of benchmark is to take an
important application or application and
worklist, and to measure the time required
to complete the entire task.  
This approach is particularly useful when 
evaluating the utility of systems for a 
single and well-known task.
.LP
Other benchmarks, such as SPECint, use a
variation on this approach by measuring
several applications and combining the
results to predict overall performance.
.LP
XXX Byte benchmark
.LP
Another variation takes the "kernel" of
an important application and measures its
performance, where the "kernel" is usually
a simplification of the most expensive
portion of a program.  
Dhrystone 
.RN Weicker84
is an example of this type of
benchmark as it measures the performance
of important matrix operations and was often
used to predict system performance for
numerical operations.
.LP
.RN Banga98
developed a benchmark to measure HTTP server
performance which can accurately measure
server performance under high load.
Due to the idiosyncracies of the HTTP protocol 
and TCP design and implementation, there are 
generally operating system limits on the rate 
at which a single system can generate 
independent HTTP requests.  
However, 
.RN Banga98
developed a system which can scalably present
load to HTTP servers in spite of this limitation.
.LP
John McCalpin's STREAM benchmark measures
memory bandwidth during four common vector
operations
.RN McCalpin95 .
It does not measure memory latency, and
strictly speaking it does not measure raw
memory bandwith although memory bandwidth
is crucial to STREAM performance.
More recently, work has begun on extending
STREAM to measure scalable memory subsystem
performance, particularly for multi-processor
machines.
.LP
Micro-benchmarking extends this "kernel" 
approach, by measuring the performance
of operations or resources in isolation.
\*[lmbench] and many other benchmarks, such 
as nfsstone
.RN Shein89 ,
measure the performance of key operations so 
users can predict performance for certain 
workloads and applications by combining the 
performance of these operations in the right 
mixture.
.LP
.RN Seltzer99
takes the micro-benchmark approach and applies
it to the problem of predicting application
performance. 
They instrument applications to determine the
basic components of application usage in terms
of the elements measured by \*[lmbench], and
then they predict application performance on
a variety of systems by combining the measured
resource requirements with the measured system
performance.
XXX I think someone else did this before Margo
.LP
Benchmarking I/O systems has proven particularly
troublesome over the years, largely due to the
strong non-linearities exhibited by disk systems.
Sequential I/O provides much higher bandwidth
than non-sequential I/O, so performance is 
highly dependent on the workload characteristics
as well as the file system's ability to 
capitalize on available sequentiality by
laying out data contiguously on disk.
.LP
I/O benchmarks have a tendency to age poorly.
For example, IOStone
.RN Park90a 
and the Andrew benchmark
.RN Howard88
used fixed size datasets, whose size was
significant at the time, but which no longer
measure I/O performance as the data can now
fit in the processor cache of many modern
machines.
.LP
Bonnie
.RN Bray90
measures sequential, streaming I/O bandwidth
for a single process.  
IOBench
.RN Wolman89
XXX
.LP
Peter Chen developed an adaptive harness for
I/O benchmarking
.RN Chen94a ,
which defines I/O load in terms of five parameters,
uniqueBytes, sizeMean, readFrac, seqFrac, and
processNum.  The benchmark then explores the
parameter space to measure file system performance
in a scalable fashion.
.LP
The AIM benchmark... XXX
.LP
Prestor's memory analysis and Saavedra's work XXX
.NH 1
Computer Architecture Primer
.LP
A processor architecture is generally defined by its
instruction set, but most computer architectures
incorporate a large number of common building blocks
and concepts, such as registers, arithmetic logic
units, and caches.
.LP
Of necessity, this primer over-simplifies the
many details and variations of specific computer
designs and architectures.  For more information,
please see 
.RN Hennessy96 .
.TSTART 1
.so lmbench3_arch.pic
.FEND "Architecture diagram" 1
.LP
Figure \n[FIGURE] contains a greatly simplified block diagram
of a computer.  Various important elements, such as
the I/O bus and devices, have been left out.  The
core of the processor are the registers (r0, ..., rn
and f0, ..., fn) and the arithmetic units (ALU and FPU).
In general, the arithmetic units can access data in
registers ''instantly''.  Often data must be explicitly
loaded from memory into a register before it can be
manipulated by the arithmetic units.
.LP
The ALU handles integer arithmetic, such as bit
operations (AND, OR, XOR, NOT, and SHIFT) as
well as ADD, MUL, DIV, and MOD.  Sometimes there
is specialized hardware to handle one or more
operations, such as a barrel shifter for SHIFT
or a multiplier, and sometimes there is no
hardware support for certain operations, such
as MUL, DIV, and MOD.  
.LP
The FPU handles floating point arithmetic.
Sometimes there are separate FPUs for single
and double precision floating point operations.
.NH 2
Memory Hierarchy
.LP
Nearly all modern processors use some form of virtual
memory addressing, meaning that each process has its
own virtual address space, or mapping of virtual 
addresses to physical addresses.  The core of the
system operates on virtual addresses, so addresses
must be translated to physical addresses in order
to actually read or write data to memory.  Most
machines do the translation between the core and
the top level cache using a ``translation look-aside
buffer'' (TLB), which is really a cache of virtual
to physical page mappings.
.LP
Nearly all machines use at least some form of
data caching, and many machines have multiple
levels of cache.  A rule of thumb is that moving
down a level in hierarchy decreases the performance 
by at least a factor of two while the size increases 
by a factor of ten.
.LP
Caches store and replace data in fixed size units, 
called ``lines''.  Cache lines are some multiple 
of the basic word size.  
.LP
In most cases, a write
to any location in a cache line which is not
currently resident in the cache will cause the
line to be read into the cache before the
dirty portion is over-written.  
.LP
Cache replacement policies need to be fast, so
systems usually use some form of set-associative
replacement policy.  In this case, each word in
memory can be place into any of a set of lines.
If the set has a single member, then the cache
is called ``direct mapped'' because each location
in memory has a direct mapping to a single cache
line.  Typically large caches might be direct-
mapped, while smaller caches tend to have higher
levels of associativity.
.NH 2
Some Recent Innovations
.LP
There are a number of modern extensions to computer
architecture that attempt to increase the processor's
ability to do several things at once.  Nearly all of
these enhancements are intended to be invisible to
programmers using higher-level languages such as
C or JAVA.
.IP "\fBSuperscalar processors\fR"
Superscalar processors have multiple processing
units which can operate simultaneously.  
.IP "\fBDynamic instruction reordering\fR"
Dynamic instruction reordering allows the processor
to execute instructions whose operands are ready
before instructions which are stalled waiting for
memory or other instruction's completion.
.IP "\fBMemory parallelism\fR"
By allowing multiple outstanding memory requests,
processors allow the memory subsystem to service
multiple (independent) requests in parallel. 
Since memory accesses are a common performance
bottleneck, this can greatly improve performance.
.IP "\fBVector processing\fR"
Vector processing allows the processor to execute
arithmetic operations on vector operands in 
parallel, and in modern commodity processors goes
by names such as MMX, SSE, and 3DNow.
.IP "\fBSimultaneous multi-threading (SMT)\fR"
SMT allows superscalar processors to simulatenously
execute instructions from several threads (contexts)
.RN Tullsen96 .
SMT may include extensions which allow for very
lightweight inter-thread synchronization primitives
that enable much finer-grained thread-level 
parallelism than traditional synchronization
methods
.RN Tullsen99 .
.IP "\fBExplicitly parallel instruction computers (EPIC)\fR"
EPIC allows the compiler to explicitly issue $N$
instructions in parallel at each instruction, which
informs the hardware that these instructions are
independent and may be executed in parallel
.RN Schlansker00 .
It moves much of the burden regarding dependency
checking from the hardware to the compiler.
.NH 1
Timing Harness
.LP
The first, and most crucial element in extending
\*[lmbench2] so that it could measure scalable
performance, was to develop a new timing harness
that could accurately measure performance for
any given load.
Once this was done, then each benchmark would
be migrated to the new timing harness.
.LP
The harness is designed to accomplish a number
of goals:
.IP 1.
during any timing interval of any child it is
guaranteed that all other child processes are
also running the benchmark
.IP 2.
the timing intervals are long enough to average
out most transient OS scheduler affects
.IP 3.
the timing intervals are long enough to ensure
that error due to clock resolution is negligible
.IP 4.
timing measurements can be postponed to allow
the OS scheduler to settle and adjust to the
load
.LP
Developing an accurate timing harness with a
valid experimental design is more difficult 
than is generally supposed.
Many programs incorporate elementary timing
harnesses which may suffer from one or more
defects, such as insufficient care taken to
ensure that the benchmarked operation is run
long enough to ensure that the error introduced 
by the clock resolution is insignificant.
The basic elements of a good timing harness
are discussed in 
.RN Staelin98 .
.LP
The new timing harness must also collect and process
the timing results from all the child processes so
that it can report the representative performance.
It currently reports the median performance over
all timing intervals from all child processes.  It
might perhaps be argued that it should report the
median of the medians.
.LP
Most of the benchmarks now accept a "-P <parallelism>"
flag, and the timing harness does the right thing to
try and measure parallel application performance.
.NH 2
Coordination
.LP
However, developing a timing harness that
correctly manages $N$ processes and accurately
measures system performance over those same
$N$ processes is significantly more difficult
because of the asynchronous nature of the
distributed programming problem.
.LP
In essence, the new timing harness needs to create
$N$ jobs, and measure the average performance of the
target subsystem while all $N$ jobs are running.  This
is a standard problem for parallel and distributed
programming, and involves starting the child
processes and then stepping through a handshaking
process to ensure that all children have started
executing the benchmarked operation before any child
starts taking measurements.
Table 1 shows the various steps taken by the timing
harness.
.TSTART 1
.TS
box tab (/) allbox expand ;
c c
l l .
Parent/Child
T{
running the benchmark with P==1 to get a 
baseline estimate of performance.  The
timing interval is auto-scaled to the system
gettimeofday() clock resolution so this is 
usually quite fast.  
T}/
T{
compute the number of iterations required for 
a single benchmark to run for one second.  
During the timing intervals each child will 
repeat the benchmark this number of times.
T}
T{
start up P child processes
T}/T{
run benchmark operation for a little while
T}
T{
wait for P "ready" signals
T}/T{
send a "ready" signal
T}
T{
[sleep for "warmup" microseconds]
T}/T{
run benchmark operation while polling for a "go" signal
T}
T{
send "go" signal to P children
T}/T{
begin timing benchmark operation
T}
T{
wait for P "done" signals
T}/T{
send a "done" signal
T}
T{
for each child, send "results" signal and gather results
T}/T{
run benchmark operation while polling for a "results" signal
T}
T{
collate results
T}/T{
send timing results and wait for "exit" signal
T}
T{
send "exit" signal
T}/T{
exit
T}
.TE
.TEND "Timing harness sequencing"
.NH 2
Accuracy
.LP
The new timing harness also needs to ensure that the 
timing intervals are long enough for the results to 
be representative.  The previous timing harness assumed
that only single process results were important, and
it was able to use timing intervals as short as
possible while ensuring that errors introduced by
the clock resolution were negligible.  
In many instances this meant that the timing intervals 
were smaller than a single scheduler time slice.  
The new timing harness must run benchmarked items 
long enough to ensure that timing intervals are longer
than a single scheduler time slice.
Otherwise, you can get results which are complete nonsense.  
For example, running several copies of an \*[lmbench2] 
benchmark on a uni-processor machine will often report 
that the performance with $N$ jobs running in parallel 
is equivalent to the performance with a single job running!\**
.FS
This was discovered by someone who naively attempted
to parallelize \*[lmbench2] in this fashion, and I
received a note from the dismayed developer describing
the failed experiment.
.FE
.LP
In addition, since the timing intervals now have to be
longer than a single scheduler time slice, they also
need to be long enough so that a single scheduler time
slice is insignificant compared to the timing interval.
Otherwise the timing results can be dramatically 
affected by small variations in the scheduler's
behavior.
.NH 2
Resource consumption
.LP
One important design goal was that resource consumption
be constant with respect to the number of child
processes.  
This is why the harness uses shared pipes to communicate
with the children, rather than having a separate set of
pipes to communicate with each child.
An early design of the system utilized a pair of pipes
per child for communication and synchronization between
the master and slave processes.  However, as the number
of child processes grew, the fraction of system 
resources consumed by the harness grew and the additional
system overhead could start to interfere with the accuracy 
of the measurements.
.LP
Additionally, if the master has to poll (\*[select])
$N$ pipes, then the system overhead of that operation
also scales with the number of children.  
.NH 2
Pipe atomicity
.LP
Since all communication between the master process and
the slave (child) processes is done via a set of shared
pipes, we have to ensure that we never have a situation
where the message can be garbled by the intermingling
of two separate messages from two separate children.
This is ensured by either using pipe operations that
are guaranteed to be atomic on all machines, or by
coordinating between processes so that at most one
process is writing at a time.
.LP
The atomicity guarantees are provided by having each
client communicate synchronization states in one-byte 
messages.  For example, the signals from the master
to each child are one-byte messages, so each child
only reads a single byte from the pipe.  Similarly,
the responses from the children back to the master
are also one-byte messages.  In this way no child
can receive partial messages, and no message can
be interleaved with any other message.
.LP
However, using this design means that we need to
have a separate pipe for each \fIbarrier\fR in
the process, so the master uses three pipes to
send messages to the children, namely: \fIstart_signal\fR,
\fIresult_signal\fR, and \fIexit_signal\fR.
If a single pipe was used for all three barrier events,
then it is possible for a child to miss a signal,
or if the signal is encoded into the message, 
then it is possible for a child to infinite loop
pulling a signal off the pipe, recognizing that
it has already received that signal so that it
needs to push it back into the pipe, and then
then re-receiving the same message it just re-sent.
.LP
However, all children share a single pipe to send
data back to the master process.  Usually the
messages on this pipe are single-byte signals,
such as \fIready\fR or \fIdone\fR.  However, the
timing data results need to be sent from the
children to the master and they are (much) larger
than a single-byte message.  In this case, the
timing harness sends a single-byte message on
the \fIresult_signal\fR channel, which can be
received by at most one child process.  This
child then knows that it has sole ownership of
the response pipe, and it writes its entire 
set of timing results to this pipe.  Once the
master has received all of the timing results
from a single child, it sends the next one-byte
message on the \fIresult_signal\fR channel to
gather the next set of timing results.
.TSTART 1
.so lmbench3_signals.pic
.FEND "Control signals" 1
.LP
The design of the signals is shown in Figure \n[FIGURE].
.NH 2
Benchmark initialization
.LP
By allowing the benchmark to specify an
initialization routine that is run in the
child processes, the new timing harness
allows benchmarks to do either or both
global initializations that are shared
by all children and specific per-child
initializations that are done independently
by each child.
Global initialization is done in the
master process before the \*[benchmp] 
harness is called, so the state is 
preserved across the \*[fork] operations.
Per-child initialization is done inside
the \*[benchmp] harness by the optional
initialization routine and is done after
the \*[fork] operation.
.LP
Similarly, each benchmark is allowed to
specify a cleanup routine that is run by
the child processes just before exiting.
This allows the benchmark routines to
release any resources that they may have
used during the benchmark.
Most system resources would be automatically
released on process exit, such as file
descriptors and shared memory segments,
but some resources such as temporary files
might need to be explicitly released by
the benchmark.
.NH 2
Scheduler transients
.LP
Particularly on multi-processor systems, side-effects
of process migration can dramatically affect program 
runtimes.  For example, if the processes are all
initially assigned to the same processor as the parent
process, and the timing is done before the scheduler
migrates the processes to other available processors,
then the system performance will appear to be that of
a uniprocessor.  Similarly, if the scheduler is
over-enthusiastic about re-assigning processes to
processors, then performance will be worse than
necessary because the processes will keep encountering
cold caches and will pay exhorbitant memory access
costs.
.LP
The first case is a scheduler transient, and users
may not want to measure such transient phenomena
if their primary interest is in predicting performance
for long-running programs.  Conversely, that same
user would be extraordinarily interested in the
second phenomena.  The harness was designed to
allow users to specify that the benchmarked processes
are run for long enough to (hopefully) get the
scheduler past the transient startup phase, so it
can measure the steady-state behavior.
.NH 1
Interface
.LP
Unfortunately we had to move away from the
macro-based timing harness used in \*[lmbench2] 
and migrate to a function-based system.  
.LP
The new interface looks like:
.DS
typedef void (*bench_f)(uint64 iterations, 
			void* cookie);
typedef void (*support_f)(void* cookie);

extern void benchmp(support_f initialize,
		bench_f benchmark,
		support_f cleanup,
		int enough,
		int parallel,
		int warmup,
		int repetitions,
		void* cookie);
.DE
.LP
A brief description of the parameters:
.IP \fIenough\fR
Enough can be used to ensure that a timing interval is at
least 'enough' microseconds in duration.  For most benchmarks
this should be zero, but some benchmarks have to run for more
time due to startup effects or other strange behavior.
.IP \fIparallel\fR
is simply the number of instances of the benchmark
that will be run in parallel on the system.  
.IP \fIwarmup\fR
can be used to force the benchmark to run for warmup
microseconds before the system starts making timing measurements.
Note that it is a lower bound, not a fixed value, since it
is simply the time that the parent sleeps after receiving the
last "ready" signal from each child (and before it sends 
the "go" signal to the children).  
.IP \fIrepetitions\fR
is the number of times the experiment should
be repeated.  The default is eleven.
.IP \fIcookie\fR
is a pointer that can be used by the benchmark
writer to pass in configuration information, such as buffer
size or other parameters needed by the inner loop.  
In \*[lmbench3] it is generally used to point
to a structure containing the relevant configuration
information.
.LP
To write a simple benchmark for getppid() all you would need
to do is:
.DS
void
benchmark_getppid(uint64 iterations, 
			void* cookie)
{
	while (iterations-- > 0) {
		getppid();
	}
}
.DE
.LP
and then somewhere in your program you might call:
.DS
benchmp(NULL, benchmark_getppid, NULL, 
	0, 1, 0, NULL);
micro("getppid", get_n());
.DE
.LP
A more complex example which has "state" and uses the 
initialization and cleanup capabilities might look something
like this:
.DS
struct bcopy_state {
	int len;
	char* src;
	char* dst;
};
.DE
.DS
void
initialize_bcopy(void* cookie)
{
	struct bcopy_state* state = 
		(struct bcopy_state*)cookie;

	state->src = valloc(state->len);
	state->dst = valloc(state->len);

	bzero(src, state->len);
	bzero(src, state->len);
}
.DE
.DS
void
benchmark_bcopy(uint64 iterations, 
		void* cookie)
{
	struct bcopy_state* state = 
		(struct bcopy_state*)cookie;

	while (iterations-- > 0) {
		bcopy(state->src, 
		      state->dst, state->len);
	}
}
.DE
.DS
void
cleanup_bcopy(void* cookie)
{
	struct bcopy_state* state = 
		(struct bcopy_state*)cookie;

	free(state->src);
	free(state->dst);
}
.DE
.LP
and then your program look something like:
.DS
#include "bench.h"
int
main()
{
	struct bcopy_state state;

	state.len = 8 * 1024 * 1024;
	benchmp(initialize_bcopy, 
		benchmark_bcopy, 
		cleanup_bcopy, 
		0, 1, 0, TRIES, &state);
	fprintf(stderr, "bcopy: ");
	mb(state.len * get_n());
	exit(0);
}
.DE
.LP
Note that this particular micro-benchmark would measure
cache-to-cache \*[bcopy] performance unless the amount of
memory being copied was larger than half the cache size.
A slightly more sophisticated approach might allocate
as much memory as possible and then \*[bcopy] from one
segment to another, changing segments within the allocated
memory before each \*[bcopy] to defeat the caches.
.NH 1
Benchmarks
.LP
\*[lmbench] contains a large number of micro-benchmarks
that measure various aspects of hardware and operating
system performance.  The benchmarks generally measure
latency or bandwidth, but some new benchmarks also
measure parallelism.
.NH 2
File System
.LP
A number of the benchmarks measure aspects of file system
performance, such as bw_file_rd, bw_mmap_rd, lat_mmap, and 
lat_pagefault.
It is not immediately apparent how these benchmarks should
be extended to the parallel domain.  For example, it may
be important to know how file system performance scales
when multiple processes are reading the same file, or
when multiple processes are reading different files.
The first case might be important for large, distributed 
scientific calculations, while the second might be more 
important for a web server.
.LP
However, for the operating system, the two cases are
significantly different.  When multiple processes
access the same file, access to the kernel data 
structures for that file must be coordinated and
so contention and locking of those structures can
impact performance, while this is less true when
multiple processes access different files.
.LP
In addition, there are any number of issues associated
with ensuring that the benchmarks are either measuring
operating system overhead (e.g., that no I/O is actually
done to disk), or actually measuring the system's I/O
performance (e.g., that the data cannot be resident in
the buffer cache).  Especially with file system related
benchmarks, it is very easy to develop benchmarks that
compare apples and oranges (e.g., the benchmark includes
the time to flush data to disk on one system, but only
includes the time to flush a portion of data to disk on
another system).
.LP
\*[lmbench3] allows the user to measure either case
as controlled by a command-line switch.  When measuring
accesses to independent files, the benchmarks first
create their own private copies of the file, one for
each child process.  Then each process accesses its
private file.  When measuring accesses to a single
file, each child simply uses the designated file
directly.
.NH 2
Context Switching
.LP
Measuring context switching accurately is a difficult
task.  \*[lmbench] versions 1 and 2 measured context
switch times via a "hot-potato" approach using pipes
connected in a ring.  However, this experimental
design heavily favors schedulers that do "hand-off"
scheduling, since at most one process is active at
a time.
Consequently, it is not really a good benchmark
for measuring scheduler overhead in multi-processor
machines.
.LP
The design and methodology for measuring context
switching and scheduler overhead need to be revisited
so that it can more accurately measure performance
for multi-processor machines.
.NH 2
Stream
.LP
\*[lmbench3] includes a new micro-benchmark which
measures the performance of John McCalpin's \*[stream]
benchmark kernels for \*[stream] versions 1 and 2.
This benchmark faithfully recreates each of the
kernel operations from both \*[stream] benchmarks,
and because of the powerful new timing harness it
can easily measure memory system scalability.
.TSTART 1
.TS
center box tab (|);
c s s s s
l | l | l | l | l .
Stream
_
Kernel|Code|Bytes|Bytes|FLOPS
||read|write|
_
COPY|$a[i]=b[i]$|8(+8)|8|0
SCALE|$a[i]=q times b[i]$|8(+8)|8|1
ADD|$a[i]=b[i]+c[i]$|16(+8)|8|1
TRIAD|$a[i]=b[i]+q times c[i]$|16(+8)|8|2
.TE
.TS
center box tab (|);
c s s s s
l | l | l | l | l .
Stream2
_
Kernel|Code|Bytes|Bytes|FLOPS
||read|write|
_
FILL|$a[i]=q$|0(+8)|8|0
COPY|$a[i]=b[i]$|8(+8)|8|0
DAXPY|$a[i]=a[i]+q times b[i]$|16|8|2
SUM|$sum=sum + a[i]$|8|0|1
.TE
.TEND "Stream operations"
.LP
Table 2 shows the four kernels for each version
of the \*[stream] benchmark.  Note that the
.I read
columns include numbers in parenthesis, which
represent the average number of bytes read into 
the cache as a result of the write to that
variable.  Cache lines are almost invariably
bigger than a single double, and so when a
write miss occurs the cache will read the line
from memory and then modify the selected bytes.
Sometimes vector instructions such as SSE
and 3DNow can avoid this load by writing an 
entire cache line at once.
.NH 2
Basic operation latency
.LP
\*[lmbench3] includes a new micro-benchmark 
which measures the latency for a variety of basic
operations, such as addition, multiplication, and
division of integer, float, and double operands.
To measure the basic operation latency we construct
a basic arithmetic statement containing the desired
operands and operations.  This statement is repeated
one hundred times and these repetitions are then
embedded in a loop.  The statements used are shown
in Table 3 below.
.TSTART
.TS
center box tab (&);
c c c
l & l & l .
Operand&Operation&Statement
_
int&$bit$&r^=i;s^=r;r|=s;
&$add$&a+=b;b-=a;
&$mul$&r=(r*i)^r;
&$div$&r=(r/i)^r;
&$mod$&r=(r%i)^r;
_
float&$add$&f+=f;
&$mul$&f*=f;
&$div$&f=g/f;
_
double&$add$&f+=f;
&$mul$&f*=f;
&$div$&f=g/f;
.TE
.TEND "lat_ops statements"
.LP
Each statement has been designed to ensure that
the statement instances are \fIinterlocked\fR,
namely that the processor cannot begin processing
the next instance of the statement until it has
completed processing the previous instance.  This
property is crucial to the correct measurement of
operation latency.
.LP
One important consideration in the design of
the statements was that they not be optimized
out of the loop by intelligent compilers.  
Since the statements are repeated one hundred
times, the compiler has the option of evaluating
the sequence of one hundred repetitions of the
same statement, and sometimes it can find
optimizations that are not immediately 
apparent.  For example, the integer statement
$a=a+a;$ when repeated one hundred times in
a loop can be replaced with the single statement
$a=0;$ because the statement $a=a+a;$ is equivalent
to $a<<=1;$, and one hundred repetitions of that
statement is equivalent to $a<<=100;$, which for
32bit (or even 64bit) integers is equivalent to
$a=0;$.  
.LP
It is relatively easy to identify floating
point statements that interlock, are not
optimized away, and that only use the operation
of interest.
It is much harder to identify integer statements
meeting the same criterion.  All simple 
integer bitwise operations can either be optimized
away, don't interlock, or use operations other
than one of interest.
We chose to add operations other than the 
operation(s) of interest to the statements.
.LP
The integer $mul$, $div$, and $mod$ statements all 
include an added $xor$ operation which prevents
(current) compilers from optimizing the statements
away.  Since the $xor$ operation is generally
completed in a single clock tick, and since
we can measure the $xor$ operation latency
separately and subtract that overhead, we can
still measure the latencies of the other 
operations of interest.
.LP
It is not possible to measure latency for 64bit
operations on 32bit machines because most
implementations allow operations on the upper
and lower bits to overlap.  This means that
on most 32bit machines, the measured latency
would appear to be a non-integral multiple of
the basic clock cycle.  For example, in the
$mul$ statement, the system could first add
the two lower words.  Then, in parallel it
could both add the two upper words (along with
the carry from the lower words), and compute
the $xor$ of the lower word.  Finally, it
can overlap the $xor$ of the upper word
with the addition of the two lower words from
the next instantiation of the statement.
.TSTART
.TS
center box tab (&);
c c c c
c c c c
l & l & r & r .
Operand&Operation&PA-RISC 2.0&PIII
&&180MHz&667MHz
_
int&$bit$&&1.16ns
&$add$&&2.36ns
&$mul$&&6.07ns
&$div$&&58.52ns
&$mod$&&65.01ns
_
float&$add$&&4.58ns
&$mul$&&7.50ns
&$div$&&35.26ns
_
double&$add$&&4.53ns
&$mul$&&7.71ns
&$div$&&35.51ns
.TE
.TEND "lat_ops results"
.LP
Table \n[TABLE] contains some sample results
for two processors.  
It does contain one result which is slightly
surprising unless you are familiar with the
PA-RISC architecture: floating point multiply
and divide are faster than the corresponding
integer operations!  This is because PA-RISC
does not contain integer MUL, DIV, or MOD
instructions and the optimizing compiler
converts the integers into floating point,
does the operations in the floating point
unit, and then converts the result back
into an integer.
.NH 2
Basic operation parallelism
.LP
Instruction-level parallelism in commodity processors
has become commonplace in the last ten years.  
Modern processors typically have more than one
operational unit that can be active during a
given clock cycle, such as an integer arithmetic
unit and a floating point unit.  In addition, 
processors may have more than a single instance
of a given type of operational unit, both of
which may be active at a given time.  All this
intra-processor parallelism is used to try and
reduce the average number of clock cycles per
executed instruction.  
.LP
\*[lmbench3] incorporates a new benchmark \*[par_ops]
which attempts to quantify the level of available
instruction-level parallelism provided by the processor.  This 
benchmark is very similar to \*[lat_ops], and
in fact uses the same statement kernels, but it
has been modified and extended.  We create
different versions of each benchmark; each
version has $N$ sets of interleaved statements.
Each set is identical to equivalent \*[lat_ops]
statements.  In this way multiple independent
sets can be executing the same operation(s)
in parallel, if the hardware supports it.
.LP
For example, the float $mul$ benchmark to measure
performance with two parallel streams of statements
would look like something this:
.DS
#define TEN(a) a a a a a a a a a a
void benchmark_1(iter_t iterations, void* cookie)
{
    register iter_t i = iterations;
    struct _state* state = (struct _state*)cookie;
    register float f0 = state->float_data[0];
    register float f1 = state->float_data[1];

    while (i-- > 0) {
        TEN(f0*=f0; f1*=f1;)
    }
    use_int((int)f0);
    use_int((int)f1);
}
.DE
.LP
If the processor had two floating point multiply
units, then both $f0$ and $f1$ multiplies could
proceed in parallel.
.LP
However, there are some potential problems with
the integer operations, namely the fact that the
statements contain mixed operations.  In general,
processors have at least as many integer units
that can do $xor$ as can do the other operations
of interest ($mul$, $div$ and $mod$), so the
inclusion of $xor$ in the statements shouldn't
be a bottleneck.  
.LP
However, since parallelism is measured by comparing 
the latency of the single-stream with that of 
multiple interleaved streams, and since the single-stream 
latency includes the $xor$ latency, the apparent 
parallelism of $mul$, $div$, $mod$ can be over-stated.
For example, if a process has one unit that can
do integer bit operations, such as $xor$, and another
unit for integer $mul$ operations, then the average
latency for $a0 = (i * a0) ^ a0$ in the single stream 
case would be:
.EQ
t bar = t sub xor + t sub mul
.EN
In the multi-stream case, the execution of the $xor$ 
operation of one stream can be overlapped with the 
$mul$ of another stream, so the average latency per 
stream would simply be $t bar = t sub mul$, assuming 
that $mul$ operations are not cheaper than $xor$ 
operations, which results in an apparent parallelism 
$p tilde$:
.EQ
p tilde = {t sub xor + t sub mul} over { t sub mul }
.EN
Assuming that $t sub xor << t sub mul$, this
still gives a reasonable approximation to
the correct answer.  Unfortunately, this is
not always a reasonable assumption.
.LP
Of course, if it was known ahead of time that
$xor$ and { $mul$, $div$, and $mod$ } used
different execution units, then the benchmark
could simply subtract $t sub xor$ from the
baseline measurement.  The difficulty lies
in determining whether the units overlap
or not.
.TSTART
.TS
center box tab (&);
c c c c
c c c c
l & l & r & r .
Operand&Operation&PA-RISC 2.0&PIII
&&180MHz&667MHz
_
int&$bit$&&1.00
&$add$&&1.61
&$mul$&&3.81
&$div$&&1.20
&$mod$&&1.11
_
float&$add$&&1.00
&$mul$&&1.14
&$div$&&1.03
_
double&$add$&&1.08
&$mul$&&1.00
&$div$&&1.03
.TE
.TEND "par_ops results"
.LP
.NH 1
Unscalable benchmarks
.LP
There are a number of benchmarks which either
did not make sense for scalable load, such as
\*[mhz], and other benchmarks which could not
be extended to measure scalable load due to
other constraints, such as \*[lat_connect].
.LP
\*[mhz] measures the processor clock speed,
which is not a scalable feature of the system,
so it doesn't make any sense to create a
version of it that measures scalable performance.
.LP
More specifically, \*[lat_connect] measures
the latency of connecting to a TCP socket.
TCP implementations have a timeout on
sockets and there is generally a fixed size
queue for sockets in the TIMEOUT state.  
This means that once the queue has been 
filled by a program connecting and closing
sockets as fast as possible, then all new
socket connections have to wait TIMEOUT
seconds.  Needless to say, this gives no
insight into the latency of socket creation
per se, but is rather a boring artifact.
Since the \*[lmbench2] version of the
benchmark can run for very short periods
of time, it generally does not run into
this problem and is able to correctly
measure TCP connection latency.  
.LP
Any scalable version of the benchmark needs 
each copy to run for at least a second, and 
there are $N$ copies creating connections as 
fast as possible, so it would essentially be
guaranteed to run into the TIMEOUT problem.
Consequently, \*[lat_connect] was not
enhanced to measure scalable performance.
.NH 1
Conclusion
.LP
\*[lmbench] is a useful, portable micro-benchmark 
suite designed to measure important aspects of 
system performance.
\*[lmbench3] adds a number of important extensions,
such as the ability to measure system scalability.
.NH 1
Acknowledgments
.LP
Many people have provided invaluable help and insight into both the
benchmarks themselves and the paper.  The \s-1USENIX\s0 reviewers
were especially helpful.
We thank all of them
and especially thank:
Wayne Scott \s-1(BitMover)\s0,
Larry McVoy \s-1(BitMover)\s0,
and
Bruce Chapman \s-1(SUN)\s0.
.LP
We would also like to thank all of the people that have run the
benchmark and contributed their results; none of this would have been possible
without their assistance.
.LP
Our thanks to 
all of the free software community for tools that were used during this
project.
\*[lmbench] is currently developed on Linux, a copylefted Unix written by 
Linus Torvalds and his band of happy hackers.
This paper and all of the 
\*[lmbench] documentation was produced using
the \f(CWgroff\fP suite of tools written by James Clark.
Finally, all of the data processing of the results is done with
\f(CWperl\fP written by Larry Wall.  
.NH 1
Obtaining the benchmarks
.LP
The benchmarks are available at
.ft I
http://ftp.bitmover.com/lmbench
.ft
.\" .R1
.\" bibliography references-lmbench3
.\" .R2
.\"********************************************************************
.\" Redefine the IP paragraph format so it won't insert a useless line
.\" break when the paragraph tag is longer than the indent distance
.\"
.de @IP
.if \\n[.$]>1 .nr \\n[.ev]:ai (n;\\$2)
.par*start \\n[\\n[.ev]:ai] 0
.if !'\\$1'' \{\
.	\" Divert the label so as to freeze any spaces.
.	di par*label
.	in 0
.	nf
\&\\$1
.	di
.	in
.	fi
.	chop par*label
.	ti -\\n[\\n[.ev]:ai]u
.	ie \\n[dl]+1n<=\\n[\\n[.ev]:ai] \\*[par*label]\h'|\\n[\\n[.ev]:ai]u'\c
.	el \{\
\\*[par*label]
.\".	br
.	\}
.	rm par*label
.\}
..
.\"********************************************************************
.\" redefine the way the reference tag is printed so it is enclosed in
.\" square brackets
.\"
.de ref*end-print
.ie d [F .IP "[\\*([F]" 2
.el .XP
\\*[ref*string]
..
.\"********************************************************************
.\" Get journal number entries right.  Now will print as V(N) rather
.\" than the awful V, N.
.\"
.de ref*add-N
.ref*field N "" ( ) 
..
.\"********************************************************************
.\" Get journal volume entries right.  Now will print as V(N) rather
.\" than the awful V, N.
.\"
.de ref*add-V
.ref*field V , "" "" ""
..
.\"********************************************************************
.\" Get the date entry right.  Should not be enclosed in parentheses.
.\"
.de ref*add-D
.ref*field D ","
..
.R1
accumulate
sort A+DT
database references-lmbench3
label-in-text
label A.nD.y-2
bracket-label [ ] ", "
bibliography references-lmbench3
.R2
.\" .so bios
