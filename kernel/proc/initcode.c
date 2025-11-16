#include "syscall/sys.h"
int main()
{
    long long heap_top = syscall(SYS_sbrk, 0);

    heap_top = syscall(SYS_sbrk, heap_top + 4096 * 10);

    heap_top = syscall(SYS_sbrk, heap_top - 4096 * 5);

    while(1);
    return 0;
}
