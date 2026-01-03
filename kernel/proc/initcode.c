#include "syscall/sys.h"
#include "fs/fcntl.h"

int main()
{
    if(syscall(SYS_open, "console", O_RDWR) < 0) {
        syscall(SYS_mknod, "console", 1, 0);
        syscall(SYS_open, "console", O_RDWR);
    }
    syscall(SYS_dup, 0);
    syscall(SYS_dup, 0);

    char path[] = "./test";
    char* argv[] = {"hello", "world", 0};

    int pid = syscall(SYS_fork);
    if(pid < 0) { // 失败
        syscall(SYS_write, 0, 20, "initcode: fork fail\n");
    } else if(pid == 0) { // 子进程
        syscall(SYS_write, 0, 22, "\n-----test start-----\n");
        syscall(SYS_exec, path, argv);
    } else { // 父进程
        syscall(SYS_wait, 0);
        syscall(SYS_write, 0, 21, "\n-----test over-----\n");
        while(1);
    }
    return 0;
}
