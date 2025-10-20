#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

// plic.c
void plic_init(void) {
    *(uint32*)(PLIC_PRIORITY(UART0_IRQ)) = 1;
}

void plic_init_hart(void) {
    int hart = cpuid();

    // set enable bits for this hart's S-mode
    // for the uart.
    *(uint32*)PLIC_SENABLE(hart) = (1 << UART0_IRQ);

    // set this hart's S-mode priority threshold to 0.
    *(uint32*)PLIC_SPRIORITY(hart) = 0;
}

int plic_claim(void) {
    int hart = cpuid();
    int irq = *(uint32*)PLIC_SCLAIM(hart);
    return irq;
}

void plic_complete(int irq) {
    int hart = cpuid();
    *(uint32*)PLIC_SCLAIM(hart) = irq;
}