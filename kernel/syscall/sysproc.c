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
    addr = myproc()->heap_top;

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

// a limited printf syscall, only recceive one augument
uint64 sys_print(void) {
    char *s = "";
    arg_str(0, s, 500);
    printf(s);
    return 0;
}

uint64 sys_fork(void) {
    return kfork();
}

uint64 sys_wait(void) {
    uint64 p;
    argaddr(0, &p);
    return kwait(p);
}

// mmap syscall: need 2 arguments:
//   1. the start va, should be page aligned.
//   2. how much space, should be page aligned.
// only for test.
uint64 sys_mmap(void) {
    uint64 va, size, start_va, pa;

    arg_uint64(0, &va);
    arg_uint64(1, &size);
    start_va = va;

    if((va % PGSIZE) != 0)
        return 0;

    if((size % PGSIZE) != 0)
        return 0;

    while (size > 0) {
        pa = (uint64)pmem_alloc(1);
        vm_mappages(myproc()->pgtbl, va, PGSIZE, pa, PTE_R | PTE_W | PTE_U);
        va += PGSIZE;
        size -= PGSIZE;
    }

    return start_va;
}

uint64 sys_munmap(void) {
    uint64 va, size;
    int npages;

    arg_uint64(0, &va);
    arg_uint64(1, &size);

    if((va % PGSIZE) != 0)
        return -1;

    if((size % PGSIZE) != 0)
        return -1;

    npages = size / PGSIZE;

    vm_unmappages(myproc()->pgtbl, va, npages, 1);

    return 0;
}
