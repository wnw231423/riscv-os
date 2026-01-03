#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "lib/spinlock.h"
#include "proc/proc.h"
#include "syscall/syscall_num.h"
#include "defs.h"

// Prototypes for the functions that handle system calls.
// in sysproc.c
extern uint64 sys_print(void);
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
//extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
//extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
//extern uint64 sys_pause(void);
//extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
extern uint64 sys_mmap(void);
extern uint64 sys_munmap(void);

// An array mapping syscall num to the function
static uint64 (*syscalls[])(void) = {
    [SYS_print] sys_print,
    [SYS_fork]    sys_fork,
    [SYS_exit]    sys_exit,
    [SYS_wait]    sys_wait,
    [SYS_pipe]    sys_pipe,
    [SYS_read]    sys_read,
    //[SYS_kill]    sys_kill,
    [SYS_exec]    sys_exec,
    [SYS_fstat]   sys_fstat,
    [SYS_chdir]   sys_chdir,
    [SYS_dup]     sys_dup,
    //[SYS_getpid]  sys_getpid,
    [SYS_sbrk]    sys_sbrk,
    //[SYS_pause]   sys_pause,
    //[SYS_uptime]  sys_uptime,
    [SYS_open]    sys_open,
    [SYS_write]   sys_write,
    [SYS_mknod]   sys_mknod,
    [SYS_unlink]  sys_unlink,
    [SYS_link]    sys_link,
    [SYS_mkdir]   sys_mkdir,
    [SYS_close]   sys_close,
    [SYS_mmap]  sys_mmap,
    [SYS_munmap] sys_munmap,
};

// handle syscall, called in trap_user.c
void syscall(void) {
    int num;
    proc_t *p = myproc();

    num = p->trapframe->a7;
    //if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    if(num >= 0 && syscalls[num]) {
        // Use num to lookup the system call function for num, call it,
        // and store its return value in p->trapframe->a0
        printf("receive number %d syscall.\n", num);
        p->trapframe->a0 = syscalls[num]();
    } else {
        printf("%d: unknown sys call %d\n",
                p->pid, num);
        p->trapframe->a0 = -1;
    }
}

static uint64
argraw(int n)
{
  proc_t *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

void arg_int(int n, int *ip) {
    *ip = argraw(n);
}

// 基于参数寄存器编号的读取
void arg_uint32(int n, uint32* ip) {
    *ip = (uint32)argraw(n);
}

void arg_uint64(int n, uint64* ip) {
    *ip = argraw(n);
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  proc_t *p = myproc();
  if(copyinstr(p->pgtbl, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
}

int arg_str(int n, char* buf, int maxlen) {
    uint64 addr;
    argaddr(n, &addr);
    return fetchstr(addr, buf, maxlen);
}

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  proc_t *p = myproc();
  if(addr >= p->heap_top || addr+sizeof(uint64) > p->heap_top) // both tests needed, in case of overflow
    return -1;
  if(copyin(p->pgtbl, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}
