#ifndef PROC_H
#define PROC_H

// Per-CPU state.
struct cpu {
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

#endif