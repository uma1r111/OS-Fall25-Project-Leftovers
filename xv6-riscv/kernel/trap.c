// kernel/trap.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

// ----------------------------------------------------------------------------
// Print human-readable FCSR exception flags for debugging
// ----------------------------------------------------------------------------
void
print_fp_exception(struct proc *p, uint64 fcsr)
{
  if (p)
    printf("pid %d %s: floating-point exception(s) detected:\n", p->pid, p->name);
  else
    printf("kernel: floating-point exception(s) detected:\n");

  if (fcsr & FCSR_NV) {
    printf("  - Invalid Operation (NV): operation produced NaN result\n");
    printf("    (e.g., sqrt of negative number, 0/0, inf-inf)\n");
  }
  if (fcsr & FCSR_DZ) {
    printf("  - Divide by Zero (DZ): division by zero\n");
  }
  if (fcsr & FCSR_OF) {
    printf("  - Overflow (OF): result too large to represent\n");
  }
  if (fcsr & FCSR_UF) {
    printf("  - Underflow (UF): result too small (near zero)\n");
  }
  if (fcsr & FCSR_NX) {
    printf("  - Inexact (NX): result was rounded\n");
  }

  printf("  FCSR = 0x%lx\n", (unsigned long)fcsr);
}

// ----------------------------------------------------------------------------
// Safe check: is the 32-bit instruction at virtual address 'va' an FP instr?
// Uses copyin to safely fetch 4 bytes from the user page table.
// Returns 1 if FP instruction, 0 otherwise.
// ----------------------------------------------------------------------------
int
is_fp_instruction_at(pagetable_t pagetable, uint64 va)
{
  uint32 inst;

  if (va >= MAXVA)
    return 0;

  // copyin returns 0 on success; -1 on error.
  if (copyin(pagetable, (char *)&inst, va, sizeof(inst)) < 0)
    return 0;

  return IS_FP_INSTRUCTION(inst);
}

// ----------------------------------------------------------------------------
// Handle illegal-instruction trap for current process:
// If FPU is disabled and the faulting instruction is FP, enable FPU lazily.
// Returns 1 if handled (we enabled FPU and caller should retry), 0 otherwise.
// ----------------------------------------------------------------------------
int
handle_illegal_instruction(struct proc *p)
{
  if (p == 0)
    return 0;

  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();

  // If FPU already enabled, we don't handle it here.
  if (!SSTATUS_FPU_DISABLED(sstatus)) {
    return 0;
  }

  // Check whether the fault instruction is an FP instruction
  int is_fp = is_fp_instruction_at(p->pagetable, sepc);

  if (is_fp) {
    // Lazy enable FPU for this hart/process.
    printf("pid %d %s: lazy FPU enablement at PC=0x%lx\n", p->pid, p->name, sepc);

    // Set FS field to Initial (so FPU instructions will execute)
    uint64 new_sstatus = SSTATUS_ENABLE_FPU(sstatus);
    w_sstatus(new_sstatus);

    // Initialize/clear FCSR for this process (clear flags, leave rounding mode default)
    w_fcsr(0);

    // clear saved fcsr in process context too, so it isn't restored on next swtch
    p->context.fcsr = 0;

    // Handled: return to userland to retry the instruction
    return 1;
  }

  // Not an FP instruction — not handled here
  return 0;
}

