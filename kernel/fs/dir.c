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

// Directories

int namecmp(const char *s, const char *t) {
    return strncmp(s, t, DIRSIZ);
}

// look for a directory entry in a directiry
// if found, set *poff to byte offset of entry
struct inode* dirlookup(struct inode *dp, char *name, uint *poff) {
    uint off, inum;
    struct dirent de;

    if (dp->type != T_DIR)
        panic("dirlookup not DIR");

    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlookup read");
        if (de.inum == 0)
            continue;
        if (namecmp(name, de.name) == 0) {
            if (poff)
                *poff = off;
            inum = de.inum;
            return iget(dp->dev, inum);
        } 
    }

    return 0;
}

// write a new directory entry (name, inum) into the directory dp.
// returns 0 on success, -1 on failure
int dirlink(struct inode *dp, char *name, uint inum) {
    int off;
    struct dirent de;
    struct inode *ip;

    if ((ip = dirlookup(dp, name, 0)) != 0) {
        iput(ip);
        return -1;
    }

    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlink read");
        if (de.inum == 0)
            break;
    }

    strncmp(de.name, name, DIRSIZ);
    de.inum = inum;
    if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
        return -1;

    return 0;
}

// paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name) {
    char *s;
    int len;

    while(*path == '/')
        path++;
    if(*path == 0)
        return 0;
    s = path;
    while(*path != '/' && *path != 0)
        path++;
    len = path - s;
    if(len >= DIRSIZ)
        memmove(name, s, DIRSIZ);
    else {
        memmove(name, s, len);
        name[len] = 0;
    }
    while(*path == '/')
        path++;
    return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name) {
    struct inode *ip, *next;

    if(*path == '/')
        ip = iget(ROOTDEV, ROOTINO);
    else
        ip = idup(myproc()->cwd);

    while((path = skipelem(path, name)) != 0){
        ilock(ip);
        if(ip->type != T_DIR){
        iunlockput(ip);
        return 0;
        }
        if(nameiparent && *path == '\0'){
        // Stop one level early.
        iunlock(ip);
        return ip;
        }
        if((next = dirlookup(ip, name, 0)) == 0){
        iunlockput(ip);
        return 0;
        }
        iunlockput(ip);
        ip = next;
    }
    if(nameiparent){
        iput(ip);
        return 0;
    }
    return ip;
}

struct inode* namei(char *path) {
    char name[DIRSIZ];
    return namex(path, 0, name);
}

struct inode* nameiparent(char *path, char *name) {
  return namex(path, 1, name);
}
