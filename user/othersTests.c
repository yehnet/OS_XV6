#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

struct sigaction {
    void (*sa_handler) (int);
    uint sigmask;
};

void somefunc();
void handler(int);
void handler2(int);
void handler3(int);
void handler4(int);
void handler5(int);
void simple_create();
void simple_exit();
void simple_join();
void simple_sleep();
void exit_last_only_thread();
void exit_last();
void exit_all_not_last();
void exit_myself_not_last();
//void create_several_threads_of_the_same_process();  - done in this test - exit_myself_not_last
void create_more_thread_than_possible();
void create_several_threads_of_different_processes(); 
//void simple_wakeup(); - will already be done in join
//void wait_for_process_with_several_threads(); - done in this test - create_several_threads_of_different_processes
void join_invalid_tid();
void join_complicated();
void thread_creates_thread_creates_thread();
void thread_available_again_can_be_reused();


//signal tests
void signal_with_several_threads();
void sigkill_with_several_threads();
void sigaction_with_several_threads();

//deadlock tests
void join_on_join_on_join();
void thread_fork();



//void test main thread exits and other threads keep on going

void somefunc()
{
    printf("this is a func just to take place\n"); 
}

void
handler(int i){

    printf("in thread world: thread with tid %d before sleep!\n", kthread_id());
    sleep(20);
    printf("in thread world: with tid %d after sleep!\n", kthread_id());
    kthread_exit(0);
}

void
handler2(int i){

    printf("thread with tid %d before wasting time!\n", kthread_id());
    int j=0;
    while (j<1000000000){
        j++;
        printf("~"); //- bettere add this line for the signal_with_several_threads test
    }
    printf("thread with tid %d after wasting time!\n", kthread_id());
    kthread_exit(0);

}

void
handler3(int i){

    printf("in handler3: thread with tid %d \n", kthread_id());
    kthread_exit(0);
}

void
handler4(int i){
    int status2;
    printf("thread with tid %d creates new thread\n", kthread_id());
    void *stack1 = malloc(4000);
    int tid2 = kthread_create(handler2, stack1);
    kthread_join(tid2, &status2);
    kthread_exit(0);
}

void
handler5(int i){
    int status1;
    int ttid = kthread_id();
    int cpid = fork();
    if (cpid){
        printf("thread with tid: %d waiting for child!\n", ttid);
        wait(&status1);
        printf("done waiting!!!!!\n", ttid);
        //kthread_exit(0); //with this line - after join should be printed
        exit(0); //with this line - after join should not be printed
    }
    else{
        printf("i am the child of thread with tid: %d\n", ttid);
        exit(0);
    }
}

void
handler6(int i){

    printf("we should print this once :) \n");

}

void simple_create(){
    //fprintf(2,"%d\n",&somefunc);
    //printf("in test: handler adddress is:%p\n", &handler);
    void *stack = malloc(4000);
    printf("before kthread_create\n");
    kthread_create(handler, stack);
    printf("before sleep\n");
    sleep(10);
    printf("after sleep\n");
}

void simple_create_long_acting(){
    fprintf(2,"%d\n",&somefunc);
    //printf("in test: handler2 adddress is:%p\n", &handler2);
    void *stack = malloc(4000);
    //printf("before kthread_create\n");
    kthread_create(handler2, stack);
   // printf("tid returned from kthread_create is: %d\n", tid);
}

 void simple_join(){
    fprintf(2,"%d\n",&somefunc);
    int status;
    printf("in test: handler adddress is:%p\n", &handler);
    void *stack = malloc(4000);
    printf("before kthread_create\n");
    int tid = kthread_create(handler, stack);
    printf("before join\n");
    kthread_join(tid, &status);
    printf("after join\n");
}

void simple_exit(){
    printf("before sleep\n");
    sleep(10);
    printf("after sleep\n");
    kthread_exit(0);
}

void simple_sleep(){
    printf("simple_sleep test \n");
    printf("before sleep\n");
    sleep(10);
    printf("after first sleep\n");
    sleep(10);
    printf("after second sleep\n");
    kthread_exit(0);
}

void exit_last_only_thread(){
    printf("exit_last_only_thread test\n");
    //kthread_exit(0); //run with and without this line to check if exits right using exit and kthread_exit
}

void exit_last(){
    printf("exit_last test\n");
    for (int i=0; i<7; i++){ //creating another 7 processes
        simple_create();
    }
    exit(0); 
}

void exit_all_not_last(){
    printf("exit_last test\n");
    for (int i=0; i<7; i++){ //creating another 7 processes
        simple_create_long_acting();
    }
    sleep(10);
    exit(0); 
}

void exit_myself_not_last(){
    printf("exit_last test\n");
    for (int i=0; i<7; i++){ //creating another 7 processes
        simple_create_long_acting();
    }
    printf("gonna exit mysel\n");
    kthread_exit(0); 
}

void create_more_thread_than_possible(){
    printf("create_more_thread_than_possible test\n");
    for (int i=0; i<10; i++){ //creating another 7 processes
        simple_create_long_acting();
    }
    printf("gonna exit mysel\n");
    kthread_exit(0); 
}


int wait_sig = 0;

void test_handler(int signum){
    wait_sig = 1;
//    printf("Received sigtest\n");
}

void signal_test(){
    int pid;
    int testsig;
    testsig=15;
    struct sigaction act = {test_handler, (uint)(1 << 29)};
    struct sigaction old; 
    sigprocmask(0);
    sigaction(testsig, &act, &old);
    if((pid = fork()) == 0){
        while(!wait_sig)
            sleep(1);
        exit(0);
    }
    kill(pid, testsig);
    wait(&pid);
    printf("Finished testing signals\n");
}

