#include "defs.h"
#include "riscv.h"
#include "types.h"

volatile static int started = 0;

void main() {
    int cpuid = r_tp();

    if(cpuid == 0) {
        uartinit();
        printfinit();
        pmem_init();
        kvminit();
        kvminithart();
        trap_kernel_init();
        trap_kernel_inithart();

        printf("cpu %d is booting!\n", cpuid);
        __sync_synchronize();
        started = 1;

    } else {
        while(started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid); 
        kvminithart();
        trap_kernel_inithart();     
    }

    while (1);    
}
