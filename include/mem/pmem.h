#include "types.h"
#include "lib/spinlock.h"

typedef struct page_node {
    struct page_node *next;
} page_node_t;

typedef struct alloc_region {
    uint64 begin;
    uint64 end;
    spinlock lock;
    uint32 num;
    page_node_t *free_list;
} alloc_region_t;

static alloc_region_t kern_region, user_region;