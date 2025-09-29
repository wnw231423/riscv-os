#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H
// qemu virtual machine memory layout
// 0x00001000, boot ROM
// 0x10000000, uart0
// 0x80000000, boot ROM loads our kernel here and jumps to here.

// UART register
#define UART0 0x10000000L

// physical address for kernel and user pages
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128 * 1024 * 1024)
#define KERN_USER_LINE (PHYSTOP - 32 * 1024 * 1024)

#endif
