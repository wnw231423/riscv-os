#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64 sys_sbrk(void) {
    uint64 addr;
    int t;
    int n;

    argint(0, &n);
    argint(1, &t);
    addr = myproc()->sz;

    if(t == SBRK_EAGER || n < 0) {
        if(growproc(n) < 0) {
        return -1;
    }
    } else {
        // Lazily allocate memory for this process: increase its memory
        // size but don't allocate memory. If the processes uses the
        // memory, vmfault() will allocate it.
        if(addr + n < addr)
            return -1;
        if(addr + n > TRAPFRAME)
            return -1;
        myproc()->sz += n;
    }
    return addr;
}
