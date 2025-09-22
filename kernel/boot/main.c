#include "defs.h"

volatile static int started = 0;

void main() {
    if (cpuid() == 0) {
        uartinit();
        printfinit();
        printf("hart %d starting\n", cpuid());
        started = 1;
    } else {
        while(started == 0)
            ;
        __sync_synchronize();
        printf("hart %d starting\n", cpuid());
    }
    while(1);
}