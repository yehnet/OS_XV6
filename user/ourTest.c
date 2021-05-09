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

void runner1()
{
    for (int i = 0; i < 5; i++)
        printf("%d: I LOVE YOU BABY\n", kthread_id());
}
void runner2()
{
    for (int i = 0; i < 5; i++)
        printf(": I LOVE YOU BABY\n", kthread_id());
    exit(0);
}
void runner3()
{
    for (int i = 0; i < 5; i++)
        printf(": I LOVE YOU BABY\n", kthread_id());
    exit(0);
}
int main(int argc, char *argv[])
{
    fprintf(2, "---------- TEST ----------\n");

    int pid = fork();
    int status = 0;
    void *userStack = malloc(4000);
    // int secPid;

    if (pid == 0)
    {
        //first function gets address 0 
        printf("function runner1 gets address >  %p\n", runner1);
        printf("create - gets runner pointer %p\n", runner2);
        printf("create - gets runner pointer %p\n", runner3);

        kthread_create(runner3, userStack);
        // kthread_join(secPid, &status);
        for (int i = 0; i < 5; i++)
        {
            printf("%d: I need YOU BABY\n ", kthread_id());
        }
    }
    else if (pid == -1)
    {
        printf("ERRPR\n");
    }
    else
    {
        // printf("proc parent running\n");
        wait(&status);
    }

    exit(0);
}
