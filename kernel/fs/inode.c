#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs/stat.h"
#include "lib/spinlock.h"
#include "lib/sleeplock.h"
#include "fs/fs.h"
#include "fs/file.h"
#include "fs/buf.h"

// Inode layer

extern struct superblock sb;

struct {
    spinlock_t lock;
    struct inode inode[NINODE];
} itable;

void iinit() {
    int i = 0;

    initlock(&itable.lock, "itable");
    for (i = 0; i < NINODE; i++) {
        initsleeplock(&itable.inode[i].lock, "inode");
    }
}

// allocate an inode
// returns an unlocked but allocated and referenced inode
// or NULL if there is no free inode
struct inode* ialloc(uint dev, short type) {
    int inum;
    struct buf *bp;
    struct dinode *dip;

    for (inum = 1; inum < sb.ninodes; inum++) {
        bp = buf_read(dev, IBLOCK(inum, sb));
        dip = (struct dinode*)bp->data + inum%IPB;
        if (dip->type == 0) {
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            buf_write(bp);
            buf_release(bp);
            return iget(dev, inum);
        }
        buf_release(bp);
    }
    printf("ialloc: no inodes\n");
    return 0;
}

// update a modified inode to disk
void iupdate(struct inode *ip) {
    struct buf *bp;
    struct dinode *dip;

    bp = buf_read(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ip->nlink;
    dip->size = ip->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    buf_write(bp);
    buf_release(bp);
}

// find the inode with number inum
// return the in-memory copy.
// doesn't lock the inode and doesn't read it from disk
struct inode* iget(uint dev, uint inum) {
    struct inode *ip, *empty;
    acquire(&itable.lock);

    empty = 0;
    for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++) {
        if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
            ip->ref++;
            release(&itable.lock);
            return ip;
        }
        if (empty == 0 && ip->ref == 0) {
            empty = ip;
        }
    }

    if (empty == 0) {
        panic("iget: no inodes");
    }

    ip = empty;
    ip->dev = dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0;
    release(&itable.lock);

    return ip;
}

// increment ref count for ip
// returns ip to enable ip = idup(ip1) idiom
struct inode* idup(struct inode *ip) {
    acquire(&itable.lock);
    ip->ref++;
    release(&itable.lock);
    return ip;
}

// lock the given inode
// reads the inode from disk if necessary
void ilock(struct inode *ip)
{
    struct buf *bp;
    struct dinode *dip;

    if(ip == 0 || ip->ref < 1)
        panic("ilock");

    acquiresleep(&ip->lock);

    if(ip->valid == 0){
        bp = buf_read(ip->dev, IBLOCK(ip->inum, sb));
        dip = (struct dinode*)bp->data + ip->inum%IPB;
        ip->type = dip->type;
        ip->major = dip->major;
        ip->minor = dip->minor;
        ip->nlink = dip->nlink;
        ip->size = dip->size;
        memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
        buf_release(bp);
        ip->valid = 1;
        if(ip->type == 0)
        panic("ilock: no type");
    }
}

// unlock the given inode
void iunlock(struct inode *ip) {
    if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
        panic("iunlock");

    releasesleep(&ip->lock);
}

// drop a reference to an in-memory inode
// if that was the last ref, the inode can be recycled
// if that was the last ref and the inode has no links to it, free the inode on disk
void iput(struct inode *ip) {
    acquire(&itable.lock);

    if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
        // inode has no links and no other references: truncate and free.
        
        // ip->ref == 1 means no other process can have ip locked,
        // so this acquiresleep() won't block (or deadlock).
        acquiresleep(&ip->lock);
        
        release(&itable.lock);
        
        itrunc(ip);
        ip->type = 0;
        iupdate(ip);
        ip->valid = 0;

        releasesleep(&ip->lock);

        acquire(&itable.lock);
    }

    ip->ref--;
    release(&itable.lock);
}

// common idiom: unlock, then put
void iunlockput(struct inode *ip) {
    iunlock(ip);
    iput(ip);
}

void ireclaim(int dev) {
    for (int inum = 1; inum < sb.ninodes; inum++) {
        struct inode *ip = 0;
        struct buf *bp = buf_read(dev, IBLOCK(inum, sb));
        struct dinode *dip = (struct dinode *)bp->data + inum % IPB;
        if (dip->type != 0 && dip->nlink == 0) {  // is an orphaned inode
            printf("ireclaim: orphaned inode %d\n", inum);
            ip = iget(dev, inum);
        }
        buf_release(bp);
        if (ip) {
            ilock(ip);
            iunlock(ip);
            iput(ip);
        }
    }
}

