#include "defs.h"
#include "memlayout.h"
#include "riscv.h"

// in trampoline.S
extern char trampoline[];      // 内核和用户切换的代码
extern char user_vector[];     // 用户触发trap进入内核
extern char user_return[];     // trap处理完毕返回用户

// in trap.S
extern char kernel_vector[];   // 内核态trap处理流程

// in trap_kernel.c
extern char* interrupt_info[16]; // 中断错误信息
extern char* exception_info[16]; // 异常错误信息

// 在user_vector()里面调用
// 用户态trap处理的核心逻辑
void trap_user_handler() {
    uint64 sepc = r_sepc();          // 记录了发生异常时的pc值
    uint64 sstatus = r_sstatus();    // 与特权模式和中断相关的状态信息
    uint64 scause = r_scause();      // 引发trap的原因
    uint64 stval = r_stval();        // 发生trap时保存的附加信息(不同trap不一样)

    // 确认trap来自U-mode
    if ((sstatus & SSTATUS_SPP) != 0) {
        panic("trap_user_handler: not from u-mode");
    }

    w_stvec((uint64)kernel_vector);

    proc_t* p = myproc();

    // save user program counter.
    p->trapframe->epc = r_sepc();

    if(scause == 8) {
        // system call
        p->trapframe->epc += 4;
        intr_on();
        printf("get a syscall from proc %d\n", myproc()->pid);
        syscall();
    } else if (scause == 0x8000000000000009L) {
        // SEI
        external_interrupt_handler();
    } else if(scause == 0x8000000000000005L) {
        // STI
        timer_interrupt_handler();
    } else {
        printf("unexpected scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, sepc, stval);
    }

    trap_user_return();
}

//
// set up trapframe and control registers for a return to user space
//
void
prepare_return(void)
{
  proc_t *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(). because a trap from kernel
  // code to usertrap would be a disaster, turn off interrupts.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (user_vector - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)trap_user_handler;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);
}


// 调用user_return()
// 内核态返回用户态
void trap_user_return() {
    proc_t *p = myproc();

    prepare_return();
    uint64 satp = MAKE_SATP(p->pgtbl);
    uint64 trampoline_userret = TRAMPOLINE + (user_return - trampoline);

    // for debug
    //vm_print(p->pgtbl);

    ((void (*)(uint64))trampoline_userret)(satp);
}
