#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sigaction.h"

struct spinlock tickslock;
uint ticks;

//sys_sigret pointers
extern void *start_sigret;
extern void *end_sigret;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

//Ass2 - Task2.4
void handleSignals(struct proc *p);

void trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void usertrap(void)
{
  // printf("DEBUG ---- Got to usertrap\n");
  int which_dev = 0;

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  struct thread *t = myThread();

  // save user program counter.
  t->trapframe->epc = r_sepc();

  if (r_scause() == 8)
  {
    // system call

    if (p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    t->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  }
  else if ((which_dev = devintr()) != 0)
  {
    // ok
  }
  else
  {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if (p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
    yield();

  //Ass2 - Task2
  if (p->pendingSig & 1 << SIGSTOP)
    yield();

  usertrapret();
}

//
// return to user space
//
void usertrapret(void)
{
  // printf("DEBUG ---- Got to usertrapret\n");
  struct proc *p = myproc();
  // Ass2 - Task3
  struct thread *t = myThread();
  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  t->trapframe->kernel_satp = r_satp();         // kernel page table
  t->trapframe->kernel_sp = t->kstack + PGSIZE; // process's kernel stack
  t->trapframe->kernel_trap = (uint64)usertrap;
  t->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.

  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(t->trapframe->epc);
  //Ass2 - Task2.4
  handleSignals(p); //FIXME: Is it the right location?

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  // uint64 fn = TRAMPOLINE + sizeof(struct trapframe) * (t - p->threads);
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  uint64 tfOffset = sizeof(struct trapframe) * (t - p->threads);
  ((void (*)(uint64, uint64))fn)(TRAPFRAME + tfOffset, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  if ((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if ((which_dev = devintr()) == 0)
  {
    // printf("DEBUG ******* %p \t in proc %d\n",myThread()->trapframe, myproc()->pid);
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    // printf("DEBUG ----- proc pid: %d\nthread tid: %d\n",myproc()->pid, myThread()->tid);
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr()
{
  uint64 scause = r_scause();

  if ((scause & 0x8000000000000000L) &&
      (scause & 0xff) == 9)
  {
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if (irq == UART0_IRQ)
    {
      uartintr();
    }
    else if (irq == VIRTIO0_IRQ)
    {
      virtio_disk_intr();
    }
    else if (irq)
    {
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if (irq)
      plic_complete(irq);

    return 1;
  }
  else if (scause == 0x8000000000000001L)
  {
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if (cpuid() == 0)
    {
      clockintr();
    }

    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  }
  else
  {
    return 0;
  }
}
//Ass2 - Task2.4
void handleSignals(struct proc *p)
{
  struct thread *t = myThread();
  int i = 0;
  // int singal;
  uint32 pendings = p->pendingSig;
  //Check if there are pending signals that are not blocked
  // if ((pendings != 0) && (pendings & p->sigMask) == 0)
  // {
  p->handlingSignal = 1;
  //Iterate over the pending signals
  while ((pendings >> i) != 0)
  {
    //finds the pending signal i and start handler

    if ((pendings & (1 << i)) != 0)
    {
      //---------- Kernel space handlers ----------
      if (p->sigHandlers[i] == (void *)SIG_DFL)
      {
        switch (i)
        {
        case SIGSTOP:
          kill(p->pid, SIGSTOP);
          break;
        case SIGCONT:
          kill(p->pid, SIGCONT);
          break;
        default:
          kill(p->pid, SIGKILL);
          break;
        }
        //Discarding the signal
        p->pendingSig -= (1 << i);
      }
      else if (p->sigHandlers[i] == (void *)SIG_IGN)
      {
        //Discarding the signal
        p->pendingSig -= (1 << i);
      }
      //---------- User space handlers ----------
      else
      {
        //TODO: maybe mmove or mmcpy?
        //Backup trapframe
        *(t->userTrapBackup) = *(t->trapframe);
        //Bcakup signal mask
        p->sigMaskBackup = p->sigMask;
        p->sigMask = ((struct sigaction *)(p->sigHandlers[i]))->sigmask;

        //Inject sigret to user stack
        uint64 sigretSize = ((uint64)&end_sigret - (uint64)&start_sigret);
        t->trapframe->sp -= sigretSize;
        copyout(p->pagetable, (uint64)t->trapframe->sp, (char *)&start_sigret, sigretSize);

        //make the return address to be the sigret
        t->trapframe->ra = t->trapframe->sp;

        //signal number as argument for sa_handler
        t->trapframe->a0 = i;

        //The process will continue with the sa_handler
        t->trapframe->epc = (uint64)p->sigHandlers[i];

        //Discarding the signal
        p->pendingSig -= (1 << i);
      }
    }
    i++;
  }
  p->handlingSignal = 0;
  // }
  return;
}
