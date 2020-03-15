#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "uproc.h"

static char *states[] = {
[UNUSED]    "unused",
[EMBRYO]    "embryo",
[SLEEPING]  "sleep ",
[RUNNABLE]  "runble",
[RUNNING]   "run   ",
[ZOMBIE]    "zombie"
};

#ifdef CS333_P3
struct ptrs {
  struct proc* head;
  struct proc* tail;
};
#endif

#ifdef CS333_P3
#define statecount NELEM(states)
#endif

static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  #ifdef CS333_P3
  struct ptrs list[statecount];
  #endif
  #ifdef CS333_P4
  struct ptrs ready[MAXPRIO+1];
  uint PromoteAtTime;
  #endif
} ptable;

static struct proc *initproc;

uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);

#ifdef CS333_P3
static void initProcessLists(void);
static void initFreeList(void);
static void stateListAdd(struct ptrs*, struct proc*);
static int stateListRemove(struct ptrs*, struct proc* p);
static void assertState(struct proc*, enum procstate);
#endif

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
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
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
  #ifdef CS333_P3
  int found = 0;
  if(ptable.list[UNUSED].head){              //Found an unused process, returning head
    p = ptable.list[UNUSED].head;
    found = 1;
  }
  else{                                  //No unused processes, releasing lock and returning 0
    release(&ptable.lock);
    return 0;
  }
  if(found == 1) {
    int check = stateListRemove(&ptable.list[p->state], p);
    if(check == -1){
      panic("stateListRemove failed!");
    }
    assertState(p, UNUSED);
    p->state = EMBRYO;
    stateListAdd(&ptable.list[p->state], p);
    p->pid = nextpid++;
    release(&ptable.lock);
  }
  if((p->kstack = kalloc()) == 0){
    acquire(&ptable.lock);
    int check = stateListRemove(&ptable.list[p->state], p);
    if(check == -1){
      panic("stateListRemove failed!");
    }
    assertState(p, EMBRYO);
    p->state = UNUSED;
    stateListAdd(&ptable.list[p->state], p);
    release(&ptable.lock);
    return 0;
   
  }
  

  #else

  int found = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  if (found == 0) {
    release(&ptable.lock);
    return 0;
  }
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);


  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  #endif
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
  
  p->start_ticks = ticks;
  
  #ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
  #endif
  #ifdef CS333_P4
  p->priority = MAXPRIO;
  p->budget = DEFAULT_BUDGET;
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
  
  #ifdef CS333_P3
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
  #endif
  #ifdef CS333_P4
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  #endif
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
  #ifdef CS333_P4
  acquire(&ptable.lock);
  int check = stateListRemove(&ptable.list[p->state], p);
  if(check == -1){
    panic("stateListRemove failed!");
  }
  assertState(p, EMBRYO);
  p->state = RUNNABLE;
  stateListAdd(&ptable.ready[p->priority], p);
  release(&ptable.lock);

  #elif defined(CS333_P3)
  acquire(&ptable.lock);
  int check = stateListRemove(&ptable.list[p->state], p);
  if(check == -1){
    panic("stateListRemove failed!");
  }
  assertState(p, EMBRYO);
  p->state = RUNNABLE;
  stateListAdd(&ptable.list[p->state], p);
  release(&ptable.lock);

  #else
  acquire(&ptable.lock);
  p->state = RUNNABLE;
  release(&ptable.lock);
  #endif

  #ifdef CS333_P2
  p->uid = DEF_UID;
  p->gid = DEF_GID;
  #endif
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
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
 #ifdef CS333_P3   
   if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    acquire(&ptable.lock);
    int check = stateListRemove(&ptable.list[np->state], np);
    if(check == -1){
      panic("stateListRemove failed!");
    }
    assertState(np, EMBRYO);
    np->state = UNUSED;
    stateListAdd(&ptable.list[np->state], np);
    release(&ptable.lock);
    return -1;
  }
  
  #else
   if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  #endif
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  #ifdef CS333_P2
  np->uid = np->parent->uid;
  np->gid = np->parent->gid;
  #endif
  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
  #ifdef CS333_P4
  acquire(&ptable.lock);
  int check = stateListRemove(&ptable.list[np->state], np);
  if(check == -1){
    panic("stateListRemove failed!");
  }
  assertState(np, EMBRYO);
  np->state = RUNNABLE;
  stateListAdd(&ptable.ready[np->priority], np);
  release(&ptable.lock);
  
  #elif defined(CS333_P3)
  acquire(&ptable.lock);
  int check = stateListRemove(&ptable.list[np->state], np);
  if(check == -1){
    panic("stateListRemove failed!");
  }
  assertState(np, EMBRYO);
  np->state = RUNNABLE;
  stateListAdd(&ptable.list[np->state], np);
  release(&ptable.lock);

  #else
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
  #endif

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifdef CS333_P4

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
  for(int i = 0; i <= MAXPRIO; i++){
    p = ptable.ready[i].head;
    while(p){
      if(p->parent == curproc){
        p->parent = initproc;
      }
    p = p->next;
    }
  }
  for(int i = EMBRYO; i <= ZOMBIE; i++){
    p = ptable.list[i].head;
    while(p){
      if(p->parent == curproc){
        p->parent = initproc;
        if(p->state == ZOMBIE){
        wakeup1(initproc);
        }
      }
    p = p->next;
    }
  }
  // Jump into the scheduler, never to return.
  int check = stateListRemove(&ptable.list[curproc->state], curproc);
  if(check == -1){
    panic("stateListRemove failed!");
  }
  assertState(curproc, RUNNING);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[curproc->state], curproc);
  sched();
  panic("zombie exit");
}

