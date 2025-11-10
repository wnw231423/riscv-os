#include "types.h"
#include "riscv.h"
#include "memlayout.h"
#include "param.h"
#include "defs.h"

pagetbl_t kernel_pagetable;

// in trampoline.S
extern char trampoline[];

extern char etext[];  // kernel.ld sets this to end of kernel code.

// Make a direct-map page table for the kernel.
pagetbl_t
kvmmake(void)
{
  pagetbl_t kpgtbl;

  kpgtbl = (pagetbl_t) pmem_alloc(0);
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  //kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetbl_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(vm_mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch the current CPU's h/w page table register to
// the kernel's page table, and enable paging.
void
kvminithart(void)
{
  // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// helper function of vm_print
void
vmprint_helper(pagetbl_t pagetable, uint64 start, int level){
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if (pte & PTE_V){
      for (int j = 3; j > level; j--){
        printf(" ..");
      }
      uint64 child = PTE2PA(pte);
      printf("%p: pte %p pa %p\n", (void*)start, (void*)pte, (void*)child);
      if (level > 0) {
        //  child is sub-pagetable.
        vmprint_helper((pagetbl_t)child, start, level - 1);
      }
    }
    start += (1 << PXSHIFT(level));
  }
}

// print a pagetable
void
vm_print(pagetbl_t pagetable) {
  printf("page table %p\n", pagetable);
  vmprint_helper(pagetable, 0, 2);
}

// same as "walk" in xv6
pte_t* vm_getpte(pagetbl_t pagetable, uint64 va, int alloc) {
  if(va >= MAXVA)
  panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetbl_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pte_t*)pmem_alloc(0)) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

int vm_mappages(pagetbl_t pagetable, uint64 va, uint64 size, uint64 pa, int perm) {
  uint64 a, last;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("mappages: va not aligned");

  if((size % PGSIZE) != 0)
    panic("mappages: size not aligned");

  if(size == 0)
    panic("mappages: size");
  
  a = va;
  last = va + size - PGSIZE;
  for(;;){
    if((pte = vm_getpte(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

void vm_unmappages(pagetbl_t pagetable, uint64 va, uint64 npages, int do_free) {
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = vm_getpte(pagetable, a, 0)) == 0) // leaf page table entry allocated?
      continue;   
    if((*pte & PTE_V) == 0)  // has physical page been allocated?
      continue;
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      pmem_free((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table
// return 0 if out of memory
pagetbl_t vm_upage_create() {
    pagetbl_t pagetable;
    pagetable = (pagetbl_t) pmem_alloc(1);
    if(pagetable == 0) {
        return 0;
    }
    memset(pagetable, 0, PGSIZE);
    return pagetable;
}

// Recursively free page-table pages
// All leaf mappings must already haven been removed
void vm_freewalk(pagetbl_t pagetable) {
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
            // this PTE points to a lower-level page table.
            uint64 child = PTE2PA(pte);
            vm_freewalk((pagetbl_t)child);
            pagetable[i] = 0;
        } else if (pte & PTE_V) {
            panic("freewalk: leaf");
        }
    }
    pmem_free((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages
void vm_upage_free(pagetbl_t pagetable, uint64 sz) {
    if (sz > 0)
        vm_unmappages(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
    vm_freewalk(pagetable);
}

// allocate pages for user proc to grow its heap
// return newsz or 0 if error
uint64 vm_u_alloc(pagetbl_t pagetable, uint64 oldsz, uint64 newsz, int xperm) {
    char *mem;
    uint64 a;

    if(newsz < oldsz)
        return oldsz;

    oldsz = PGROUNDUP(oldsz);
    for (a = oldsz; a < newsz; a += PGSIZE) {
        mem = pmem_alloc(1);
        if (mem == 0) {
            vm_u_dealloc(pagetable, a, oldsz);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if(vm_mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R | PTE_U | xperm) != 0) {
            pmem_free(mem);
            vm_u_dealloc(pagetable, a, oldsz);
            return 0;
        }
    }
    return newsz;
}

// dealloc to bring the heap top from oldsz to newsz
// return the newsz
uint64 vm_u_dealloc(pagetbl_t pagetable, uint64 oldsz, uint64 newsz) {
    if (newsz >= oldsz)
        return oldsz;

    if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
        int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
        vm_unmappages(pagetable, PGROUNDUP(newsz), npages, 1);
    }

    return newsz;
}
