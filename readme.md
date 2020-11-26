//install qemu on your system
//assignment done in ubuntu 18.04

Modified the following on top of already available code for xv6 operation system designed by MIT
To compile :
make clean ; make
To run;
make qemu SCHEDULER=<RR,FCFS,MLFQ,PBS> -B

TASK 1:

WAITX SYSCALL
added waitx system call whose return value is process id of process and gives us information about run time and wait time 
to add system call following files are modified:
sysproc.c , syscall.c , syscall.h , users.h, usys.S , defs.h
and  waitx call is implemented in proc.c file.

created a test file called time.c ,which executes the given file and returns total time(run+wait time),or if not passed any argument executes default program (some random sleeptime)

PS:
This user program returns some basic information about all the active processes.
for this modified the existing proc structure and added the following:
● Priority
- ​ Current priority of the process (defined as per the need of
schedulers below)
● State
- ​ Current state of the process
● r-time
- ​ Total time for which the process ran on CPU till now (use a
suitable unit of time)
● w-time
- ​ Time for which the process has been waiting (reset this to 0
whenever the process gets to run on CPU or if a change in the queue takes
place (in the case of MLFQ scheduler))
● n_run
- ​ Number of times the process was picked by the scheduler
● cur_q
- ​ Current queue (check task 2 part C)
● q{i}
- ​ Number of ticks the process has received at each of the 5
queues



TASK 2:

xv6 uses round robin as default scheduler.

Implemented firstcome-firstserve ,priority based scheduling,Multilevel feedback queue scheduling which can be choosen while compiling ,default (if not specified) goes to round-robin scheduler.

FCFS:
A non-preemptive policy that selects the process with the lowest creation time. The process runs until it no longer needs CPU time.

created a variable called ctime in proc structure whose value is ticks when it is allocated the proc.
in schduler program the one with least creation time is choosen.

Removed yield() in trap.c at timer interrupt part to make it non preemptive.

PBS:

added variable priority to proc str.
implemented a function call set_priority which  takes arguments new priority and processid ,and changes priority of process of specified process id.
a user program called setPriority which is used to do it from cli.
usage : setPriority <newpriority> <pid>
default process priority is 60.
lower values means higher priority.

scheduler implementation:
process with highest priority is choosed to run , if two processes have saame priority its its choosen in rr fashion.

MLFQ:

There are 5 queues(0 to 4, high to low priority)
struct proc *queue[5][NPROC];
- These queues contain runnable processes only.
- The function enqueue adds process to queue and dequeue functionremove process from queue functions take arguments of the process and queue number and make appropriate changes in the array(enqueue and dequeue).
we add the process into the queue whenever process state changes to runnable.
each process enters queue 0 at time of creation (enqueued),
1 tick of cpu timer is assigned as 1.
time slices of 5 queues are 1,2,4,8,16 respectively

implemented aging:(to prevent starvation)
aging checks if all any process has mean waiting for more than x ticks , if yes its promoted to upper queue.
to implemented this  added variable enter in proc structure whose value is ticks when it gets enqueued.
here x=30.

selecting processes: the processes in higher priority queue runs in fifo fashion always, if tehere is no process then next highest priority queue is selected.

after selecting processes they are removed and run, if their time slice is completed they are moved into next higher priority queue(checked in trap.c).else if process voluntarily relinquishes control of the CPU and when the process becomes ready again after the I/O, it is​ ​ inserted at the tail of the same queue, from which it is relinquished earlier​.

since all process are runnable, fifo fashion would be similar to rr fashion.(as asked for lowest priority queue)