#elif defined(CS333_P3)

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
  for(int i = EMBRYO; i <= ZOMBIE; i++){
    p = ptable.list[i].head;
    while(p){
      if(p->parent == curproc){
        p->parent = initproc;
        if(p->state == ZOMBIE){
        wakeup1(initproc);
        }
      }
    p = p->next;
    }
  }
  // Jump into the scheduler, never to return.
  int check = stateListRemove(&ptable.list[curproc->state], curproc);
  if(check == -1){
    panic("stateListRemove failed!");
  }
  assertState(curproc, RUNNING);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[curproc->state], curproc);
  sched();
  panic("zombie exit");
}

#else
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
      if(p->state == ZOMBIE){
       wakeup1(initproc);
      }
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#endif
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.

#ifdef CS333_P4
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(int i = 0; i <= MAXPRIO; i++){
      p = ptable.ready[i].head;
      while(p){
        if(p->parent != curproc){
          p = p->next;
          continue;
        }
         havekids = 1;
        if(p->state == ZOMBIE){
          // Found one.
          pid = p->pid;
          kfree(p->kstack);
          p->kstack = 0;
          freevm(p->pgdir);
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;
          int check = stateListRemove(&ptable.list[p->state], p);
          if(check == -1){
            panic("stateListRemove failed!");
          }
          assertState(p, ZOMBIE);
          p->state = UNUSED;
          stateListAdd(&ptable.list[p->state], p);
          release(&ptable.lock);
          return pid;
        }
      p = p->next; 
      }
    }
    for(int i = EMBRYO; i <= ZOMBIE; i++){
      p = ptable.list[i].head;
      while(p){
        if(p->parent != curproc){
          p = p->next;
          continue;
        }
        havekids = 1;
        if(p->state == ZOMBIE){
          // Found one.
          pid = p->pid;
          kfree(p->kstack);
          p->kstack = 0;
          freevm(p->pgdir);
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;
          int check = stateListRemove(&ptable.list[p->state], p);
          if(check == -1){
            panic("stateListRemove failed!");
          }
          assertState(p, ZOMBIE);
          p->state = UNUSED;
          stateListAdd(&ptable.list[p->state], p);
          release(&ptable.lock);
          return pid;
        }
      p = p->next; 
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

#elif defined(CS333_P3)
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(int i = EMBRYO; i <= ZOMBIE; i++){
      p = ptable.list[i].head;
      while(p){
        if(p->parent != curproc){
          p = p->next;
          continue;
        }
        havekids = 1;
        if(p->state == ZOMBIE){
          // Found one.
          pid = p->pid;
          kfree(p->kstack);
          p->kstack = 0;
          freevm(p->pgdir);
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;
          int check = stateListRemove(&ptable.list[p->state], p);
          if(check == -1){
            panic("stateListRemove failed!");
          }
          assertState(p, ZOMBIE);
          p->state = UNUSED;
          stateListAdd(&ptable.list[p->state], p);
          release(&ptable.lock);
          return pid;
        }
      p = p->next; 
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


#else
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
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

#endif
//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

#ifdef CS333_P4	
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
      acquire(&ptable.lock);
      if(ticks >= ptable.PromoteAtTime){      //Promoting
        struct proc* current;
        for(int i = MAXPRIO; i >= 0; i--){     //Checking ready lists, promoting if not at MAXPRIO, resetting budgets
          current = ptable.ready[i].head;
          while(current){
            struct proc* curnext = current->next;
            if(current->priority < MAXPRIO){
              int check = stateListRemove(&ptable.ready[i], current);
              if(check == -1){
                panic("stateListRemove failed!");
              }
              assertState(current, RUNNABLE);
              current->priority = current->priority + 1;
              current->budget = DEFAULT_BUDGET;
              stateListAdd(&ptable.ready[current->priority], current);
              current = curnext;
            }
            else{
              current->budget = DEFAULT_BUDGET;
              current = curnext;
            }
          }
        }
        current = ptable.list[SLEEPING].head;    //Checking Sleeping
        while(current){ 
          if(current->priority < MAXPRIO){             //Promoting unless priority is already MAXPRIO
            current->priority = current->priority + 1;
          }
          current->budget = DEFAULT_BUDGET;
          current = current->next;
        }
        current = ptable.list[RUNNING].head;    //Checking Running
        while(current){
          if(current->priority < MAXPRIO){            //Promoting unless current priority is already MAXPRIO, resetting budgets
            current->priority = current->priority + 1;
          }
          current->budget = DEFAULT_BUDGET;
          current = current->next;
        }
      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
      }
      
      for(int i = MAXPRIO; i >= 0; i--){
        if(ptable.ready[i].head){
          p = ptable.ready[i].head;
          // Switch to chosen process.  It is the process's job
          // to release ptable.lock and then reacquire it
          // before jumping back to us.
#ifdef PDX_XV6
          idle = 0;  // not idle this timeslice
#endif // PDX_XV6
          c->proc = p;
          #ifdef CS333_P2
          p->cpu_ticks_in = ticks;
          #endif
          switchuvm(p);
          int check = stateListRemove(&ptable.ready[i], p);
          if(check == -1){
           panic("stateListRemove failed!");
          }
          assertState(p, RUNNABLE);
          p->state = RUNNING;
          stateListAdd(&ptable.list[p->state], p);
          swtch(&(c->scheduler), p->context);
          switchkvm();
          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->proc = 0;
          break;
          }   
      }
      release(&ptable.lock);
      
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}

#elif defined(CS333_P3)
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
      acquire(&ptable.lock);
      if(ptable.list[RUNNABLE].head){
        p = ptable.list[RUNNABLE].head;
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
#ifdef PDX_XV6
        idle = 0;  // not idle this timeslice
#endif // PDX_XV6
        c->proc = p;
        #ifdef CS333_P2
        p->cpu_ticks_in = ticks;
        #endif
        switchuvm(p);
        int check = stateListRemove(&ptable.list[p->state], p);
        if(check == -1){
          panic("stateListRemove failed!");
        }
        assertState(p, RUNNABLE);
        p->state = RUNNING;
        stateListAdd(&ptable.list[p->state], p);
        swtch(&(c->scheduler), p->context);
        switchkvm();
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
       
      } 
      release(&ptable.lock);
      
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}

#else
	
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      #ifdef CS333_P2
      p->cpu_ticks_in = ticks;
      #endif
      switchuvm(p);
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      switchkvm();
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}

#endif

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
  #ifdef CS333_P2
  p->cpu_ticks_total += (ticks - p->cpu_ticks_in);
  #endif
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;

}

