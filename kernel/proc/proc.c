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

proc_t proc[NPROC];

proc_t *proczero;

// TODO: I have no idea what does this lock do. 
// must be acquired before any p->lock
spinlock_t wait_lock;

// Allocate a page for each proc's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetbl_t kpgtbl) {
    proc_t *p = proczero;
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

// TODO: 
// alloc for all proc_blocks.
// deprecate the proczero initilization work from this function.
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

    // heap_sz
    p->heap_top = 2*PGSIZE;

    // ustack_pages
    p->ustack_pages = 1;

    // kstack
    p->kstack = KSTACK(0);

    // set proc context
    memset(&p->ctx, 0, sizeof(p->ctx));
    p->ctx.ra = (uint64)trap_user_return;
    p->ctx.sp = p->kstack + PGSIZE;  // sp要指向栈顶

    // set inst and stack
    // inst
    extern unsigned char kernel_proc_initcode[];
    uint64 code_page = (uint64)pmem_alloc(1);
    memset((void*)code_page, 0, PGSIZE);
    memmove((void*)code_page, kernel_proc_initcode, sizeof(kernel_proc_initcode));
    vm_mappages(p->pgtbl, 0, PGSIZE, code_page, PTE_R | PTE_X | PTE_U);
    p->trapframe->epc = 0;
    // stack
    vm_mappages(p->pgtbl, PGSIZE, PGSIZE, (uint64)pmem_alloc(1), PTE_W | PTE_R | PTE_U);
    p->trapframe->sp = 2*PGSIZE;

    return 0;
}

void proc_make_first() {
    alloc_proc(proczero);
    // switch to proczero
    mycpu()->proc = proczero;
    swtch(&mycpu()->ctx, proczero->ctx);
}

// return 0 on success, -1 on failure
int grow_proc(int n) {
    uint64 heap_top;
    proc_t *p = myproc();

    heap_top = p->heap_top;
    if (n > 0) {
        if ((heap_top = vm_u_alloc(p->pgtbl, heap_top, heap_top + n, PTE_W)) == 0) {
            return -1;
        }
    } else if (n < 0) {
        heap_top = vm_u_dealloc(p->pgtbl, heap_top, heap_top + n);
    }
    p->heap_top = heap_top;
    return 0;
}

// pass p's abandoned children to init proc
// Caller must hold wait_lock
void reparent(proc_t *p) {
    proc_t *pp;

    for(pp = proc; pp < &proc[NPROC]; pp++) {
        if(pp->parent == p) {
            pp->parent = proczero;
            wakeup(initproc);
        }
    }
}

// Exit the current process
void kexit(int status) {
    proc_t *p = myproc();
    
    if (p == proczero)
        panic("init exiting");

    acquire(&wait_lock);

    // Given any children to init
    reparent(p);

    // Parent might be sleeping in wait()
    wakeup(p->parent);

    acquire(&p->lock);

    p->xstate = status;
    p->state = ZOMBIE;

    release(&wait_lock);
    release(&p->lock);
    // never to return
    //sched();
    //panic("zombie exit");
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int kfork() {

}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int kwait(uint64 addr) {
    
}
