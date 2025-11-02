#include "types.h"
#include "param.h"
#include "proc/proc.h"
#include "riscv.h"

struct cpu cpus[NCPU];

int cpuid() {
    int id = r_tp();
    return id;
}

struct cpu* mycpu() {
    int id = cpuid();
    struct cpu *c = &cpus[id];
    return c;
}

// in trampoline.S
extern char trampoline[];

// in swtch.S
extern void swtch(context_t *old, context_t *new);

// in trap_user.c
extern void trap_user_return();

static proc_t proczero;

// Allocate a page for each proc's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl) {
    proc_t *p = &proczero;
    char *pa = pmem_alloc(0);
    if (pa == 0) {
        panic("pmem_alloc")
    }
    uint64 va = KSTACK(0);
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
}


pagetbl_t proc_pgtbl_init(uint64 trapframe) {
    pagetbl_t proc_pgtbl;

    proc_pgtbl = vm_upage_create();
    if(proc_pgtbl == 0) {
        return 0;
    }

    // try to map trampoline
    if(vm_mappages(proc_pgtbl, TRAMPOLINE, PGSIZE,
                    (uint64)trampoline, PTE_R | PTE_X) < 0) {
        vm_upage_free(pagetable, 0);
        return 0;
    }

    // try to map trapframe
    if(vm_mappages(proc_pgtbl, TRAPFRAME, PGSIZE,
                    trapframe, PTE_R | PTE_W) < 0) {
        vm_unmappages(proc_pgtbl, TRAMPOLINE, 1, 0);
        vm_upage_free(proc_pgtbl, 0);
        return 0;
    }

    return proc_pgtbl;
}

void alloc_proc(proc_t *p) {
    p->pid = 0;
    if ((p->trapframe = (trapframe_t *)pmem_alloc(1)) == 0) {
        free_proc(p);
        return 0;
    }

    if ((p->pgtbl = proc_pgtbl_init((uint64)(p->trapframe))) == 0) {
        free_proc(p);
        return 0;
    }
}

void proc_make_first() {
    alloc_proc(&proczero);
}
