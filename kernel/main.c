#include "defs.h"

void main() {
    if (cpuid() == 0) {
        uartinit();
        printfinit();
    } else {
        __sync_synchronize();
        printf("hart %d starting\n", cpuid());
    }
    while(1);
}