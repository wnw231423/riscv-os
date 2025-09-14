// qemu virtual machine memory layout
// 0x00001000, boot ROM
// 0x10000000, uart0
// 0x80000000, boot ROM loads our kernel here and jumps to here.

// UART register
#define UART0 0x10000000L