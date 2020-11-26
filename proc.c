#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"



struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct proc *queue[5][NPROC];
int num_proq[5]={-1,-1,-1,-1,-1};
int qlimit[5]={1,2,4,8,16};


int enqueue(struct proc *p,int que_num);
int dequeue(struct proc *p,int que_num);


int enqueue(struct proc *p,int que_num)
{
  for(int i=0; i <=num_proq[que_num]; i++)
	{
		if(p->pid == queue[que_num][i]->pid)
			return -1;
	}
	p->cur_q=que_num;
  p->enter=ticks;
	num_proq[que_num]++;
	queue[que_num][num_proq[que_num]]=p;
 // cprintf("Process %d added to Queue %d\n", p->pid, que_num);
	return 1;
}
int dequeue(struct proc *p,int que_num)
{
  int found_at=-1;
  for(int i=0; i <=num_proq[que_num]; i++)
	{
		if(p->pid == queue[que_num][i]->pid)
		{
      found_at=i;
      break;
    }	
	}
  if(found_at==-1)
    return -1;
  //remove it be shifting remaining processes  by 1 to front
  for(int j=found_at; j <= num_proq[que_num]; j++) 
  {
    queue[que_num][j]=queue[que_num][j+1];
  }
  num_proq[que_num]=num_proq[que_num]-1;  
  return 0;
}
void check_aging(void)
{
      
    for(int i=1;i<5;i++)
    {  for(int j=0;j<=num_proq[i];j++)
      {
        //check if anything is aged
        struct proc *p=queue[i][j];
        int age=ticks - p->enter;
        if(age >30)//took aging limit as 40
        {
          //promote it to higher priority queue
          dequeue(p,i);
          enqueue(p,i-1);
          //cprintf("%s with pid (%d) promoted to %d\n",p->name,p->pid,p->cur_q);
        }
      }
    } 
     // return 0;
}



static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}



//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  //edit after this
  p->ctime=ticks;//update creation time
  p->etime=0; //endtime
  p->rtime=0; //runtime
  p->iotime=0;
  p->waittime=0;
  p->twtime=0;
  p->priority=60; //set default priority
  p->n_run=0;//no.of times it ran is 0 initially
  #ifdef MLFQ
    p->cur_q=0;
    p->process_ticks=0;
    p->enter=0;
    p->flag=0;
    for(int i=0;i<=4;i++)
      p->q[i]=0;
  #endif    
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  //add to queue 0 in case of mlfq
  #ifdef MLFQ
    p->cur_q=0;
    enqueue(p,0);
  #endif

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
  //same u need to add this proces sto first queue
  #ifdef MLFQ 
    np->cur_q=0;
    enqueue(np,0);
  #endif

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  curproc->etime =ticks; //update endtime
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        #ifdef MLFQ
          dequeue(p,p->cur_q);
        #endif    
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
int 
set_priority(int new_priority,int pid)
{
  struct proc *p=0;
  int oldpr=0;
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
   {
     if(pid==p->pid)
     { 
       acquire(&ptable.lock);
       oldpr=p->priority;
       p->priority=new_priority;
       //cprintf("priority changed\n");
       release(&ptable.lock);
       break;
     }  
   }
  //release(&ptable.lock);
  if(oldpr > new_priority)
    yield(); 
 
  return oldpr;
}

