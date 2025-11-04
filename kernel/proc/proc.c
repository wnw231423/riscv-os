#include "types.h"
#include "param.h"
#include "proc/proc.h"
#include "riscv.h"
#include "memlayout.h"
#include "proc/initcode.h"
#include "defs.h"

cpu_t cpus[NCPU];

int cpuid() {
    int id = r_tp();
    return id;
}

cpu_t* mycpu() {
    int id = cpuid();
    cpu_t *c = &cpus[id];
    return c;
}

proc_t* myproc() {
    cpu_t *c = mycpu();
    proc_t *p = c->proc;
    return p;
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
void proc_mapstacks(pagetbl_t kpgtbl) {
    //proc_t *p = &proczero;
    char *pa = pmem_alloc(0);
    if (pa == 0) {
        panic("pmem_alloc");
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
        vm_upage_free(proc_pgtbl, 0);
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

int alloc_proc(proc_t *p) {
    // pid
    p->pid = 0;

    // trapframe
    if ((p->trapframe = (trapframe_t *)pmem_alloc(1)) == 0) {
        return 1;
    }
    memset((void*)p->trapframe, 0, PGSIZE);

    // pagetable
    if ((p->pgtbl = proc_pgtbl_init((uint64)(p->trapframe))) == 0) {
        return 1;
    }

    // heap_top
    p->heap_top = 0;

    // ustack_pages
    p->ustack_pages = 1;

    // kstack
    p->kstack = KSTACK(0);

    // set proc context
    memset(&p->ctx, 0, sizeof(p->ctx));
    p->ctx.ra = (uint64)trap_user_return;
    p->ctx.sp = p->kstack + PGSIZE;  // sp要指向栈顶

    // set inst, data and stack
    extern unsigned char kernel_proc_initcode[];

    uint64 code_page = (uint64)pmem_alloc(1);
    memset((void*)code_page, 0, PGSIZE);
    memmove((void*)code_page, kernel_proc_initcode, sizeof(kernel_proc_initcode));
    vm_mappages(p->pgtbl, PGSIZE, PGSIZE, code_page, PTE_R | PTE_X | PTE_U);
    p->trapframe->epc = PGSIZE;

    vm_mappages(p->pgtbl, 2*PGSIZE, PGSIZE, (uint64)pmem_alloc(1), PTE_W | PTE_R | PTE_U);
    p->trapframe->sp = 3*PGSIZE;

    return 0;
}

void proc_make_first() {
    alloc_proc(&proczero);
    // switch to proczero
    mycpu()->proc = &proczero;
    swtch(&mycpu()->ctx, &proczero.ctx);
}
