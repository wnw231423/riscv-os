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
    timer_interrupt_handler();
}

void timer_interrupt_handler() {
    if (cpuid() == 0) {
        uint64 ticks = timer_get_ticks();
        printf("timer tick %ld\n", ticks);
        timer_update();
    }

    w_stimecmp(r_time() + 1000000);
}