// implementation of waitx
int 
waitx(int* wtime,int* rtime)
{
  struct proc *p=0;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one
        //need to write those here
        *rtime=p->rtime;
				*wtime=p->etime - p->ctime - p->rtime -p->iotime;
        pid = p->pid;
        #ifdef MLFQ
          dequeue(p,p->cur_q);
        #endif 
       // cprintf("creationtime: %d,runtime: %d,waitime: %d,iotime: %d  of process pid %d\n",p->ctime,p->rtime,*wtime,p->iotime,pid);
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
//syscall for ps
int 
psimp(void)
{
  struct proc *p;
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  //cprintf("hi ps called");
  acquire(&ptable.lock);
  cprintf("pid  priority  state  runtime   waitime   n_run   name  cur_q \t q0 \t q1 \t q2 \t q3 \t q4\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  if(p->state!=UNUSED){
    p->waittime=ticks - p->ctime - p->rtime -p->iotime;
    cprintf("%d     %d       %s   %d       %d \t %d \t %s \t %d \t %d \t %d \t %d \t %d \t %d \n",p->pid,p->priority,states[p->state],p->rtime,p->twtime,p->n_run,p->name,p->cur_q,p->q[0],p->q[1],p->q[2],p->q[3],p->q[4]);
  }  
  release(&ptable.lock);
  return 0;
}


//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;

  #ifdef RR
    cprintf("algo is RR\n");
  #endif

  #ifdef FCFS
    cprintf("algo is FCFS\n");
  #endif    
  
  #ifdef PBS
    cprintf("algo is PBS\n");
  #endif 
   #ifdef MLFQ
    cprintf("algo is MLFQ\n");
  #endif  
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    #ifdef RR
      struct proc *p=0;
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state != RUNNABLE)
          continue;

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        p->waittime=0;
        p->twtime=0;
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;
        //cprintf("%s with pid (%d) scheduled in rr\n",p->pid,p->name);
        p->n_run =p->n_run+1;
        //cprintf(" pid (%d) scheduled in rr\n",p->pid);
        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
    }
    #else
    #ifdef FCFS
      struct proc *p=0;
      struct proc *curr_proc=0;
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
      {
        if(curr_proc==0 && p->state==RUNNABLE)
          curr_proc=p;
        else if(p->ctime < curr_proc->ctime && p->state==RUNNABLE)  //check one with least creation time
          curr_proc=p; 
      }
      if(curr_proc!=0 && curr_proc->state == RUNNABLE){
      // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        p->waittime=0;
        p->twtime=0;
        p=curr_proc;
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;
      //  cprintf(" pid (%d) scheduled in fcfs\n",curr_proc->pid);
        p->n_run =p->n_run+1;
        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
    #else
    #ifdef PBS
      struct proc *p=0;
      struct proc *curr_pr_proc=0;
      //first find min.priority process
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
      {
            if (curr_pr_proc == 0 && p->state == RUNNABLE)
              curr_pr_proc = p;
            else if (curr_pr_proc->priority > p->priority && p->state==RUNNABLE)
              curr_pr_proc = p;  
      }
      //if there is no process release lock and continue to wait for a process
      if(curr_pr_proc ==0)
      {
        release(&ptable.lock);
        continue;
      }
      
      // find if any other process has high priority or same priority
      for( p= ptable.proc ; p < &ptable.proc[NPROC];p++)
      {
        struct proc *hp=0; int high_pr=0;//no higher priority process
        for(hp=ptable.proc; hp < &ptable.proc[NPROC];hp++)
        {
          if(hp->state==RUNNABLE && hp->priority< curr_pr_proc->priority)
            high_pr=1; //found a higher priority process
        }
        if(high_pr==1)
          continue; //goes and reschedules it

        //found other process with same priority(1 will surely be there, which is itself)   
        else if(p->priority == curr_pr_proc->priority && p->state==RUNNABLE) //do in rr fashion
        {
          // Switch to chosen process.  It is the process's job
          // to release ptable.lock and then reacquire it
          // before jumping back to us.
          p->waittime=0;
          p->twtime=0;
          c->proc = p;
          switchuvm(p);
          p->state = RUNNING;
          //cprintf("%s with pid (%d) scheduled in rr\n",p->pid,p->name);
          p->n_run =p->n_run+1;
        // cprintf(" pid (%d) with priority (%d)scheduled in pbs\n",p->pid,p->priority);
          swtch(&(c->scheduler), p->context);
          switchkvm();

          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->proc = 0;

        }  
      }
    #else  
    #ifdef MLFQ
      //to avoid starvation check aging 
      check_aging();
     //  cprintf("reached check point 1\n");
      struct proc *admitted_pr=0;
      //run the process in high priority queue (if present)
      
      for(int i=0;i<=4;i++)
      {
        if(num_proq[i]>=0) //0 is counted as process
        {
          //remove and run it
          admitted_pr=queue[i][0];
          //cprintf("process found\n");
          dequeue(admitted_pr,i);
          break;         //found process to proceed
        }
      }
      
      //cprintf("reached check point 2 with name \n");
      struct proc *p=0;
      if(admitted_pr!=0 && admitted_pr->state == RUNNABLE)
      {
        // Switch to chosen process.  It is the process's job
          // to release ptable.lock and then reacquire it
          // before jumping back to us.
          p=admitted_pr;
          p->process_ticks++;
          p->q[p->cur_q]++;
          p->waittime=0;
          p->twtime=0;
          c->proc = p;
          switchuvm(p);
          p->state = RUNNING;
         //cprintf("%s with pid (%d) scheduled in mlfq\n",p->name,p->pid);
          p->n_run =p->n_run+1;
          // cprintf(" pid (%d) with priority (%d)scheduled in pbs\n",p->pid,p->priority);
          swtch(&(c->scheduler), p->context);
          switchkvm();

          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->proc = 0;
          if(p!=0 && p->state == RUNNABLE)
          {
            //timeslice completed
            if(p->flag==1)
            {
              //chnge it to end of next queue 
              if(p->cur_q!=4)
                p->cur_q=p->cur_q+1;
              p->flag=0;
              p->waittime=0;
              p->twtime=0;
              
            }
            p->process_ticks=0;
            enqueue(p,p->cur_q); 
          }

      }

    #endif
    #endif
    #endif
    #endif
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {  if(p->state == SLEEPING && p->chan == chan)
    {  
      p->state = RUNNABLE;
      #ifdef MLFQ
        p->process_ticks=0;
        enqueue(p,p->cur_q);
      #endif
    } 
  }    
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
      {
          p->state = RUNNABLE;
          #ifdef MLFQ
          //p->process_ticks=0;
            enqueue(p,p->cur_q);
          #endif
      }    
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