// Give up the CPU for one scheduling round.

#ifdef CS333_P4
void
yield(void)
{
  struct proc *curproc = myproc();
  acquire(&ptable.lock);  //DOC: yieldlock
  int check = stateListRemove(&ptable.list[curproc->state], curproc);
  if(check == -1){
    panic("stateListRemove failed!");
  }
  assertState(curproc, RUNNING);
  curproc->state = RUNNABLE;
  curproc->budget = curproc->budget - (ticks - curproc->cpu_ticks_in);
  if(curproc->budget <= 0){                   
    if(curproc->priority > 0){
      curproc->priority = curproc->priority - 1;
    }
    curproc->budget = DEFAULT_BUDGET;
  }
  stateListAdd(&ptable.ready[curproc->priority], curproc);
  sched();
  release(&ptable.lock);
}

#elif defined(CS333_P3)
void
yield(void)
{
  struct proc *curproc = myproc();
  acquire(&ptable.lock);  //DOC: yieldlock
  int check = stateListRemove(&ptable.list[curproc->state], curproc);
  if(check == -1){
    panic("stateListRemove failed!");
  }
  assertState(curproc, RUNNING);
  curproc->state = RUNNABLE;
  stateListAdd(&ptable.list[curproc->state], curproc);
  sched();
  release(&ptable.lock);
}

