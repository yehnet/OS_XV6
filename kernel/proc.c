#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sigaction.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
int nexttid = 100;

struct spinlock pid_lock;
struct spinlock tid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);
static void freeThread(struct thread *t);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

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
  for (p = proc; p < &proc[NPROC]; p++)
  {
    initlock(&p->lock, "proc");

    p->kstack = KSTACK((int)(p - proc));
    //Ass2 - Task3
    for (t = p->threads; t < &p->threads[NTHREAD]; t++)
    {
      initlock(&t->lock, "thread");
    }
    //     // p->threads[i]->kstack = KSTACK((int)(p->threads[i] - p->threads[0]));
    //     t->kstack = KSTACK((int)(t - p->threads));
    //   }
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

  if ((t->kstack = (uint64)kalloc()) == 0)
  {
    freeThread(t);
    release(&t->lock);
    return 0;
  }
  // t->kstack = (uint64)kalloc();
  // t->trapframe = (struct trapframe *)(p->start + (t->myNum * TPGSIZE));
  printf("DEBUG ---- t->myNum: %d\n", t->myNum);
  printf("DEBUG ---- (t-p->threads): %d\n", (t - p->threads));
  printf("DEBUG ---- sizeof(struct trapframe): %d\n", sizeof(struct trapframe));

  t->trapframe = (struct trapframe *)(p->start + ((t - p->threads) * sizeof(struct trapframe)));

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

  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if ((p->start = (struct trapframe*)kalloc()) == 0)
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
}
// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  struct thread *t;
  //Free all threads
  // for (int i = 0; i < NTHREAD; i++)
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
  struct thread *t = myThread(); //Ass2 - Task3

  // Allocate process.
  if ((np = allocproc()) == 0)
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
  *(np->threads[0].trapframe) = *(t->trapframe); //Ass2 - Task3

  //Ass2 - Task2
  //Inherit signal mask and signal handlers
  np->sigMask = p->sigMask;
  *(np->sigHandlers) = *(p->sigHandlers);

  // Cause fork to return 0 in the child.
  np->threads[0].trapframe->a0 = 0; //Ass2 - Task3
  //Ass 2 - Task2.4
  np->handlingSignal = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  np->threads[0].state = RUNNABLE; //Ass2 - Task3
  release(&np->lock);

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
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{
  struct proc *p = myproc();
  struct thread *t;
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
  wakeup(p->parent);

  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  // for (int i = 0; i < NTHREAD; i++)
  for (t = p->threads; t < &p->threads[NTHREAD]; t++)
  {
    //FIXME: how do we kill all threads
    // p->currThreads[i].killed = 1;
    acquire(&t->lock);
    t->state = ZOMBIE;
    release(&t->lock);
  }
  //TODO: Do we need this?
  acquire(&myThread()->lock);
  release(&wait_lock);

  // Jump into the scheduler, never to return.
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
  for (;;)
  {
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      // printf("DEBUG ---- CPU %d acquire proc %d\n", cpuid(), p->pid);

      if (p->state == RUNNABLE)
      {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.

        p->state = RUNNING;
        c->proc = p;

        for (t = p->threads; t < &p->threads[NTHREAD]; t++)
        {
          acquire(&t->lock);
          // printf("DEBUG ---- CPU %d acquire thread num %d of proc %d\n", cpuid(), t->myNum, p->pid);

          if (t->state == RUNNABLE)
          {
            t->state = RUNNING;
            c->currThread = t;
            swtch(&c->context, &t->context);
            c->currThread = 0;
          }
          // printf("DEBUG ---- swtch done\n");
          // printf("DEBUG ---- CPU %d release thread num %d of proc %d\n", cpuid(), t->myNum, p->pid);
          release(&t->lock);
        }
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      // printf("DEBUG ---- CPU %d release proc %d\n", cpuid(), p->pid);
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
  struct thread *t = myThread();
  acquire(&t->lock);
  t->state = RUNNABLE;
  acquire(&myproc()->lock);
  sched();
  release(&t->lock);
}
// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
  printf("DEBUG ---- Got to forkret\n");

  static int first = 1;

  // Still holding p->lock from scheduler.
  // Still holding t->lock from scheduler.

  release(&myproc()->lock);
  release(&myThread()->lock);

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

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock); //DOC: sleeplock1
  acquire(&t->lock); //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  t->chan = chan;
  t->state = SLEEPING;

  sched();

  // Tidy up.
  t->chan = 0;

  // Reacquire original lock.
  release(&t->lock);
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
// void wakeup(void *chan)
// {
//   struct proc *p;

//   for (p = proc; p < &proc[NPROC]; p++)
//   {
//     if (p != myproc())
//     {
//       acquire(&p->lock);
//       if (p->state == SLEEPING && p->chan == chan)
//       {
//         p->state = RUNNABLE;
//       }
//       release(&p->lock);
//     }
//   }
// }

void wakeup(void *chan)
{
  struct proc *p;
  struct thread *t;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    // or (int i = 0; i < NTHREAD; i++)
    for (t = p->threads; t < &p->threads[NTHREAD]; t++)
    {
      //TODO: Cehck if right
      if (t != myThread())
      {
        acquire(&t->lock);
        if (t->state == SLEEPING && t->chan == chan)
        {
          t->state = RUNNABLE;
          p->state = RUNNABLE;
        }
        release(&t->lock);
      }
    }
    release(&p->lock);
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid, int signum)
{
  struct proc *p;

  if (signum < 0)
  {
    return -1;
  }

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
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
    release(&p->lock);
  }
  return -1;
}