void create_several_threads_of_different_processes(){
    int cpid = fork();
    if (cpid) { //father!!!!!
        sleep(2);
        simple_create_long_acting();
        printf("gonna wait for son now\n");
        wait(0);
    }
    else{
        simple_create_long_acting();
        simple_create_long_acting();
        sleep(70);
        kthread_exit(0);
    }
}

void join_invalid_tid(){
    printf("join_invalid_tid test\n");
    printf("%d\n",&somefunc);
    int status =5;
    //void *stack = malloc(MAX_STACK_SIZE);
    printf("before join\n");
    int returnedVal = kthread_join(16, &status);
    printf("after join, status equals: %d, and returnedVal equals: %d\n", status, returnedVal);
}

void join_complicated(){
    //first - getting invalid tid
    join_invalid_tid();
    printf("after join_invalid_tid\n");
    
    //creating several threads for the same process and waiting for a specific one
    fprintf(2,"%d\n",&somefunc);
    int status;
    void *stack1 = malloc(4000);
    void *stack2 = malloc(4000);
    void *stack3 = malloc(4000);
    printf("before 3 kthread_create\n");
    int tid1 = kthread_create(handler3, stack1);
    printf("created thread with tis %d\n", tid1);
    int tid2 = kthread_create(handler2, stack2);
    printf("created thread with tis %d\n", tid2);
    int tid3 = kthread_create(handler, stack3);
    printf("created thread with tis %d\n", tid3);

    printf("before join\n");
    kthread_join(tid2, &status);
    printf("after join\n");
}

void thread_creates_thread_creates_thread(){
    int status;
    printf("thread_creates_thread_creates_thread test\n");
    printf("thread with tid %d creates new thread\n", kthread_id());
    void *stack1 = malloc(4000);
    int tid = kthread_create(handler4, stack1);
    kthread_join(tid, &status);
}

void thread_available_again_can_be_reused(){
    for (int i=0; i<7; i++){ //creating another 7 processes
        simple_create();
    }
    printf( "~~~~~~after creating first 7 processes~~~~~");

    sleep(60);

    for (int i=0; i<7; i++){ //creating another 7 processes
        simple_create();
    }
    printf( "~~~~~~after creating another 7 processes~~~~~");
    sleep(20);
}

void thread_fork(){
    int status;
    void *stack = malloc(4000);
    printf("before kthread_create\n");
    int tid = kthread_create(handler5, stack);
    printf("before join 1\n");
    kthread_join(tid, &status);
    printf("after join\n");
}

void signal_with_several_threads(){
    int status;
    printf("signal_with_several_threads test\n");
    int cpid = fork();
    if (cpid){ //father
        sleep(1); //so the son will create several threads.
        kill(cpid, SIGSTOP);
        printf("father sent sigstop \n");
        sleep(30);
        printf("father woke up\n");
        kill(cpid, SIGCONT);
        printf("father sent sigcont and going to wait\n");
        wait(&status);
    }
    else{ //son
        for (int i=0; i<4; i++){ //creating another 7 processes
            simple_create_long_acting();
        }
        printf("son going to sleep\n");
        sleep (100);
        printf("son waking up\n");
        exit(0);
    }
}

void sigaction_with_several_threads(){
    int status;
    printf("signal_with_several_threads test\n");
    int cpid = fork();
    if (cpid){ //father
        sleep(10); //so the son will create several threads.
        kill(cpid, 2);
        printf("father sent sig 2 \n");
        sleep(30);
        printf("father woke up\n");
        wait(&status);
    }
    else{ //son
        for (int i=0; i<3; i++){ //creating another 7 processes
            simple_create_long_acting();
        }
        struct sigaction old;
        struct sigaction new;
        new.sa_handler = handler6;
        new.sigmask=0;
        sigaction(2, &new, &old);
        printf("son changed sig 2 & gonna sleep\n");
        sleep (20);
        printf("son waking up\n");
        exit(0);
    }
}


void sigkill_with_several_threads(){
    int status;
    printf("sigkill_with_several_threads test\n");
    int cpid = fork();
    if (cpid){ //father
        sleep(1); //so the son will create several threads.
        kill(cpid, SIGSTOP);
        printf("father sent sigstop \n");
        sleep(30);
        printf("father woke up\n");
        kill(cpid, SIGKILL);
        //kill(cpid, 4);
        printf("father sent sigkill and going to wait\n");
        wait(&status);
    }
    else{ //son
        for (int i=0; i<5; i++){ //creating another 7 processes
            simple_create_long_acting();
        }
        printf("son going to sleep\n");
        sleep (20);
        printf("son waking up\n");
        exit(0);
    }
}

int
main(int argc, char *argv[])
{
    printf("hello world\n");
    //signal_test();
    //simple_create();
    //thread_test("\ntest_threads\n");
    //simple_exit();
    //simple_join();
    //simple_sleep();
    //exit_last_only_thread();
    //exit_last();
    //exit_myself_not_last();
    //exit_all_not_last();
    //create_more_thread_than_possible(); //just started! need to check this!!
    //exit_all_not_last();
    //create_several_threads_of_different_processes();
    //join_invalid_tid();
    //join_complicated();
    //thread_creates_thread_creates_thread();
    //thread_creates_thread_creates_thread();
    //thread_fork();// - this test is very important. not a cllasic scenario but might find bugs.
    //thread_available_again_can_be_reused(); //better delete printings from simple_create before running 
    //signal_with_several_threads();
    //sigaction_with_several_threads();
    sigkill_with_several_threads();
    printf("passed\n");
    exit(0);
}