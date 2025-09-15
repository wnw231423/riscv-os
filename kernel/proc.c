#include "types.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"

struct cpu cpus[NCPU];

int cpuid() {
    int id = r_tp();
    return id;
}

struct cpu* mycpu() {
    int id = cpuid();
    struct cpu *c = &cpus[id];
    return c;
}