#else	
void
yield(void)
{
  struct proc *curproc = myproc();
  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}
#endif
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
#ifdef CS333_P4
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
 
  int check = stateListRemove(&ptable.list[p->state], p);
  if(check == -1){
    panic("stateListRemove failed!");
  }
  assertState(p, RUNNING);
  p->chan = chan;
  p->state = SLEEPING;
  p->budget = p->budget - (ticks - p->cpu_ticks_in);
  if(p->budget <= 0){                   //Demoting unless already at 0
    if(p->priority > 0){
      p->priority = p->priority - 1;
    }
    p->budget = DEFAULT_BUDGET;
  }
  stateListAdd(&ptable.list[p->state], p);
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#else

void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  #ifdef CS333_P3
  //acquire(&ptable.lock);
  int check = stateListRemove(&ptable.list[p->state], p);
  if(check == -1){
    panic("stateListRemove failed!");
  }
  assertState(p, RUNNING);
  p->chan = chan;
  p->state = SLEEPING;
  stateListAdd(&ptable.list[p->state], p);
  //release(&ptable.lock);
  #else
  p->chan = chan;
  p->state = SLEEPING;
  #endif
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#endif

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.

#ifdef CS333_P4
static void
wakeup1(void *chan)
{
  //cprintf("in wakeup1\n");
  struct proc *p = ptable.list[SLEEPING].head;
  while(p){
    if(p->state == SLEEPING && p->chan == chan){
      int check = stateListRemove(&ptable.list[p->state], p);
      if(check == -1){
        panic("stateListRemove failed!");
      }
      assertState(p, SLEEPING);
      p->state = RUNNABLE;
      stateListAdd(&ptable.ready[p->priority], p);
    }
  p = p->next; 
  }
}

#elif defined(CS333_P3)
static void
wakeup1(void *chan)
{
  //cprintf("in wakeup1\n");
  struct proc *p = ptable.list[SLEEPING].head;
  struct proc *pnext;
  while(p){
    pnext = p->next;
    if(p->state == SLEEPING && p->chan == chan){
      int check = stateListRemove(&ptable.list[p->state], p);
      if(check == -1){
        panic("stateListRemove failed!");
      }
      assertState(p, SLEEPING);
      p->state = RUNNABLE;
      stateListAdd(&ptable.list[p->state], p);
    }
  p = pnext; 
  }
}

