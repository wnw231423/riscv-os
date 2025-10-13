#ifndef TIMER_H
#define TIMER_H

#include "lib/spinlock.h"

typedef struct timer {
    uint64 ticks;
    spinlock_t lk;
} timer_t;

#endif