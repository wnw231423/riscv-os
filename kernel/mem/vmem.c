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

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64 vm_getpa(pagetbl_t pagetable, uint64 va) {
    pte_t *pte;
    uint64 pa;

    if(va >= MAXVA)
        return 0;

    pte = vm_getpte(pagetable, va, 0);
    if(pte == 0)
        return 0;
    if((*pte & PTE_V) == 0)
        return 0;
    if((*pte & PTE_U) == 0)
        return 0;
    pa = PTE2PA(*pte);
    return pa;
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

// allocate pages for user proc to grow its heap top from old to new
// return new heap top or 0 if error
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

// dealloc to bring the heap top from old to new
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

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
// Used in these cases:
//   1. to fork a proc, you need to do a vm_u_copy.
int vm_u_copy(pagetbl_t old, pagetbl_t new, uint64 sz) {
    pte_t *pte;
    uint64 pa, i;
    uint flags;
    char *mem;

    for (int i = 0; i < sz; i += PGSIZE) {
        if ((pte = vm_getpte(old, i, 0)) == 0)
            continue;
        if ((*pte & PTE_V) == 0)
            continue;
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);
        if ((mem = pmem_alloc(1)) == 0)
            goto err;
        memmove(mem, (char*)pa, PGSIZE);
        if (vm_mappages(new, i, PGSIZE, (uint64)mem, flags) != 0) {
            pmem_free(mem);
            goto err;
        }
    }
    return 0;

err:
    vm_unmappages(new, 0, i / PGSIZE, 1);
    return -1;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
// Used in these cases:
//   1. kwait syscall. copy the xstate of the child proc to parent proc's given addr.
int copyout(pagetbl_t pagetable, uint64 dstva, char *src, uint64 len) {
    uint64 n, va0, pa0;
    pte *pte;

    while (len > 0) {
        va0 = PGROUNDDOWN(dstva);
        if (va0 >= MAXVA)
            return -1;

        pa0 = vm_getpa(pagetable, va0);
        if (pa0 == 0) {
            return -1;
        }

        pte = vm_getpte(pagetable, va0, 0);
        if ((*pte & PTE_W) == 0)
            return -1;

        n = PGSIZE - (dstva -va0);
        if (n > len)
            n = len;
        memmove((void*)(pa0 + (dstva - va0)), src, n);

        len -= n;
        src += n;
        dstva = va0 + PGSIZE;
    }
    return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetbl_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = vm_getpa(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}

// allocate and map user memory if process is referencing a page
// that was lazily allocated in sys_sbrk().
// returns 0 if va is invalid or already mapped, or if
// out of physical memory, and physical address if successful.
// uint64
// vmfault(pagetable_t pagetable, uint64 va, int read)
// {
//   uint64 mem;
//   struct proc *p = myproc();

//   if (va >= p->heap_top)
//     return 0;
//   va = PGROUNDDOWN(va);
//   if(ismapped(pagetable, va)) {
//     return 0;
//   }
//   mem = (uint64) pmem_alloc(1);
//   if(mem == 0)
//     return 0;
//   memset((void *) mem, 0, PGSIZE);
//   if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W|PTE_U|PTE_R) != 0) {
//     pmem_free((void *)mem);
//     return 0;
//   }
//   return mem;
// }

// int
// ismapped(pagetable_t pagetable, uint64 va)
// {
//   pte_t *pte = walk(pagetable, va, 0);
//   if (pte == 0) {
//     return 0;
//   }
//   if (*pte & PTE_V){
//     return 1;
//   }
//   return 0;
// }


