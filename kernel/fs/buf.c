// buffer layer

#include "types.h"
#include "param.h"
#include "lib/spinlock.h"
#include "lib/sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs/fs.h"
#include "fs/buf.h"

struct {
    spinlock_t lock;
    struct buf buf[NBUF];

    // double linked list of all buffers
    struct buf head;
} bcache;

void binit(void) {
    struct buf *b;

    initlock(&bcache.lock, "bcache");

    // Create linked list of buffers
    bcache.head.prev = &bcache.head;
    bcache.head.next = &bcache.head;
    for(b = bcache.buf; b < bcache.buf+NBUF; b++){
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        initsleeplock(&b->lock, "buffer");
        bcache.head.next->prev = b;
        bcache.head.next = b;
    }
}

// bget
// helper function
static struct buf* bget(uint dev, uint blockno) {
    struct buf *b;

    acquire(&bcache.lock);

    // check if the block already cached
    for(b = bcache.head.next; b != &bcache.head; b = b->next){
        if(b->dev == dev && b->blockno == blockno){
            b->refcnt++;
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // not cached. LRU strategy.
    for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
        if(b->refcnt == 0) {
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            b->refcnt = 1;
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    panic("bget: no buffers");
}

struct buf* buf_read(uint dev, uint blockno) {
    struct buf *b;

    b = bget(dev, blockno);
    if (!b->valid) {
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

void buf_write(struct buf *b) {
    if (!holdingsleep(&b->lock)) {
        panic("buf_write");
    }
    virtio_disk_rw(b, 1);
}

void buf_release(struct buf *b) {
    if (!holdingsleep(&b->lock))
        panic("buf_release");

    releasesleep(&b->lock);

    acquire(&bcache.lock);
    b->refcnt--;
    if (b->refcnt == 0) {
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        bcache.head.next->prev = b;
        bcache.head.next = b;
    }

    release(&bcache.lock);
}

void buf_print(void) {
    printf("\nbuf_cache:\n");
    struct buf *b = bcache.head.next;
    acquire(&bcache.lock);
    while(b != &bcache.head) {
        printf("buf %d: ref = %d, block_num = %d\n", (int)(b - bcache.buf), b->refcnt, b->blockno);
        for (int i = 0; i < 8; i++) {
            printf("%d ", b->data[i]);
        }
        printf("\n");
        b = b->next;
    }
    release(&bcache.lock);
}
