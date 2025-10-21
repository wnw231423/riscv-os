// driver for 16550a UART
// ONLY implements char output

#include "memlayout.h"
#include "defs.h"

// a macro which gets device register by address based on UART0
#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))
// a macro which reads a reg
#define ReadReg(reg) (*(Reg(reg)))
// a macro which writes data to a reg
#define WriteReg(reg, v) (*(Reg(reg)) = v)

#define RHR 0                 // receive holding register (for input bytes)
#define THR 0                 // transmit holding register (for output bytes)
#define IER 1                 // interrupt enable register
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // FIFO control register
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
#define ISR 2                 // interrupt status register
#define LCR 3                 // line control register
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
#define LSR 5                 // line status register
#define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR

// see http://byterunner.com/16550.html
// LSR BIT 5:
// 0 = transmit holding register is full. 16550 will not accept any data for transmission.
// 1 = transmitter hold register (or FIFO) is empty. CPU can load the next character.
// Note that a write operation should be performed when the transmit holding register empty flag is set.
#define LSR_EMPTY_FLAG (1<<5)  // EMPTY FLAG for THR

extern volatile int panicked;  // from printf.c

void uartinit(void) {
    // disable interrupts.
    WriteReg(IER, 0x00);

    // special mode to set baud rate.
    WriteReg(LCR, LCR_BAUD_LATCH);

    // LSB for baud rate of 38.4K.
    WriteReg(0, 0x03);

    // MSB for baud rate of 38.4K.
    WriteReg(1, 0x00);

    // leave set-baud mode,
    // and set word length to 8 bits, no parity.
    WriteReg(LCR, LCR_EIGHT_BITS);

    // reset and enable FIFOs.
    WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

    // enable transmit and receive interrupts.
    WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);
}

void uart_putc_sync(char c) {
    push_off();

    while(panicked);

    while ((ReadReg(LSR) & LSR_EMPTY_FLAG) == 0);

    WriteReg(THR, c);

    pop_off();
}

void uart_puts(char* s) {
    while (*s != '\0') {
        uart_putc_sync(*s);
        s++;
    }
}

int uart_getc(void) {
    if (ReadReg(LSR) & LSR_RX_READY) {
        return ReadReg(RHR);
    } else {
        return -1;
    }
}

void uart_intr(void) {
    ReadReg(ISR);

    while(1){
        int c = uart_getc();
        if (c == -1) {
            break;
        }
        printf("%c", c);
    }
}