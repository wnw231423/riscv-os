#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H
// qemu virtual machine memory layout
// 0x00001000, boot ROM
// 0x02000000, CLINT
// 0x0C000000, PLIC
// 0x10000000, uart0
// 0x80000000, boot ROM loads our kernel here and jumps to here.

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000L
#define PLIC_PRIORITY(id) (PLIC + (id)*4)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// UART register
#define UART0 0x10000000L
#define UART0_IRQ 10

// physical address for kernel and user pages
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128 * 1024 * 1024)
#define KERN_USER_LINE (PHYSTOP - 32 * 1024 * 1024)

#endif
