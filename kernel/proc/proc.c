#include "types.h"
#include "param.h"
#include "proc/proc.h"
#include "riscv.h"
#include "memlayout.h"
#include "proc/initcode.h"
#include "defs.h"

/*** things about CPU ***/
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

/*** things about process ***/

// in trampoline.S
extern char trampoline[];

// in swtch.S
extern void swtch(context_t *old, context_t *new);

// in trap_user.c
extern void trap_user_return();

// the process table
proc_t proc[NPROC];

// the first process
proc_t *proczero;

// about pid
int nextpid = 1;
spinlock_t pid_lock;

// TODO: I have no idea what does this lock do. 
// must be acquired before any p->lock
spinlock_t wait_lock;

int alloc_pid() {
    int pid;

    acquire(&pid_lock);
    pid = nextpid;
    nextpid = nextpid + 1;
    release(&pid_lock);

    return pid;
}

// Allocate a page for each proc's kernel stack.
// Map it high in memory, followed by an invalid guard page.
// Used for initializing kernel memory.
void proc_mapstacks(pagetbl_t kpgtbl) {
    proc_t *p = proczero;
    char *pa = pmem_alloc(0);
    if (pa == 0) {
        panic("pmem_alloc");
    }
    uint64 va = KSTACK(0);
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
}

// initialize the proc table.
void proc_init(void) {
    proc_t *p;

    initlock(&pid_lock, "nextpid")
    initlock(&wait_lock, "wait_lock");
    for (p = proc; p < &proc[NPROC]; p++) {
        initlock(&p->lock, "proc");
        p->state = UNUSED;
        p->kstack = KSTACK((int) (p - proc));
    }
}

static pagetbl_t proc_pgtbl_init(uint64 trapframe) {
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

// find an unusued proc
// if found, initialize and return with p->lock held
// if there's any error, return 0
static proc_t* alloc_proc(void) {
    proc_t *p;

    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state == UNUSED) {
            goto found;
        } else {
            release(&p->lock);
        }
    }
    return 0;

found:
    p->pid = alloc_pid();
    p->state = USED;

    // trapframe
    if ((p->trapframe = (trapframe_t *)pmem_alloc(1)) == 0) {
        free_proc(p);
        release(&p->lock);
        return 0;
    }
    memset((void*)p->trapframe, 0, PGSIZE);

    // pagetable
    if ((p->pgtbl = proc_pgtbl_init((uint64)(p->trapframe))) == 0) {
        free_proc(P);
        return 0;
    }

    // heap_sz
    p->heap_top = 2*PGSIZE;

    // ustack_pages
    p->ustack_pages = 1;

    // kstack
    p->kstack = KSTACK(0);

    // set proc context
    memset(&p->ctx, 0, sizeof(p->ctx));
    p->ctx.ra = (uint64)forkret;
    p->ctx.sp = p->kstack + PGSIZE;  // sp要指向栈顶



    return p;
}

// free a proc, p->lock must be held.
static void free_proc(proc_t *p) {
    if (p->trapframe)
        pmem_free((void*)p->trapframe);
    p->trapframe = 0;
    if (p->pgtbl)
        proc_free_pagetable(p->pgtbl, p->heap_top);
    p->pgtbl = 0;
    p->heap_top = 0;
    p->pid = 0;
    p->parent = 0;
    p->chan = 0;
    p->killed = 0;
    p->xstate = 0;
    p->state = UNUSED;
}

// free a process's pagetable, including the physical memory.
void proc_free_pagetable(pagetbl_t pagetable, uint64 sz) {
    vm_unmappages(pagetable, TRAMPOLINE, 1, 0);
    vm_unmappages(pagetable, TRAPFRAME, 1, 0);
    vm_upage_free(pagetable, sz);
}

// This function is deprecated after introducing scheduling.
// 
// void proc_make_first() {
//     alloc_proc(proczero);
//     // switch to proczero
//     mycpu()->proc = proczero;
//     swtch(&mycpu()->ctx, proczero->ctx);
// }

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

// TODO:
// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int kfork() {

}

// TODO:
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int kwait(uint64 addr) {
    
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void) {
    extern char userret[];
    proc_t *p = myproc();
    static int first = 1;

    if (first) {
        first = 0;
        __sync_synchronize();

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
    }

    // return to user space, mimicing usertrap()'s return.
    prepare_return();
    uint64 satp = MAKE_SATP(p->pagetable);
    uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
    ((void (*)(uint64))trampoline_userret)(satp);
}
