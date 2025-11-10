#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

// -----------------------------------------------------------------------------
// FPU Support: Floating-Point Status bits in mstatus
// -----------------------------------------------------------------------------
#define MSTATUS_FS         (3UL << 13)   // FS field mask (bits 13-14)
#define MSTATUS_FS_OFF     (0UL << 13)   // 00 = Off
#define MSTATUS_FS_INITIAL (1UL << 13)   // 01 = Initial (Clean)
#define MSTATUS_FS_CLEAN   (2UL << 13)   // 10 = Clean
#define MSTATUS_FS_DIRTY   (3UL << 13)   // 11 = Dirty

void main();
void timerinit();

// one stack per CPU for entry.S
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// -----------------------------------------------------------------------------
// start()
// Runs in machine mode on entry stack (stack0). Sets up environment,
// enables FPU, and then switches to supervisor mode via mret.
// -----------------------------------------------------------------------------
void
start(void)
{
  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE);

  // ---------------------------------------------------------------------------
  // Enable FPU in Machine Mode
  // ---------------------------------------------------------------------------
  x = r_mstatus();
  x &= ~MSTATUS_FS;            // clear FS bits
  x |= MSTATUS_FS_INITIAL;     // set FS = Initial (01)
  w_mstatus(x);

  // Clear floating-point control/status register (fcsr)
  // sets exception flags = 0, rounding mode = default
  asm volatile("csrw fcsr, x0");

  // ---------------------------------------------------------------------------
  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  // ---------------------------------------------------------------------------
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}

// -----------------------------------------------------------------------------
// ask each hart to generate timer interrupts.
// -----------------------------------------------------------------------------
void
timerinit(void)
{
  // enable supervisor-mode timer interrupts.
  w_mie(r_mie() | MIE_STIE);

  // enable the sstc extension (stimecmp).
  w_menvcfg(r_menvcfg() | (1L << 63));

  // allow supervisor to use stimecmp and time.
  w_mcounteren(r_mcounteren() | 2);

  // schedule the first timer interrupt.
  w_stimecmp(r_time() + 1000000);
}
