#include "types.h"
#include "defs.h"
#include "riscv.h"
#include "mem/pmem.h"
#include "memlayout.h"

void freerange(void *pa_start, void *pa_end);

extern char end[];  // defined by kernel.ld

// init physical memory.
// alloc pmem for two regions -- kernel and user region.
void pmem_init() {
    initlock(&kern_region.lock, "kern_region");
    initlock(&user_region.lock, "user_region");
    freerange(end, (void*)KERN_USER_LINE);
    freerange((void*)KERN_USER_LINE, (void*)PHYSTOP);

    // look the top 5 free page in list. for debug.
    // page_node_t *u = user_region.free_list;
    // page_node_t *k = kern_region.free_list;
    // for (int i = 0; i < 5; i++) {
    //     printf("User list # %d: %p\n", i, u);
    //     printf("Kernel list # %d: %p\n", i, k);
    //     u = u->next;
    //     k = k->next;
    // }
}

void freerange(void *pa_start, void *pa_end) {
    if (((uint64)pa_start < KERN_USER_LINE) && ((uint64)pa_end > KERN_USER_LINE))
        panic("freerange");

    char *p;
    p = (char*)PGROUNDUP((uint64)pa_start);
    for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
        pmem_free(p);
}

// alloc a free page from kernel or user free page linklist
// depending on type, 0 for kernel, 1 for user
void* pmem_alloc(int type) {
    page_node_t *p;
    alloc_region_t *region;

    if (type == 0) {
        region = &kern_region;
    } else if (type == 1) {
        region = &user_region;
    } else {
        panic("pmem_alloc");
    }

    acquire(&region->lock);
    p = region->free_list;
    
    if (region->num > 0) {
        region->free_list = p->next;  
        region->num--;
    }
    release (&region->lock);

    if (p) {
        memset((char*)p, 5, PGSIZE);
    }

    return (void*)p;
}

void pmem_free(void *pa) {
    page_node_t *p;
    alloc_region_t *region;

    if (((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa > PHYSTOP) {
        panic("pmem_free");
    }

    memset(pa, 1, PGSIZE);

    if ((uint64)pa < KERN_USER_LINE) {
        region = &user_region;
    } else {
        region = &kern_region;
    }

    p = (page_node_t*) pa;
    
    acquire(&region->lock);
    p->next = region->free_list;
    region->free_list = p;
    region->num++;
    release(&region->lock);
}