// inode content
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// return the disk block address of the nth block in inode ip
// if there is no such block, bmap allocates one
// returns 0 if out of disk space
static uint bmap(struct inode *ip, uint bn) {
    uint addr, *a;
    struct buf *bp;

    if (bn < NDIRECT) {
        if ((addr = ip->addrs[bn]) == 0) {
            addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            ip->addrs[bn] = addr;
        }
        return addr;
    }
    bn -= NDIRECT;

    if (bn < NINDIRECT) {
        // load indirect block, allocating if necessary
        if ((addr = ip->addrs[NDIRECT]) == 0) {
            addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            ip->addrs[NDIRECT] = addr;
        }
        bp = buf_read(ip->dev, addr);
        a = (uint*)bp->data;
        if((addr = a[bn]) == 0) {
            addr = balloc(ip->dev);
            if (addr) {
                a[bn] = addr;
                buf_write(bp);
            }
        }
        buf_release(bp);
        return addr;
    }

    panic("bmap: out of range");
}

// truncate inode (discard contents)
void itrunc(struct inode *ip) {
    int i, j;
    struct buf *bp;
    uint *a;

    for(i = 0; i < NDIRECT; i++) {
        if(ip->addrs[i]) {
            bfree(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if (ip->addrs[NDIRECT]) {
        bp = buf_read(ip->dev, ip->addrs[NDIRECT]);
        a = (uint*)bp->data;
        for(j = 0; j < NINDIRECT; j++){
            if(a[j])
                bfree(ip->dev, a[j]);
        }
        buf_release(bp);
        bfree(ip->dev, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;       
    }

    ip->size = 0;
    iupdate(ip);
}

// copy stat infomation from inode
void stati(struct inode *ip, struct stat *st) {
    st->dev = ip->dev;
    st->ino = ip->inum;
    st->type = ip->type;
    st->nlink = ip->nlink;
    st->size = ip->size;
}

#define min(a, b) ((a) < (b) ? (a) : (b))
// read data from inode
// if user_dst==1, then dst is a user virtual address;
// otherwise, dst is a kernel address
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n) {
    uint tot, m;
    struct buf *bp;

    if (off > ip->size || off +n < off)
        return 0;
    if (off + n > ip->size)
        n = ip->size - off;

    for (tot = 0; tot < n; tot += m, off += m, dst += m) {
        uint addr = bmap(ip, off/BSIZE);
        if(addr == 0)
            break;
        bp = buf_read(ip->dev, addr);
        m = min(n - tot, BSIZE - off%BSIZE);
        if (either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
            buf_release(bp);
            tot = -1;
            break;
        }
        buf_release(bp);
    }
    return tot;
}

// write data to inode
// if user_src==1, then src is a user virtual addr; otherwise kernel addr
// returns the number of bytes successfully written.
// if return value is less than requested n, there was an error of some kind
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n) {
    uint tot, m;
    struct buf *bp;

    if (off > ip->size || off + n < off)
        return -1;
    if (off + n > MAXFILE*BSIZE)
        return -1;

    for (tot = 0; tot < n; tot += m, off += m, src += m) {
        uint addr = bmap(ip, off/BSIZE);
        if (addr == 0)
            break;
        bp = buf_read(ip->dev, addr);
        m = min(n - tot, BSIZE - off%BSIZE);
        if (either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
            buf_release(bp);
            break;
        }
        buf_write(bp);
        buf_release(bp);
    }

    if (off > ip->size)
        ip->size = off;

    iupdate(ip);

    return tot;
}

static char* inode_types[] = {
    "INODE_UNUSED",
    "INODE_DIR",
    "INODE_FILE",
    "INODE_DEVICE",
};

void printi(struct inode *ip) {
    if (!holdingsleep(&ip->lock)) {
        printf("printi: lk\n");
        return;
    }

    printf("\ninode information:\n");
    printf("num = %d, ref = %d, valid = %d\n", ip->inum, ip->ref, ip->valid);
    printf("type = %s, major = %d, minor = %d, nlink = %d\n", inode_types[ip->type], ip->major, ip->minor, ip->nlink);
    printf("size = %d, addrs =", ip->size);
    for (int i = 0; i < NDIRECT + 1; i++) {
        printf(" %d", ip->addrs[i]);
    }
    printf("\n");
}