#else
static void
wakeup1(void *chan)
{
  struct proc *p;
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#endif
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
#ifdef CS333_P4
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
    for(int i = 0; i <= MAXPRIO; i++){
      p = ptable.ready[i].head;
      while(p){
        if(pid == p->pid){
          p->killed = 1;
          release(&ptable.lock);
          return 0;  
        }
      p = p->next;
      }
    }
       
    for(int i = EMBRYO; i < ZOMBIE; i++){
      p = ptable.list[i].head;
      while(p){
        if(p->pid == pid){
          p->killed = 1;
          if(p->state == SLEEPING){
            int check = stateListRemove(&ptable.list[p->state], p);
            if(check == -1){
              panic("stateListRemove failed!");
            }
            assertState(p, SLEEPING);
            p->state = RUNNABLE;
            stateListAdd(&ptable.ready[p->priority], p);
            }
          release(&ptable.lock);
          return 0;
        }
      p = p->next; 
      }
    }

  release(&ptable.lock);
  return -1;
}

#elif defined(CS333_P3)
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  
    for(int i = EMBRYO; i < ZOMBIE; i++){
      p = ptable.list[i].head;
      while(p){
        if(p->pid == pid){
          p->killed = 1;
          if(p->state == SLEEPING){
            int check = stateListRemove(&ptable.list[p->state], p);
            if(check == -1){
              panic("stateListRemove failed!");
            }
            assertState(p, SLEEPING);
            p->state = RUNNABLE;
            stateListAdd(&ptable.list[p->state], p);
            }
          release(&ptable.lock);
          return 0;
        }
      p = p->next; 
      }
    }

  release(&ptable.lock);
  return -1;
}

#else
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
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif
//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.

#ifdef CS333_P4
void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
 cprintf("\nPID\tName\tUID\tGID\tPPID\tPRIO\tElapsed\tCPU\tState\tSize\t PCs\n");
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
  
    int d1 = ((ticks-p->start_ticks)/100)%10;
    int d2 = ((ticks-p->start_ticks)/10)%10;
    int d3 = ((ticks-p->start_ticks))%10;
    int cpu1 = ((p->cpu_ticks_total)/100)%10;
    int cpu2 = ((p->cpu_ticks_total)/10)%10;
    int cpu3 = p->cpu_ticks_total%10;
   
    int ppid;
    if(p->parent)
      ppid = p->parent->pid;
    else
      ppid = p->pid;

    cprintf("%d\t%s\t%d\t%d\t%d\t%d\t%d.%d%d%d\t%d.%d%d%d\t%s\t%d\t", p->pid, p->name, p->uid, p->gid, ppid, p->priority, (ticks-p->start_ticks)/1000, d1, d2, d3, p->cpu_ticks_total/1000, cpu1, cpu2, cpu3, state, p->sz);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
      
  }
}
#elif defined(CS333_P2)
void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
 cprintf("\nPID\tName\tUID\tGID\tPPID\tElapsed\tCPU\tState\tSize\t PCs\n");
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
  
    int d1 = ((ticks-p->start_ticks)/100)%10;
    int d2 = ((ticks-p->start_ticks)/10)%10;
    int d3 = ((ticks-p->start_ticks))%10;
    int cpu1 = ((p->cpu_ticks_total)/100)%10;
    int cpu2 = ((p->cpu_ticks_total)/10)%10;
    int cpu3 = p->cpu_ticks_total%10;
   
    int ppid;
    if(p->parent)
      ppid = p->parent->pid;
    else
      ppid = p->pid;

    cprintf("%d\t%s\t%d\t%d\t%d\t%d.%d%d%d\t%d.%d%d%d\t%s\t%d\t", p->pid, p->name, p->uid, p->gid, ppid,(ticks-p->start_ticks)/1000, d1, d2, d3, p->cpu_ticks_total/1000, cpu1, cpu2, cpu3, state, p->sz);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
      
  }
}
#else

