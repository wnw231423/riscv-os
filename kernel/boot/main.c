#include "defs.h"
#include "riscv.h"
#include "types.h"

volatile static int started = 0;

volatile static int over_1 = 0, over_2 = 0;

static int* mem[1024];

int main() {
    int cpuid = r_tp();

    if(cpuid == 0) {
        uartinit();
        printfinit();
        pmem_init();

        printf("cpu %d is booting!\n", cpuid);
        __sync_synchronize();
        started = 1;

        for(int i = 0; i < 512; i++) {
            mem[i] = pmem_alloc(0);
            memset(mem[i], 1, PGSIZE);
            // printf("CPU0 in loop %d.mem = %p, data = %d\n", i, mem[i], mem[i][0]);
            printf("mem = %p, data = %d\n", mem[i], mem[i][0]);

        }
        printf("cpu %d alloc over\n", cpuid);
        over_1 = 1;
        
        while(over_1 == 0 || over_2 == 0);
        
        for(int i = 0; i < 512; i++)
            pmem_free((void*)mem[i]);
        printf("cpu %d free over\n", cpuid);
    } else {
        while(started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
        
        for(int i = 512; i < 1024; i++) {
            mem[i] = pmem_alloc(0);
            memset(mem[i], 1, PGSIZE);
            // printf("CPU%d in loop %d.mem = %p, data = %d\n",cpuid, i, mem[i], mem[i][0]);
            printf("mem = %p, data = %d\n", mem[i], mem[i][0]);
        }
        printf("cpu %d alloc over\n", cpuid);
        over_2 = 1;

        while(over_1 == 0 || over_2 == 0);

        for(int i = 512; i < 1024; i++)
            pmem_free((void*)mem[i]);
        printf("cpu %d free over\n", cpuid);        
 
    }
    while (1);    
}
