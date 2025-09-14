#include "defs.h"

void main() {
    char* s = "boot successful.";
    uart_puts(s);
    while (1);
}