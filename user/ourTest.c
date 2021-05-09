#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

#include "kernel/spinlock.h" // NEW INCLUDE FOR ASS2
// #include "Csemaphore.h"   // NEW INCLUDE FOR ASS 2
#include "kernel/proc.h" // NEW INCLUDE FOR ASS 2, has all the signal definitions and sigaction definition.  Alternatively, copy the relevant things into user.h and include only it, and then no need to include spinlock.h .

void runner();

int main(int argc, char *argv[])
{
    fprintf(2, "---------- TEST ----------\n");

    int pid = fork();
    int status = 0;
    void *userStack = malloc(4000);

    if (pid == 0)
    {
        printf("proc sun running\n");
        kthread_create(runner, userStack);
        for (int i = 0; i < 5; i++)
            printf("%d: I need YOU BABY\n ", kthread_id);
    }
    else
    {
        printf("proc parent running\n");

        wait(&status);
    }

    exit(0);
}

void runner()
{
    for (int i = 0; i < 5; i++)
        printf("%d: I LOVE YOU BABY\n", kthread_id());
}
