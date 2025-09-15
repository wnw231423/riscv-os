#ifndef DEFS_H
#define DEFS_H

struct spinlock;

// uart.c
void uartinit(void);
void uart_putc_sync(char c);
void uart_puts(char* s);

// spinlock.c
void            acquire(struct spinlock*);
int             holding(struct spinlock*);
void            initlock(struct spinlock*, char*);
void            release(struct spinlock*);
void            push_off(void);
void            pop_off(void);

// proc.c
int cpuid(void);
struct cpu* mycpu(void);

// printf.c
int             printf(char*, ...) __attribute__ ((format (printf, 1, 2)));
void            panic(char*) __attribute__((noreturn));
void            printfinit(void);

#endif