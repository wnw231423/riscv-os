#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs/buf.h"
#include "fs/fs.h"

extern struct superblock sb;

// Zero a block.
static void bzero(int dev, int bno)
{
  struct buf *bp;

  bp = buf_read(dev, bno);
  memset(bp->data, 0, BSIZE);
  buf_write(bp);
  buf_release(bp);
}

// allocate a zeroed disk block
// returns 0 if out of disk space
uint balloc(uint dev) {
    int b, bi, m;
    struct buf *bp;

    bp = 0;
    for (b = 0; b < sb.size; b += BPB) {
        bp = buf_read(dev, BBLOCK(b, sb));
        for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
            m = 1 << (bi % 8);
            if((bp->data[bi/8] & m) == 0) {
                bp->data[bi/8] |= m;  // Mark block in use.
                buf_write(bp);
                buf_release(bp);
                bzero(dev, b + bi);
                return b + bi;
            }
        }
        buf_release(bp);
    }
    printf("balloc: out of blocks\n");
    return 0;
}

// free a disk block
void bfree(int dev, uint b) {
    struct buf *bp;
    int bi, m;

    bp = buf_read(dev, BBLOCK(b, sb));
    bi = b % BPB;
    m = 1 << (bi % 8);
    if((bp->data[bi/8] & m) == 0)
        panic("freeing free block");
    bp->data[bi/8] &= ~m;
    buf_write(bp);
    buf_release(bp);
}
