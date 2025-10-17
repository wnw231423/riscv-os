#include "types.h"
#include "defs.h"
#include "riscv.h"
#include "dev/timer.h"

#define INTERVAL 1000000

timer_t timer;

// ask each hart to generate timer interrupts.
void
timerinit()
{
  // enable supervisor-mode timer interrupts.
  w_mie(r_mie() | MIE_STIE);
  
  // enable the sstc extension (i.e. stimecmp).
  w_menvcfg(r_menvcfg() | (1L << 63)); 
  
  // allow supervisor to use stimecmp and time.
  w_mcounteren(r_mcounteren() | 2);
  
  // ask for the very first timer interrupt.
  w_stimecmp(r_time() + 1000000);
}

// create a timer for each hart
void timer_create() {
    timer.ticks = 0;
    initlock(&timer.lk, "time");
}

void timer_update() {
    acquire(&timer.lk);
    timer.ticks++;
    release(&timer.lk);
}

uint64 timer_get_ticks() {
    return timer.ticks;
}