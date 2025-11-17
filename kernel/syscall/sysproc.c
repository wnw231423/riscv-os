#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "lib/spinlock.h"
#include "proc/proc.h"

uint64 sys_sbrk(void) {
    uint64 addr;
    int n;

    arg_int(0, &n);
    if(grow_proc(n) < 0) {
        return -1;
    }
    return addr;
}

uint64 sys_exit(void) {
    int n;
    arg_int(0, &n);
    kexit(n);
    return 0;
}
