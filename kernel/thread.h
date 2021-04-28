struct thread
{
    struct spinlock lock;

    enum procstate state;        // Thread state, maybe need an enum change?????
    void *chan;                  // If non-zero, sleeping on chan
    int killed;                  // If non-zero, have been killed
    int tid;                     // Thread ID
    struct trapframe *trapframe; // data page for trampoline.S
    uint64 kstack;               // Virtual address of kernel stack

    // proc_tree_lock must be held when using this:
    struct proc *parent;    // Parent process - ShOuLD WE NEED THIS?
    struct context context; // swtch() here to run process
};
