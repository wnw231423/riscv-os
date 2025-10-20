#include "types.h"
#include "defs.h"
#include "riscv.h"
#include "memlayout.h"

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
        //if (ticks % 100 == 0) {
            printf("timer tick %ld\n", ticks);
        //}
        timer_update();
    }

    w_stimecmp(r_time() + 1000000);
}

void external_interrupt_handler() {
    int irq = plic_claim();

    if (irq == UART0_IRQ) {
        uart_intr();
    } else {
        printf("Unknown external interrupt.\n");
    }

    if (irq) {
        plic_complete(irq);
    }

}