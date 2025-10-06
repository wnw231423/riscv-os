#include "types.h"
#include "defs.h"
#include "riscv.h"


void lab2p2()
{
    int cpuid = r_tp();

    if(cpuid == 0) {
        pagetbl_t test_pgtbl = pmem_alloc(0);
        memset(test_pgtbl, 0, PGSIZE);

        uint64 mem[5];
        for(int i = 0; i < 5; i++)
            mem[i] = (uint64)pmem_alloc(0);

        printf("\ntest-1\n\n");    
        vm_mappages(test_pgtbl, 0, PGSIZE, mem[0], PTE_R);
        //vm_mappages(test_pgtbl, PGSIZE * 10, PGSIZE / 2, mem[1], PTE_R | PTE_W);  //size未按页对齐
        //vm_mappages(test_pgtbl, PGSIZE * 512, PGSIZE - 1, mem[2], PTE_R | PTE_X); //size未按页对齐
        vm_mappages(test_pgtbl, PGSIZE * 512, PGSIZE, mem[2], PTE_R | PTE_X);
        vm_mappages(test_pgtbl, PGSIZE * 512 * 512, PGSIZE, mem[2], PTE_R | PTE_X);
        vm_mappages(test_pgtbl, MAXVA - PGSIZE, PGSIZE, mem[4], PTE_W);
        vm_print(test_pgtbl);

        printf("\ntest-2\n\n");    
        //vm_mappages(test_pgtbl, 0, PGSIZE, mem[0], PTE_W);  //Remap
        vm_unmappages(test_pgtbl, PGSIZE * 10, PGSIZE, 1);
        vm_unmappages(test_pgtbl, PGSIZE * 512, PGSIZE, 1);
        vm_print(test_pgtbl);
    }
    while (1);    
}