//Ass2 - Task2
uint sigprocmask(uint sigmask)
{
  struct proc *p = myproc();
  acquire(&p->lock);
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
    p->sigHandlers[signum] = (struct sigaction *)act; //without casting makes an error becouse it's const
    release(&p->lock);
  }
  if (oldact != 0)
  {
    return copyout(p->pagetable, (uint64)oldact, (char *)temp, sizeof(struct sigaction));
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
int kthread_create(void (*start_func)(), void *stack)
{
  //TODO: Implement
  struct proc *p = myproc();
  struct thread *currThread = myThread();
  struct thread *newThread = allocThread(p);

  if (newThread == 0)
    return -1;

  acquire(&newThread->lock);
  // newThread->kstack = (uint64)kalloc(); //TODO: Do we need this here? https://moodle2.bgu.ac.il/moodle/mod/forum/discuss.php?d=495788
  memmove(newThread->trapframe, currThread->trapframe, sizeof(struct trapframe));
  //or *(newThread->trapframe) = *(currThread->trapframe);
  newThread->state = RUNNABLE;
  newThread->trapframe->epc = (uint64)&start_func;
  //allocate a user stack with size MAX_STACK_SIZE
  newThread->trapframe->sp = (uint64)stack + MAX_STACK_SIZE - 16; // should be minus??
  newThread->tid = alloctid();

  release(&newThread->lock);

  if (newThread->tid == -1)
    return -1;

  return newThread->tid;
}

int kthread_id()
{
  struct thread *t = myThread();
  return t->tid;
}

void kthread_exit(int status)
{
  //TODO: Implement
  struct thread *t = myThread();

  //last thread of the first proc
  // if(t == initThread)
  //   panic("init exiting");

  // last thread of the proc
  // if (isLastThread(myProc()))
  //   exit(status);

  // acquire(&t->lock);
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
  // for (int i = 0; i < NTHREAD; i++)
  for (t = p->threads; t < &p->threads[NTHREAD]; t++)
  {
    if (t->tid == target_id)
      return t;
  }
  return 0;
}

int kthread_join(int thread_id, int *status)
{
  //TODO: Implement
  // struct thread *t = myThread();
  struct proc *p = myproc();
  struct thread *targett = getThread(p, thread_id);

  //thread id not exist
  if (targett == 0)
    return -1;

  //if the target thread already terminated , no point on waiting
  if (targett->state == ZOMBIE)
  {
    *status = targett->xstate;
    return 0;
  }
  else
  {
    acquire(&wait_lock);
    for (;;)
    {
      if (targett->state == ZOMBIE)
      {
        release(&wait_lock);
        *status = targett->xstate;
        return 0;
      }

      sleep(targett, &wait_lock); // is legal for threads too ?
    }
  }
  return -1; //what can cause error?
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
