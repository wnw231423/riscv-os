#include "types.h"
#include "riscv.h"

void main();

__attribute__ ((aligned (16))) char stack0[4096];

// entry.S jumps here
void start() {
    // set M Previous Privilege mode to Supervisor, for mret.
    //unsigned long x = r_mstatus();
    //x &= ~MSTATUS_MPP_MASK;
    //x |= MSTATUS_MPP_S;
    //w_mstatus(x);

    // set M Exception Program Counter to main, for mret.
    // requires gcc -mcmodel=medany
    //w_mepc((uint64)main);
    
    // switch to supervisor mode and jump to main().
    //asm volatile("mret");
    main();
}