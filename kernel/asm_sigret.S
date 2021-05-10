#include "syscall.h"

.globl start_sigret
.globl end_sigret


start_sigret:
        li a7, SYS_sigret
        ecall
        ret
end_sigret:
