#include "types.h"
#include "defs.h"
#include "riscv.h"
#include "memlayout.h"

// 中断信息
char* interrupt_info[16] = {
    "U-mode software interrupt",      // 0
    "S-mode software interrupt",      // 1
    "reserved-1",                     // 2
    "M-mode software interrupt",      // 3
    "U-mode timer interrupt",         // 4
    "S-mode timer interrupt",         // 5
    "reserved-2",                     // 6
    "M-mode timer interrupt",         // 7
    "U-mode external interrupt",      // 8
    "S-mode external interrupt",      // 9
    "reserved-3",                     // 10
    "M-mode external interrupt",      // 11
    "reserved-4",                     // 12
    "reserved-5",                     // 13
    "reserved-6",                     // 14
    "reserved-7",                     // 15
};

// 异常信息
char* exception_info[16] = {
    "Instruction address misaligned", // 0
    "Instruction access fault",       // 1
    "Illegal instruction",            // 2
    "Breakpoint",                     // 3
    "Load address misaligned",        // 4
    "Load access fault",              // 5
    "Store/AMO address misaligned",   // 6
    "Store/AMO access fault",         // 7
    "Environment call from U-mode",   // 8
    "Environment call from S-mode",   // 9
    "reserved-1",                     // 10
    "Environment call from M-mode",   // 11
    "Instruction page fault",         // 12
    "Load page fault",                // 13
    "reserved-2",                     // 14
    "Store/AMO page fault",           // 15
};

// in trap.S, calls trap_kernel_handler()
void kernel_vector();

// helper functions
void timer_interrupt_handler();
void external_interrupt_handler();

void trap_kernel_init() {
    timer_create();
}

void trap_kernel_inithart(){
    w_stvec((uint64)kernel_vector);
}

void trap_kernel_handler() {
    uint64 sepc = r_sepc();
    uint64 scause = r_scause();
    
    if (scause == 0x8000000000000009L) {
        // SEI
        external_interrupt_handler();
    } else if(scause == 0x8000000000000005L) {
        // STI
        timer_interrupt_handler();
    } else {
        printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, sepc, r_stval());
        panic("UNK intr");
    }
}

void timer_interrupt_handler() {
    if (cpuid() == 0) {
        uint64 ticks = timer_get_ticks();
        if (ticks % 10 == 0) {
            printf("timer tick %ld\n", ticks);
        }
        timer_update();
    }

    w_stimecmp(r_time() + 1000000);
}

void external_interrupt_handler() {
    int irq = plic_claim();

    if (irq == UART0_IRQ) {
        uart_intr();
    } else if (irq) {
        printf("UNK external intr.");
    }

    if (irq) {
        plic_complete(irq);
    }
}
