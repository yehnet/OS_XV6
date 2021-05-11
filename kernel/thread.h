struct thread
{
    struct spinlock lock;
    int myNum;                     // Thread number in proc
    enum procstate state;        // Thread state, maybe need an enum change?????
    void *chan;                  // If non-zero, sleeping on chan
    int killed;                  // If non-zero, have been killed
    int xstate;                  // Exit status to be returned to parent's wait
    int tid;                     // Thread ID
    struct trapframe *trapframe; // data page for trampoline.S
    struct trapframe *userTrapBackup; //trapframe back up for signal handling
    uint64 kstack;               // Virtual address of kernel stack

   // proc_tree_lock must be held when using this:
    struct proc *parent;    // Parent process - ShOuLD WE NEED THIS?
    struct context context; // swtch() here to run process
    //Ass2 - Task4
    int bsem_id;
    int tlocks[8];          //for debugging, locks acquire and release locations
};


