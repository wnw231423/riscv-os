#ifndef DEFS_H
#define DEFS_H

#include "types.h"
#include "lib/spinlock.h"
#include "lib/sleeplock.h"
#include "proc/proc.h"
#include "fs/buf.h"
#include "fs/file.h"

// uart.c
void            uartinit(void);
void            uartintr(void);
void            uartwrite(char [], int);
void            uartputc_sync(int);
int             uartgetc(void);

// spinlock.c
void acquire(spinlock_t *);
int holding(spinlock_t *);
void initlock(spinlock_t *, char *);
void release(spinlock_t *);
void push_off(void);
void pop_off(void);

// sleeplock.c
void            acquiresleep(struct sleeplock*);
void            releasesleep(struct sleeplock*);
int             holdingsleep(struct sleeplock*);
void            initsleeplock(struct sleeplock*, char*);

// proc.c
int cpuid(void);
cpu_t *mycpu(void);
proc_t *myproc(void);
//void proc_make_first();
pagetbl_t proc_pagetable(proc_t *);
//pagetbl_t proc_pgtbl_init(uint64);
void proc_free_pagetable(pagetbl_t, uint64);
void proc_init(void);
void proc_mapstacks(pagetbl_t);
void init_zero(void);
void proc_scheduler(void) __attribute__((noreturn));
void proc_sched(void);
int grow_proc(int);
void kexit(int);
int kfork(void);
void sleep(void*, spinlock_t*);
void wakeup(void*);
void yield(void);
int killed(proc_t*);
int kwait(uint64);

int either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int either_copyin(void *dst, int user_src, uint64 src, uint64 len);


// printf.c
int printf(char *, ...) __attribute__((format(printf, 1, 2)));
void panic(char *) __attribute__((noreturn));
void printfinit(void);

// str.c
int memcmp(const void *, const void *, uint);
void *memmove(void *, const void *, uint);
void *memset(void *, int, uint);
char *safestrcpy(char *, const char *, int);
int strlen(const char *);
int strncmp(const char *, const char *, uint);
char *strncpy(char *, const char *, int);

// pmem.c
void pmem_init(void);
void *pmem_alloc(int);
void pmem_free(void *);

// vmem.c
pagetbl_t kvmmake(void);
void kvmmap(pagetbl_t, uint64, uint64, uint64, int);

void vm_print(pagetbl_t);

void kvminit(void);
void kvminithart(void);

pte_t *vm_getpte(pagetbl_t, uint64, int);
uint64 vm_getpa(pagetbl_t, uint64);

int vm_mappages(pagetbl_t, uint64, uint64, uint64, int);
void vm_unmappages(pagetbl_t, uint64, uint64, int);

pagetbl_t vm_upage_create();
void vm_upage_free(pagetbl_t pagetable, uint64 sz);
uint64 vm_u_alloc(pagetbl_t, uint64, uint64, int);
uint64 vm_u_dealloc(pagetbl_t, uint64, uint64);
int vm_u_copy(pagetbl_t, pagetbl_t, uint64);

int copyout(pagetbl_t, uint64, char*, uint64);
int copyinstr(pagetbl_t, char*, uint64, uint64);

int copyin (pagetbl_t, char *, uint64, uint64);

void uvmclear(pagetbl_t, uint64);


// timer.c
void timer_init();
void timer_create();
void timer_update();
uint64 timer_get_ticks();

// plic.c
void plic_init(void);
void plic_init_hart(void);
int plic_claim(void);
void plic_complete(int);

// trap_kernel.c
void trap_kernel_init();
void trap_kernel_inithart();
void trap_kernel_handler();
void timer_interrupt_handler();
void external_interrupt_handler();

// trap_user.c
void prepare_return();
//void trap_user_return();

// syscall.c
void arg_int(int, int *);
void arg_uint32(int, uint32*);
void arg_uint64(int, uint64*);
int arg_str(int, char*, int);
void argaddr(int, uint64*);
int fetchaddr(uint64, uint64*);
int fetchstr(uint64, char*, int);
void syscall();

// virtio_disk.c
void            virtio_disk_init(void);
void            virtio_disk_rw(struct buf *, int);
void            virtio_disk_intr(void);

// bio.c
void binit(void);
struct buf* buf_read(uint, uint);
void buf_release(struct buf*);
void buf_write(struct buf*);

// bitmap.c
uint balloc(uint);
void bfree(int, uint);

// inode.c
struct inode*   ialloc(uint, short);
struct inode*   idup(struct inode*);
void            iinit();
void            ilock(struct inode*);
void            iput(struct inode*);
void            iunlock(struct inode*);
void            iunlockput(struct inode*);
void            iupdate(struct inode*);
int             readi(struct inode*, int, uint64, uint, uint);
void            stati(struct inode*, struct stat*);
int             writei(struct inode*, int, uint64, uint, uint);
void            itrunc(struct inode*);
void            ireclaim(int);
void            printi(struct inode*);

struct inode*   iget(uint, uint);

// dir.c
int             dirlink(struct inode*, char*, uint);
struct inode*   dirlookup(struct inode*, char*, uint*);
int             namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);

// file.c
struct file*    filealloc(void);
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit(void);
int             fileread(struct file*, uint64, int n);
int             filestat(struct file*, uint64 addr);
int             filewrite(struct file*, uint64, int n);

// fs.c
void            fsinit(int);

// pipe.c
int             pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, uint64, int);
int             pipewrite(struct pipe*, uint64, int);

// exec.c
int             kexec(char*, char**);

// console.c
void            consoleinit(void);
void            consoleintr(int);
void            consputc(int);

// tests
void lab2p1(void);
void lab2p2(void);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#endif