// ----------------------------------------------------------------------------
// Check FCSR for exception flags for the given process.
// If exception flags set -> print, clear flags, and kill the process.
// Called on every trap (Option A).
// ----------------------------------------------------------------------------
void
check_fp_exceptions(struct proc *p)
{
  if (p == 0)
    return;

  uint64 sstatus = r_sstatus();

  // Only check if the FPU is enabled for this process.
  if (SSTATUS_FPU_DISABLED(sstatus))
    return;

  uint64 fcsr = r_fcsr();

  // If only NX (inexact) is set, treat as non-fatal: clear NX and update saved context.
  if ((fcsr & ~FCSR_NX) == 0 && (fcsr & FCSR_NX)) {
    uint64 new_fcsr = fcsr & ~FCSR_NX;
    w_fcsr(new_fcsr);
    // Clear saved context fcsr so it won't be restored later.
    p->context.fcsr = new_fcsr;
    return;
  }

  // If any *fatal* exception flags are set, print details and kill process.
  if (FCSR_HAS_FATAL_EXCEPTION(fcsr)) {
    print_fp_exception(p, fcsr);

    // Clear fatal flags (preserve rounding mode) and update saved context.
    uint64 new_fcsr = fcsr & ~FCSR_FATAL_MASK;
    w_fcsr(new_fcsr);
    p->context.fcsr = new_fcsr;

    // Kill the process (safer approach)
    printf("pid %d %s: killed due to floating-point fatal exception\n", p->pid, p->name);
    setkilled(p);
    return;
  }

  // If here, either no flags or only non-fatal bits were present and handled above.
}

//
// handle an interrupt, exception, or system call from user space.
// called from, and returns to, trampoline.S
// return value is user satp for trampoline.S to switch to.
//
uint64
usertrap(void)
{
  int which_dev = 0;

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // save user program counter.
  p->trapframe->epc = r_sepc();

  if (r_scause() == CAUSE_USER_ECALL || r_scause() == 8) {
    // system call

    if (killed(p))
      kexit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if ((which_dev = devintr()) != 0) {
    // device interrupt handled
  } else if ((r_scause() == CAUSE_STORE_PAGE_FAULT || r_scause() == CAUSE_LOAD_PAGE_FAULT ||
               r_scause() == CAUSE_INSTRUCTION_PAGE_FAULT) &&
             vmfault(p->pagetable, r_stval(),
                     (r_scause() == CAUSE_LOAD_PAGE_FAULT) ? 1 : 0) != 0) {
    // page fault on lazily-allocated page handled by vmfault
  }
  // Handle illegal instruction (potential FP trap -> lazy enable)
  else if (r_scause() == CAUSE_ILLEGAL_INSTRUCTION) {
    if (!handle_illegal_instruction(p)) {
      // Not an FP instruction or already enabled
      printf("usertrap(): illegal instruction pid=%d\n", p->pid);
      printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
      setkilled(p);
    }
  } else {
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p ? p->pid : 0);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    setkilled(p);
  }

  // Check for floating-point exceptions on every trap (Option A)
  check_fp_exceptions(p);

  if (killed(p))
    kexit(-1);

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
    yield();

  prepare_return();

  // the user page table to switch to, for trampoline.S
  uint64 satp = MAKE_SATP(p->pagetable);

  // return to trampoline.S; satp value in a0.
  return satp;
}

//
// set up trapframe and control registers for a return to user space
//
void
prepare_return(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(). because a trap from kernel
  // code to usertrap would be a disaster, turn off interrupts.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set the S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  if ((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if ((which_dev = devintr()) == 0) {
    // interrupt or trap from an unknown source
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2 && myproc() != 0)
    yield();

  // restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  if (cpuid() == 0) {
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }

  // schedule next timer interrupt; clears the interrupt request
  w_stimecmp(r_time() + 1000000);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if (scause == 0x8000000000000009L) {
    // supervisor external interrupt, via PLIC.
    int irq = plic_claim();

    if (irq == UART0_IRQ) {
      uartintr();
    } else if (irq == VIRTIO0_IRQ) {
      virtio_disk_intr();
    }
#ifdef LAB_NET
    else if (irq == E1000_IRQ) {
      e1000_intr();
    }
#endif
    else if (irq) {
      printf("unexpected interrupt irq=%d\n", irq);
    }

    if (irq)
      plic_complete(irq);

    return 1;
  } else if (scause == 0x8000000000000005L) {
    // timer interrupt.
    clockintr();
    return 2;
  } else {
    return 0;
  }
}
