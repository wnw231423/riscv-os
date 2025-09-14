// driver for 16550a UART
// ONLY implements char output

#include "memlayout.h"

// a macro which gets device register by address based on UART0
#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))
// a macro which reads a reg
#define ReadReg(reg) (*(Reg(reg)))
// a macro which writes data to a reg
#define WriteReg(reg, v) (*(Reg(reg)) = v)

// control registers
#define THR 0                  // transmit holding register (for output)
#define LSR 5                  // line status register

// see http://byterunner.com/16550.html
// LSR BIT 5:
// 0 = transmit holding register is full. 16550 will not accept any data for transmission.
// 1 = transmitter hold register (or FIFO) is empty. CPU can load the next character.
// Note that a write operation should be performed when the transmit holding register empty flag is set.
#define LSR_EMPTY_FLAG (1<<5)  // EMPTY FLAG for THR

void uart_putc(char c) {
    while ((ReadReg(LSR) & LSR_EMPTY_FLAG) == 0);
    WriteReg(THR, c);
}

void uart_puts(char* s) {
    while (*s != '\0') {
        uart_putc(*s);
        s++;
    }
}