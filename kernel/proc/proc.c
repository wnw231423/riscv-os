#include "types.h"
#include "param.h"
#include "defs.h"
#include "lib/spinlock.h"
#include "proc/proc.h"
#include "riscv.h"
#include "memlayout.h"
#include "proc/initcode.h"


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
    proc_t *p;
    for (p = proc; p < &proc[NPROC]; p++) {
        char *pa = pmem_alloc(0);
        if (pa == 0) {
            panic("pmem_alloc");
        }
        uint64 va = KSTACK((int) (p - proc));
        kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    }
}

// initialize the proc table.
void proc_init(void) {
    proc_t *p;

    initlock(&pid_lock, "nextpid");
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

// free a process's pagetable, including the physical memory.
void proc_free_pagetable(pagetbl_t pagetable, uint64 sz) {
    vm_unmappages(pagetable, TRAMPOLINE, 1, 0);
    vm_unmappages(pagetable, TRAPFRAME, 1, 0);
    vm_upage_free(pagetable, sz);
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

void forkret(void);
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
    //p->state = USED;
    p->state = RUNNABLE;

    // trapframe
    if ((p->trapframe = (trapframe_t *)pmem_alloc(1)) == 0) {
        free_proc(p);
        release(&p->lock);
        return 0;
    }
    memset((void*)p->trapframe, 0, PGSIZE);

    // pagetable
    if ((p->pgtbl = proc_pgtbl_init((uint64)(p->trapframe))) == 0) {
        free_proc(p);
        release(&p->lock);
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
            wakeup(proczero);
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
    
    // never to return
    proc_sched();
    panic("zombie exit");
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int kfork() {
    int pid;
    proc_t *np;
    proc_t *p = myproc();

    if ((np = alloc_proc()) == 0) {
        return -1;
    }

    if (vm_u_copy(p->pgtbl, np->pgtbl, p->heap_top) < 0) {
        free_proc(np);
        release(&np->lock);
        return -1;
    }
    np->heap_top = p->heap_top;

    // copy saved user registers
    *(np->trapframe) = *(p->trapframe);

    // fork should return 0
    np->trapframe->a0 = 0;

    pid = np->pid;

    release(&np->lock);

    acquire(&wait_lock);
    np->parent = p;
    release(&wait_lock);

    acquire(&np->lock);
    np->state = RUNNABLE;
    release(&np->lock);

    return pid;
}

int
killed(proc_t *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int kwait(uint64 addr) {
    proc_t *pp;
    int havekids, pid;
    proc_t *p = myproc();
    
    acquire(&wait_lock);

    for(;;) {
        havekids = 0;
        for (pp = proc; pp < &proc[NPROC]; pp++) {
            if (pp->parent == p) {
                acquire(&pp->lock);
                havekids = 1;
                if (pp->state == ZOMBIE) {
                    pid = pp->pid;
                    if (addr != 0 && copyout(p->pgtbl, addr, (char *)&pp->xstate,
                                             sizeof(pp->xstate)) <0) {
                        release(&pp->lock);
                        release(&wait_lock);
                        return -1;
                    }
                    free_proc(pp);
                    release(&pp->lock);
                    release(&wait_lock);
                    return pid;
                }
            release(&pp->lock);
            }
        }

        if (!havekids || killed(p)) {
            release(&wait_lock);
            return -1;
        }

        sleep(p, &wait_lock);
    }
}

// Sleep on channel chan, releasing condition lock lk.
// Re-acquires lk when awakened.
void sleep(void *chan, spinlock_t *lk) {
    proc_t *p = myproc();

    acquire(&p->lock);
    release(lk);

    p->chan = chan;
    p->state = SLEEPING;

    proc_sched();

    p->chan = 0;
    release(&p->lock);
    acquire(lk);
}

// Wake up all processes sleeping on channel chan.
// Caller should hold the condition lock.
void wakeup(void *chan) {
    proc_t *p;
    for (p = proc; p < &proc[NPROC]; p++) {
        if (p != myproc()) {
            acquire(&p->lock);
            if (p->state == SLEEPING && p->chan == chan) {
                p->state = RUNNABLE;
            }
            release(&p->lock);
        }
    }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void proc_scheduler(void) {
    proc_t *p;
    cpu_t *c = mycpu();

    c->proc = 0;
    for (;;) {
        intr_on();
        intr_off();

        int found = 0;
        for (p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if(p->state == RUNNABLE) {
                p->state = RUNNING;
                c->proc = p;
                swtch(&c->ctx, &p->ctx);

                c->proc = 0;
                found = 1;
            }
            release(&p->lock);
        }
        if (found == 0) {
            asm volatile("wfi");
        }
    }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void proc_sched(void) {
    int intena;
    proc_t *p = myproc();

    if (!holding(&p->lock))
        panic("sched p->lock");
    if(mycpu()->noff != 1)
        panic("sched locks");
    if(p->state == RUNNING)
        panic("sched RUNNING");
    if(intr_get())
        panic("sched interruptible");

    intena = mycpu()->intena;
    swtch(&p->ctx, &mycpu()->ctx);
    mycpu()->intena = intena;
}

// set up first user process
void init_zero(void) {
    proc_t *p;
    p = alloc_proc();
    proczero = p;
    release(&p->lock);
}


// Give up the CPU for one scheduling round.
void
yield(void)
{
  proc_t *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  proc_sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void) {
    extern char user_ret[];
    proc_t *p = myproc();
    static int first = 1;

    release(&p->lock);

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
    uint64 satp = MAKE_SATP(p->pgtbl);
    uint64 trampoline_userret = TRAMPOLINE + (user_ret - trampoline);
    ((void (*)(uint64))trampoline_userret)(satp);
}