void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  cprintf("\nPID\tName\t\tElapsed\t\tState\tSize\t PCs\n");
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    
    int d1 = ((ticks-p->start_ticks)/100)%10;
    int d2 = ((ticks-p->start_ticks)/10)%10;
    int d3 = ((ticks-p->start_ticks))%10;
 
    cprintf("%d\t%s\t\t%d.%d%d%d\t\t%s\t%d\t", p->pid, p->name, (ticks-p->start_ticks)/1000, d1, d2, d3, state, p->sz);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }  
}

#endif
#ifdef CS333_P2
int
getprocs(uint max, struct uproc* tab)
{
  
  acquire(&ptable.lock);
  
  int i = 0;
  struct proc *p;
  
  for(p = ptable.proc; p < &ptable.proc[NPROC] && i < max; p++){
      	  
    if(p->state == UNUSED || p->state == EMBRYO)
      continue;
    else{
      tab[i].pid = p->pid;                               //Process ID
      safestrcpy(tab[i].name, p->name, sizeof(p->name)); //Name
      tab[i].gid = p->gid;                               //Group ID
      tab[i].uid = p->uid;                               //User ID
      #ifdef CS333_P4
      tab[i].priority = p->priority;
      #endif
      if(!p->parent){
        tab[i].ppid = p->pid;                            //No parent, parent ID
      }
      else{ 
      tab[i].ppid = p->parent->pid;                      //Has parent, parent ID
      }
      
      tab[i].elapsed_ticks = (ticks-p->start_ticks);     //Elapsed
      tab[i].CPU_total_ticks = p->cpu_ticks_total;       //CPU elapsed
      
      if(p->state == RUNNABLE)                           //States
	safestrcpy(tab[i].state, "runble", STRMAX);
      if(p->state == SLEEPING)
        safestrcpy(tab[i].state, "sleep", STRMAX);
      if(p->state == RUNNING)
        safestrcpy(tab[i].state, "run", STRMAX);
      if(p->state == ZOMBIE)
        safestrcpy(tab[i].state, "zombie", STRMAX);
      
      tab[i].size = p->sz;                               //Size
    }
    i++;
  } 
  release(&ptable.lock);
  return i;
}
#endif

#ifdef CS333_P3
static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p->next = NULL;
  } else{
    ((*list).tail)->next = p;
    (*list).tail = ((*list).tail)->next;
    ((*list).tail)->next = NULL;
  }
}

static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL){
    return -1;
  }
  
  struct proc* current = (*list).head;
  struct proc* previous = 0;
  if(current == p){
    (*list).head = ((*list).head)->next;
    // prevent tail remaining assigned when we’ve removed the only item
    // on the list
    if((*list).tail == p){
      (*list).tail = NULL;
    }
    return 0;
  }
  while(current){
    if(current == p){
      break;
    }
	
    previous = current;
    current = current->next;
  }
  
  // Process not found, hit eject.
  if(current == NULL){
    return -1;
  }
  
  // Process found. Set the appropriate next pointer.
  if(current == (*list).tail){
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else{
    previous->next = current->next;
  }
  
  // Make sure p->next doesn’t point into the list.
  p->next = NULL;

  return 0;
}

static void
initProcessLists()
{
  int i;
  
  for (i = UNUSED; i <= ZOMBIE; i++) {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;
  }
  #ifdef CS333_P4
  for (i = 0; i <= MAXPRIO; i++) {
    ptable.ready[i].head = NULL;
    ptable.ready[i].tail = NULL;
  }
  #endif
}

static void
initFreeList(void)
{
  struct proc* p;
  
  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  }
}


static void
assertState(struct proc *p, enum procstate state)
{
  if(p->state == state)
    return;
  else
    panic("Incorrect state!");
}

