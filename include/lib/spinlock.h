#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "types.h"

typedef struct cpu cpu_t;

// Mutual exclusion lock.
typedef struct spinlock {
  uint locked; // Is the lock held?

  // For debugging:
  char *name;      // Name of lock.
  cpu_t *cpu; // The cpu holding the lock.
} spinlock_t;

#endif
