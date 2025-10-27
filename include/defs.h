#ifndef DEFS_H
#define DEFS_H

#include "types.h"
#include "lib/spinlock.h"

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
struct cpu *mycpu(void);
void proc_make_first();
pagetable_t proc_pgtbl_init(uint64 trapframe);

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
int vm_mappages(pagetbl_t, uint64, uint64, uint64, int);
void vm_unmappages(pagetbl_t, uint64, uint64, int);
pagetbl_t vm_upage_create();
void vm_upage_free(pagetbl_t pagetable, uint64 sz);

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

// tests
void lab2p1(void);
void lab2p2(void);

#endif
