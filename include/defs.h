#ifndef DEFS_H
#define DEFS_H

#include "types.h"
#include "lib/spinlock.h"
#include "proc/proc.h"

// uart.c
void uartinit(void);
void uart_putc_sync(char c);
void uart_puts(char *s);
void uart_intr(void);

// spinlock.c
void acquire(spinlock_t *);
int holding(spinlock_t *);
void initlock(spinlock_t *, char *);
void release(spinlock_t *);
void push_off(void);
void pop_off(void);

// proc.c
int cpuid(void);
cpu_t *mycpu(void);
proc_t *myproc(void);
//void proc_make_first();
//pagetbl_t proc_pgtbl_init(uint64);
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
int kwait(uint64);

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
uint64 vm_getpe(pagetbl_t, uint64);

int vm_mappages(pagetbl_t, uint64, uint64, uint64, int);
void vm_unmappages(pagetbl_t, uint64, uint64, int);

pagetbl_t vm_upage_create();
void vm_upage_free(pagetbl_t pagetable, uint64 sz);
uint64 vm_u_alloc(pagetbl_t, uint64, uint64, int);
uint64 vm_u_dealloc(pagetbl_t, uint64, uint64);
int vm_u_copy(pagetbl_t, pagetbl_t, uint64);

int copyout(pagetbl_t, uint64, char*, uint64);
int copyinstr(pagetbl_t, char*, uint64, uint64);

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
void syscall();

// tests
void lab2p1(void);
void lab2p2(void);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#endif
