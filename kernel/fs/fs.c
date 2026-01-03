#include "types.h"
#include "param.h"
#include "lib/spinlock.h"
#include "lib/sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs/fs.h"
#include "fs/buf.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb; 

// Read the super block.
static void readsb(int dev, struct superblock *sb) {
    struct buf *bp;

    bp = buf_read(dev, 1);
    memmove(sb, bp->data, sizeof(*sb));
    buf_release(bp);
}

// Init fs
void fsinit(int dev) {
    readsb(dev, &sb);
    if(sb.magic != FSMAGIC)
        panic("invalid file system");
    //initlog(dev, &sb);
    ireclaim(dev);
}
