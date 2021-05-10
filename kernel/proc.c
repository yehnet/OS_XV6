#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sigaction.h"
#include "bsem.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;
struct bsem bsem[MAX_BSEM];

int nextpid = 1;
int nexttid = 100;
int nextsid = 1;

struct spinlock pid_lock;
struct spinlock tid_lock;
struct spinlock sid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);
static void freeThread(struct thread *t);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

//print proc\thread lock position
//NOTICE: not implemented fully (like in signals.)
void print_locks_position(int procLocks[], int threadLocks[])
{
  printf("DEBUG ------Locks:\talloc\texit\tscheduler\tyield\tsleep\tkill\n");
  printf("\t\t\t%d ", procLocks[0]);//alloc
  printf("\t%d ", procLocks[1]);//exit
  printf("\t%d ", procLocks[2]);//tscheduler
  printf("\t\t%d ", procLocks[3]);//tyield
  printf("\t%d ", procLocks[4]);//tsleep
  printf("\t%d ", procLocks[5]);//tkill

  printf("\n");

  printf("\t\t\t%d ", threadLocks[7]);//talloc
  printf("\t%d", threadLocks[1]);//texit
  printf("-%d", threadLocks[2]);//texit
  printf("\t%d ", threadLocks[3]);//tscheduler
  printf("\t\t%d ", threadLocks[4]);//tyield
  printf("\t%d ", threadLocks[5]);//tsleep
  printf("\t%d ", threadLocks[6]);//tkill
  // printf("%d ", threadLocks[i]);

  printf(" \n");
}

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    // uint64 va = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table at boot time.
void procinit(void)
{
  struct proc *p;
  struct thread *t;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  initlock(&tid_lock, "nexttid");
  initlock(&sid_lock, "nextsid");

  for (p = proc; p < &proc[NPROC]; p++)
  {
    initlock(&p->lock, "proc");
    p->kstack = KSTACK((int)(p - proc));
    //Ass2 - Task3
    for (t = p->threads; t < &p->threads[NTHREAD]; t++)
    {
      initlock(&t->lock, "thread");
      t->kstack = KSTACK((int)((p - proc) * 8 + (t - p->threads)));

      //     // p->threads[i]->kstack = KSTACK((int)(p->threads[i] - p->threads[0]));
      // t->kstack = KSTACK((int)(t - p->threads));
    }
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}
//Ass2 - Task3
// Return the current struct thred *, or zero if none.
struct thread *
myThread(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct thread *t = c->currThread;
  pop_off();
  return t;
}

int allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

int alloctid()
{
  // printf("DEBUG--------------allocid acquire \n");
  acquire(&tid_lock);
  int tid = nexttid;
  nexttid++;
  release(&tid_lock);

  return tid;
}

int allocsid()
{
  // printf("DEBUG--------------allocid acquire \n");
  acquire(&sid_lock);
  int sid = nextsid;
  nextsid++;
  release(&sid_lock);
  return sid;
}

//@proc p as the thread process
//@tnum as thread number in the process thread array
static struct thread *
allocThread(struct proc *p)
{
  //TODO
  struct thread *t;
  int i = 0;
  for (t = p->threads; t < &p->threads[NTHREAD]; t++)
  {
    // printf("DEBUG--------------allocthread acquire \n");
    acquire(&t->lock);
    t->tlocks[7] = 1;
    if (t->state == UNUSED)
    {
      t->myNum = i;
      i++;
      goto found;
    }
    else if (t->state == ZOMBIE)
    {
      freeThread(t);
      release(&t->lock);
      i++;
    }
    i++;
  }
  return 0;

found:
  t->tid = alloctid();
  t->state = USED;
  t->parent = p;
  t->killed = 0;
  t->chan = 0;
  t->bsem_id = 0;
  t->tlocks[0] = 0;
  t->tlocks[1] = 0;
  t->tlocks[2] = 0;
  t->tlocks[3] = 0;
  t->tlocks[4] = 0;
  t->tlocks[5] = 0;
  t->tlocks[6] = 0;
  t->tlocks[7] = 0;

  if ((t->kstack = (uint64)kalloc()) == 0)
  {
    freeThread(t);
    release(&t->lock);
    return 0;
  }
  // t->kstack = (uint64)kalloc();
  // t->trapframe = (struct trapframe *)(p->start + (t->myNum * TPGSIZE));
  // printf("DEBUG ---- t->myNum: %d\n", t->myNum);
  // printf("DEBUG ---- (t-p->threads): %d\n", (t - p->threads));
  // printf("DEBUG ---- sizeof(struct trapframe): %d\n", sizeof(struct trapframe));

  // printf("DEBUG >>>> assign tf in allocthread \t in proc %d \n", p->pid);
  // t->trapframe = (struct trapframe *)(p->start + ((t - p->threads) * sizeof(struct trapframe)));
  t->trapframe = (struct trapframe *)p->start + t->myNum;
  // t->trapframe = (struct trapframe *) ((uint64)(p->start) + (uint64)((t->myNum)*sizeof(struct trapframe)));
  // printf("DEBUG ******* %p \t in proc %d\n", p->start, p->pid);
  // t->trapframe->sp = t->kstack + PGSIZE;
  // Set up new context to start executing at forkret,
  // which returns to user space.
  if ((t->userTrapBackup = (struct trapframe *)kalloc()) == 0)
  {
    freeThread(t);
    release(&t->lock);
    return 0;
  }

  memset(&t->context, 0, sizeof(t->context));
  t->context.ra = (uint64)forkret;
  // printf("DEBUG ---- Passed forkret\n");
  //TODO: WTF?
  t->context.sp = t->kstack + PGSIZE;

  return t;

  //take a place in proc->currThreads[i] , if all is taken return as failure
  //return 0 in case of failure
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  // struct thread *t; // Ass2 - Task3
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == UNUSED)
    {
      goto found;
    }
    else
    {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->plocks[0] = 1;
  p->pid = allocpid();
  p->state = USED;
  p->plocks[0] = 0;
  p->plocks[1] = 0;
  p->plocks[2] = 0;
  p->plocks[3] = 0;
  p->plocks[4] = 0;
  p->plocks[5] = 0;
  p->plocks[6] = 0;
  p->plocks[7] = 0;

  // Allocate a trapframe page.
  if ((p->start = kalloc()) == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  // alloc thread iterating on all threads starting from 0, we assume the 0 one will get back.
  // t = p->threads[0];
  // for (int i = 0; i < NTHREAD; i++)
  // for (t=*(p->threads); t < p->threads[NTHREAD]; t++)
  // {
  //   acquire(&t->lock);
  //   // TODO: allocate memory for thread?
  //   t->state = UNUSED;
  // }
  ////// TODO: maybe we need to allocate all the threads in the proc???
  // allocThread(p);

  // Allocate a trapframe backup page.
  //TODO: Do we need to move it to thread?
  // if ((p->userTrapBackup = (struct trapframe *)kalloc()) == 0)
  // {
  //   freeproc(p);
  //   release(&p->lock);
  //   return 0;
  // }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // moved to allocthread
  // memset(&t->context, 0, sizeof(t->context));
  // t->context.ra = (uint64)forkret;
  // t->context.sp = t->kstack + PGSIZE;
  //Ass2 Task2

  for (int i = 0; i < 32; i++)
  {
    p->sigHandlers[i] = (void *)SIG_DFL;
  }

  //should we initialize with sigcont on?
  p->pendingSig = 1 << SIGCONT;
  //Question: where the release is happenning?
  return p;
}

static void
freeThread(struct thread *t)
{
  if (t->trapframe)
    kfree((void *)t->trapframe);
  // if (t->userTrapBackup)
  //   kfree((void *)t->userTrapBackup);
  //Is it the right way to free kstack?
  if (t->myNum != 0)
    kfree((void *)t->kstack);
  t->kstack = 0;
  t->trapframe = 0;
  t->chan = 0;
  t->state = 0;
  t->tid = 0;
  t->state = UNUSED;
  t->myNum = 0;
  t->killed = 0;
  t->parent = 0;
  t->xstate = 0;
  t->bsem_id = 0;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  struct thread *t;
  // Free all threads
  for (int i = 0; i < NTHREAD; i++)
    for (t = p->threads; t < &p->threads[NTHREAD]; t++)
    {
      freeThread(t);
    }
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->tparent = 0;
  p->name[0] = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process,
// with no user memory, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE,
               (uint64)trampoline, PTE_R | PTE_X) < 0)
  {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE,
               (uint64)(p->start), PTE_R | PTE_W) < 0)
  //  (uint64)(p->trapframe), PTE_R | PTE_W) < 0)
  {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
    0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
    0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// Set up first user process.
void userinit(void)
{
  struct proc *p;
  p = allocproc();
  initproc = p;
  struct thread *t = allocThread(p);

  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  t->trapframe->epc = 0;     // user program counter
  t->trapframe->sp = PGSIZE; // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  t->state = RUNNABLE;
  p->state = RUNNABLE;

  release(&t->lock);
  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0)
  {
    if ((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0)
    {
      return -1;
    }
  }
  else if (n < 0)
  {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  struct thread *nt;             //Ass2 - Task3
  struct thread *t = myThread(); //Ass2 - Task3

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }
  np->tparent = t;
  //FIXME: the right way to assign thread* to thread ?
  // np->threads[0] = *allocThread(np);
  // Allocate thread.
  if ((nt = allocThread(np)) == 0)
  {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;
  // copy saved user registers.
  // *(np->threads[0].trapframe) = *(t->trapframe); //Ass2 - Task3
  *(nt->trapframe) = *(t->trapframe);
  //Ass2 - Task2
  //Inherit signal mask and signal handlers
  np->sigMask = p->sigMask;
  *(np->sigHandlers) = *(p->sigHandlers);

  // Cause fork to return 0 in the child.
  // np->threads[0].trapframe->a0 = 0; //Ass2 - Task3
  nt->trapframe->a0 = 0; //Ass2 - Task3

  //Ass 2 - Task2.4
  np->handlingSignal = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;
  release(&nt->lock);
  release(&np->lock);

  // printf("DEBUG -----fork - acquire wait_lock \n");
  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  // printf("DEBUG ----- fork - acquire proc %d lock \n", np->pid);
  acquire(&np->lock);
  np->state = RUNNABLE;
  // np->threads[0].state = RUNNABLE; //Ass2 - Task3
  nt->state = RUNNABLE; //Ass2 - Task3

  release(&np->lock);
  // printf("DEBUG ---- thread %d of proc %s was forked\n", nt->tid, nt->parent->name);
  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++)
  {
    if (pp->parent == p)
    {
      pp->parent = initproc;
      wakeup(initproc->threads);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{

  struct proc *p = myproc();
  struct thread *t = myThread();
  // struct thread *wt;

  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd])
    {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  //TODO: Should we wake up all the threads or only the first?
  // for (wt = p->parent->threads; wt < &p->parent->threads[NTHREAD]; wt++)
  // {
  //   // printf("DEBUG ---- Trying to wake up proc on chan %p %d\n", (void *)wt, p->parent->pid);
  //   wakeup(wt);
  // }

  wakeup(p->tparent);
  // wakeup(p->parent);
  printf("DEBUG ---- exiting1 \n");
  acquire(&p->lock);
  p->plocks[1] = 1;
  printf("DEBUG ---- exiting2 \n");

  p->xstate = status;
  p->state = ZOMBIE;
  // t->state = ZOMBIE;

  // for (int i = 0; i < NTHREAD; i++)
  struct thread *nt;
  for (nt = p->threads; nt < &p->threads[NTHREAD]; nt++)
  {
    //FIXME: how do we kill all threads
    // p->currThreads[i].killed = 1;
    acquire(&t->lock);
    t->tlocks[1] = 1;
    t->state = ZOMBIE;
    t->tlocks[1] = 0;
    release(&t->lock);
  }
  //TODO: Do we need this?
  acquire(&t->lock);
  t->tlocks[2] = 1;
  release(&wait_lock);
  // Jump into the scheduler, never to return.
  // printf("DEBUG ---- thread  %d finish to exit\n", myThread()->tid);
  printf("DEBUG ---- exiting0 \n");

  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();
  struct thread *t = myThread();

  acquire(&wait_lock);

  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (np = proc; np < &proc[NPROC]; np++)
    {
      if (np->parent == p)
      {
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);
        havekids = 1;
        if (np->state == ZOMBIE)
        {
          // Found one.
          pid = np->pid;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                   sizeof(np->xstate)) < 0)
          {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || p->killed)
    {
      release(&wait_lock);
      return -1;
    }
    // Wait for a child to exit.
    // printf("DEBUG: %s goes to sleep", p->name);
    sleep(t, &wait_lock); //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void)
{
  // printf("DEBUG -------- CPU %d Got to scheduler --------\n", cpuid());
  struct proc *p;
  struct thread *t;
  struct cpu *c = mycpu();
  c->proc = 0;
  c->currThread = 0;
  // uint64 it = 0;
  for (;;)
  {
    // printf("DEBUG ---- scheduler iteration\n");
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      p->plocks[2] = 1;
      // printf("DEBUG ---- CPU %d acquire proc %d\n", cpuid(), p->pid);

      if (p->state == RUNNABLE || p->state == RUNNING)
      {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;

        for (t = p->threads; t < &p->threads[NTHREAD]; t++)
        {
          // printf("DEBUG ---- CPU %d acquire thread num %d of proc %d\n", cpuid(), t->myNum, p->pid);
          // if (t->tlocks[0] == 1 || t->tlocks[1] == 1 || t->tlocks[2] == 1 || t->tlocks[3] == 1 || t->tlocks[4] == 1 || t->tlocks[5] == 1 || t->tlocks[6] == 1 || t->tlocks[7] == 1)
          // {
          // printf("scheduler: %d %d %d %d %d %d %d %d\n",t->tlocks[0],t->tlocks[1],t->tlocks[2],t->tlocks[3],t->tlocks[4],t->tlocks[5],t->tlocks[6],t->tlocks[7]);
          //   break;
          // }

          if (t->state == RUNNABLE)
          {
            // printf("\nDEBUG ----  %d:\tfound thread %d to run\n",cpuid(), t->tid);
            acquire(&t->lock);
            t->tlocks[3] = 1;
            t->state = RUNNING;
            c->currThread = t;
            swtch(&c->context, &t->context);
            c->currThread = 0;
            t->tlocks[3] = 0;
            release(&t->lock);
          }
          // printf("DEBUG ---- swtch done\n");
          // printf("DEBUG ---- %d:\trelease thread %d of proc %d\n", cpuid(), t->tid, p->pid);
          // break;
        }
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      // printf("DEBUG ---- CPU %d release proc %d\n", cpuid(), p->pid);
      p->plocks[2] = 0;
      release(&p->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();
  struct thread *t = myThread();
  if (!holding(&p->lock))
    panic("sched p->lock");
  if (!holding(&t->lock))
    panic("sched t->lock");
  if (mycpu()->noff != 2)
  {
    panic("sched locks");
  }
  if (t->state == RUNNING)
    panic("sched running");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&t->context, &mycpu()->context);
  mycpu()->intena = intena;
}
// //Proc yield
// // Give up the CPU for one scheduling round.
// void yield(void)
// {
//   struct proc *p = myproc();
//   acquire(&p->lock);
//   p->state = RUNNABLE;
//   sched();
//   release(&p->lock);
// }

//Thread yield
// Give up the CPU for one scheduling round.
void yield(void)
{
  struct proc *p = myproc();
  struct thread *t = myThread();
  printf("DEBUG ---- yielding1 \n");
  acquire(&p->lock);
  p->plocks[3] = 1;
  printf("DEBUG ---- yielding2\n");
  p->state = RUNNABLE;
  // print_locks_position(p->plocks, t->tlocks);
  acquire(&t->lock);
  t->tlocks[4] = 1;

  printf("DEBUG ---- yielding3\n");
  t->state = RUNNABLE;

  // printf("DEBUG ***** yield sched,\t proc %d \t thread %d\n", p->pid, t->tid);
  sched();
  release(&t->lock);
  t->tlocks[4] = 0;
  p->plocks[3] = 0;
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
  // printf("DEBUG ---- Got to forkret\n");

  static int first = 1;

  // Still holding p->lock from scheduler.
  // Still holding t->lock from scheduler.

  release(&myThread()->lock);
  release(&myproc()->lock);
  if (first)
  {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  //changed all function from proc (p) to thread (t).
  struct thread *t = myThread();

  // printf("DEBUG ---- proc %s %d sleeping on chan %p\n", p->name, p->pid, chan);
  // printf("DEBUG ---- thread %d of %s sleeping on chan %p\n", t->tid, t->parent->name, chan);
  // printf("\nDEBUG ---- WTF channel???\n\tthread*? tid: %d\n\tproc*? pid: %d name: %s\n\n", ((struct thread *)chan)->tid, ((struct proc *)chan)->pid, ((struct proc *)chan)->name);
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.
  acquire(&p->lock); //DOC: sleeplock1
  p->plocks[4] = 1;
  acquire(&t->lock); //DOC: sleeplock1
  t->tlocks[5] = 1;

  release(lk);

  // Go to sleep.
  t->chan = chan;
  t->state = SLEEPING;
  // printf("DEBUG ***** sleep sched,\t proc %d \t thread %d\n", p->pid, t->tid);

  sched();

  // Tidy up.
  t->chan = 0;

  // Reacquire original lock.
  t->tlocks[5] = 0;

  release(&t->lock);

  p->plocks[4] = 0;
  release(&p->lock);
  acquire(lk);
}

void wakeup(void *chan)
{
  struct proc *p;
  struct thread *t;
  // printf("DEBUG ---- Trying to wake up chan %p\n", chan);
  for (p = proc; p < &proc[NPROC]; p++)
  {
    for (t = p->threads; t < &p->threads[NTHREAD]; t++)
    {
      //TODO: Check if right
      // if (t != myThread())
      // {
      //   acquire(&t->lock);
      if (t->state == SLEEPING && t->chan == chan)
      {
        t->state = RUNNABLE;
        // printf("DEBUG ---- thread %d woke up\n", t->tid);
      }
      // release(&t->lock);
      // }
    }
    // release(&p->lock);
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid, int signum)
{
  struct proc *p;
  struct thread *t;

  if (signum < 0)
  {
    return -1;
  }

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    p->plocks[5] = 1;
    //check if signal already on
    //Do we really need to check if ZOMBIE?
    if (((p->pendingSig & 1 << signum) == 0) && (p->pid == pid) && (p->state != ZOMBIE))
    {
      switch (signum)
      {
      case SIGKILL:
        p->killed = 1;
        if (p->state == SLEEPING)
        {
          // Wake process from sleep().
          p->state = RUNNABLE;
          //FIXME: is it ok?
          for (t = p->threads; t < &p->threads[NTHREAD]; t++)
          {
            acquire(&t->lock);
            t->tlocks[6] = 1;
            t->killed = 1;
            t->state = RUNNABLE;
            t->tlocks[6] = 0;
            release(&t->lock);
          }
        }
        break;
        //Ass2 - Task2
      case SIGSTOP:
        // add sigstop to pending and remove sigcont
        //FIXME :in case signal is down, will the subtract cause errors?
        p->pendingSig = ((p->pendingSig + (1 << SIGSTOP)) - (1 << SIGCONT));
        break;
      case SIGCONT:
        p->pendingSig = ((p->pendingSig + (1 << SIGCONT)) - (1 << SIGSTOP));
        break;
      default:
        p->pendingSig = p->pendingSig + (1 << signum);
        break;
      }
      release(&p->lock);
      return 0;
    }
    p->plocks[5] = 0;
    release(&p->lock);
  }
  return -1;
}

//Ass2 - Task2
uint sigprocmask(uint sigmask)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->plocks[6] = 1;
  uint oldMask = p->sigMask;
  p->sigMask = sigmask;
  release(&p->lock);
  return oldMask;
}

//Ass2 - Task2
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
  if (signum == SIGKILL || signum == SIGSTOP || act->sigmask < 0 || signum < 0)
  {
    return -1;
  }
  //TODO: Need to check the end of task 2.1.4 and acccept only valid values
  struct proc *p = myproc();
  struct sigaction *temp = p->sigHandlers[signum];
  if (act != 0)
  {
    acquire(&p->lock);
    p->plocks[7] = 1;
    p->sigHandlers[signum] = (struct sigaction *)act; //without casting makes an error becouse it's const
    release(&p->lock);
  }
  if (oldact != 0)
  {
    return copyout(p->pagetable, (uint64)oldact, (char *)&temp, sizeof(struct sigaction *));
  }
  return 0;
}

//Ass2 - Task2.1.5
void sigret()
{
  //TODO: Implement in task 2.4
  struct proc *p = myproc();
  struct thread *t = myThread();

  // printf("DEBUG --------------sigret acquire \n");
  acquire(&t->lock); //TODO: Check if we need to lock.
  //TODO: maybe mmove or mmcpy?
  *(t->trapframe) = *(t->userTrapBackup);
  p->sigMask = p->sigMaskBackup;
  p->handlingSignal = 0;
  release(&p->lock);
}

//Ass2 - Task 3.2
//TODO: add explanations
int kthread_create(void (*start_func)(), void *stack)
{
  //TODO: Implement
  struct proc *p = myproc();
  struct thread *currThread = myThread();
  struct thread *newThread = allocThread(p);
  // printf("DEBUG --- sooo the creator is %d and the createee is %d \n", currThread->tid, newThread->tid);
  if (newThread == 0)
    return -1;

  // acquire(&newThread->lock);
  // newThread->kstack = (uint64)kalloc(); //TODO: Do we need this here? https://moodle2.bgu.ac.il/moodle/mod/forum/discuss.php?d=495788
  // memmove(newThread->trapframe, currThread->trapframe, sizeof(struct trapframe));
  *(newThread->trapframe) = *(currThread->trapframe);
  newThread->state = RUNNABLE;
  newThread->trapframe->epc = (uint64)start_func;
  //allocate a user stack with size MAX_STACK_SIZE
  newThread->trapframe->sp = (uint64)stack + MAX_STACK_SIZE - 16; // should be minus??

  // printf("DEBUG ----- create - release proc %d lock \n", p->pid);
  release(&newThread->lock);
  return newThread->tid;
}

int kthread_id()
{
  //FIXME: what can cause an error?
  struct thread *t = myThread();
  return t->tid;
}

void kthread_exit(int status)
{
  //TODO: Implement
  struct proc *p = myproc();
  struct thread *t = myThread();

  //last thread of the first proc
  // if (p == initproc && p->tCounter == 1)

  if (p == initproc)
    panic("init exiting");

  // last thread of the proc
  // if (p->tCounter == 1)
  // {
  // exit(status);
  // return;
  // }
  // print_locks_position(p->plocks, t->tlocks);
  acquire(&t->lock);
  t->xstate = status;
  //t->killed = 1;
  t->state = ZOMBIE;

  sched();
  panic("not-last thread exit");
  //TODO: I'm not sure this is enough.
  //Why do we need the status param?
  return;
}

struct thread *
getThread(struct proc *p, int target_id)
{
  struct thread *t;
  for (t = p->threads; t < &p->threads[NTHREAD]; t++)
  {
    if (t->tid == target_id)
      return t;
  }
  return 0;
}

int kthread_join(int thread_id, int *status)

{
  // struct thread *t = myThread();
  struct proc *p = myproc();
  struct thread *targett = getThread(p, thread_id);

  //thread id not exist
  if (targett == 0)
    return -1;

  acquire(&wait_lock);
  for (;;)
  {
    if (targett->state == ZOMBIE)
    {
      //FIXME : should we move the value to user through another way? (like copyout)
      release(&wait_lock);
      *status = targett->xstate;
      return 0;
    }
    // printf("DEBUG ----- join -  sleeping on wait lock");
    sleep(targett, &wait_lock); // is legal for threads too ?
  }

  release(&wait_lock);
  return -1; //what can cause an error?
}
//Ass2 - Task4
int bsem_alloc(void)
{
  struct bsem *bs;
  for (bs = bsem; bs < &bsem[MAX_BSEM]; bs++)
  {
    if (bs->state == DEALLOC)
      goto found;
  }
  return -1;

found:
  bs->sid = allocsid();
  bs->state = DEALLOC;
  bs->lock = UNLOCKED;
  return bs->sid;
};

void bsem_free(int descriptor)
{
  //TODO: implement
  struct bsem *bs;
  for (bs = bsem; bs < &bsem[MAX_BSEM]; bs++)
  {
    if (bs->sid == descriptor)
      goto found;
  }
  return;
found:
  if (bs->state == ALLOC && bs->lock == UNLOCKED)
  {
    bs->sid = 0;
    bs->state = DEALLOC; //TODO: Need to make sure that there are no threads blocked because of it
    bs->lock = UNLOCKED;
  }
  return;
}

void bsem_down(int descriptor)
{

  struct bsem *bs;
  struct thread *t = myThread();
  for (bs = bsem; bs < &bsem[MAX_BSEM]; bs++)
  {
    if (bs->sid == descriptor)
      goto found;
  }
  return;
found:
  //Check if bsem allocated
  if (bs->state == ALLOC)
  {
    if (bs->lock == LOCKED)
    {
      t->bsem_id = bs->sid;
    }
  }

  //Need to block the current thread until it is unlocked
  return;
}

void bsem_up(int descriptor)
{
  struct bsem *bs;
  for (bs = bsem; bs < &bsem[MAX_BSEM]; bs++)
  {
    if (bs->sid == descriptor)
      goto found;
  }
  return;
found:
  //TODO: implement
  return;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst)
  {
    return copyout(p->pagetable, dst, src, len);
  }
  else
  {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src)
  {
    return copyin(p->pagetable, dst, src, len);
  }
  else
  {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