#ifdef CS333_P4
void
readylist(void)
{
  int found = 0;
  struct proc *current;
  acquire(&ptable.lock);
  cprintf("Ready List Processes: \n");
  for(int i = MAXPRIO; i >= 0; i--){
    current = ptable.ready[i].head;
      cprintf("Priority %d: ", i);
    if(current){
      found = 1;
      cprintf("(%d,%d)", current->pid, current->budget);
      if(current->next){
        cprintf(" -> ");
      }
      current = current->next;
      while(current){
        cprintf("(%d, %d)", current->pid, current->budget);
        if(current->next){
          cprintf(" -> ");
        }
        current = current->next;
      }
    }
    cprintf("\n");
  }
  cprintf("\n");
  if(found == 0)
    cprintf("Ready list empty!\n");
  release(&ptable.lock);
}

#else
void
readylist(void)
{
  struct proc *current = ptable.list[RUNNABLE].head;
  cprintf("Ready List Processes: \n");
  if(current){
    cprintf("%d", current->pid);
    current = current->next;
  
    while(current){
      cprintf(" -> ");
      cprintf("%d", current->pid);
      current = current->next;
    }
    cprintf("\n"); 
  }
  else
    cprintf("Ready list empty!\n");
}
#endif

void
freelist(void)
{
  int count = 0;
  struct proc *current = ptable.list[UNUSED].head;

  while(current){
    count++;
    current = current->next;
  }
  cprintf("Free list size: %d processes\n", count);
}

void
sleeplist(void)
{
 struct proc *current = ptable.list[SLEEPING].head;
 cprintf("Sleep List Processes: \n");
 if(current){
   cprintf("%d", current->pid);
   current = current->next;
 
   while(current){
     cprintf(" -> ");
     cprintf("%d", current->pid);
     current = current->next;
   }
   cprintf("\n"); 
 }
 else
   cprintf("Sleep list empty!\n");
}

void
zombielist(void)
{
  struct proc *current = ptable.list[ZOMBIE].head;
  cprintf("Zombie List Processes: \n");
  if(current){
    cprintf("(%d, %d)", current->pid, current->parent->pid);
    current = current->next;
  
    while(current){
      cprintf(" -> ");
      cprintf("(%d, %d)", current->pid, current->parent->pid);
      current = current->next;
    }
    cprintf("\n"); 
  }
  else
    cprintf("Zombie list empty!\n");
}
#endif

#ifdef CS333_P4

int
setpriority(int pid, int prio)
{
  if(pid < 0){
    cprintf("Invalid PID!\n");
    return -1;
  }
  
  if(prio < 0 || prio > MAXPRIO){
    cprintf("Invalid priority!  Budget and priority remaining unchanged.\n");
    return -1;
  }

  struct proc* p;
  acquire(&ptable.lock);
  for(int i = 0; i <= MAXPRIO; i++){
    p = ptable.ready[i].head;
    while(p){
      struct proc* curnext = p->next;
      if(p->pid == pid){
        if(p->priority != prio){
          int check = stateListRemove(&ptable.ready[p->priority], p);
          if(check == -1){
            panic("stateListRemove failed!");
          }
          assertState(p, RUNNABLE);
          p->priority = prio;
          p->budget = DEFAULT_BUDGET;
          stateListAdd(&ptable.ready[p->priority], p);
        }
      }
    p = curnext;
    }
  }
  for(int i = SLEEPING; i <= RUNNING; i++){
    p = ptable.list[i].head;
    while(p){
      if(p->pid == pid){
        p->priority = prio;
        p->budget = DEFAULT_BUDGET;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return 0;
}

int
getpriority(int pid)
{
  struct proc* p;
  acquire(&ptable.lock);
  for(int i = 0; i <= MAXPRIO; i++){
    p = ptable.ready[i].head;
    while(p){
      if(p->pid == pid){
        release(&ptable.lock);
        return p->priority;
      }
      p = p->next;
    }
  }
  for(int i = SLEEPING; i <= RUNNING; i++){
    p = ptable.list[i].head;
    while(p){
      if(p->pid == pid){
        release(&ptable.lock);
        return p->priority;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif
