#ifndef __ASSEMBLER__

// which hart (core) is this?
static inline uint64
r_mhartid()
{
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r" (x) );
  return x;
}

// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_FS         (3UL << 13)   // FS field mask (bits 13–14)
#define MSTATUS_FS_OFF     (0UL << 13)   // 00 = Off
#define MSTATUS_FS_INITIAL (1UL << 13)   // 01 = Initial (Clean)
#define MSTATUS_FS_CLEAN   (2UL << 13)   // 10 = Clean
#define MSTATUS_FS_DIRTY   (3UL << 13)   // 11 = Dirty

static inline uint64
r_fcsr()
{
  uint64 x;
  asm volatile("csrr %0, fcsr" : "=r" (x) );
  return x;
}

// Write FCSR
static inline void 
w_fcsr(uint64 x)
{
  asm volatile("csrw fcsr, %0" : : "r" (x));
}

// ============================================================================
// FCSR Exception Flags (bits 0-4)
// ============================================================================
#define FCSR_NX (1L << 0)  // Inexact
#define FCSR_UF (1L << 1)  // Underflow
#define FCSR_OF (1L << 2)  // Overflow
#define FCSR_DZ (1L << 3)  // Divide by Zero
#define FCSR_NV (1L << 4)  // Invalid Operation

// All exception flags mask
#define FCSR_EXCEPTION_MASK 0x1F  // bits 0-4

// Rounding modes (bits 5-7)
#define FCSR_RM_MASK (7L << 5)
#define FCSR_RM_RNE  (0L << 5)  // Round to Nearest, ties to Even
#define FCSR_RM_RTZ  (1L << 5)  // Round towards Zero
#define FCSR_RM_RDN  (2L << 5)  // Round Down (towards -∞)
#define FCSR_RM_RUP  (3L << 5)  // Round Up (towards +∞)
#define FCSR_RM_RMM  (4L << 5)  // Round to Nearest, ties to Max Magnitude

// ============================================================================
// SSTATUS FPU Field Definitions (for lazy FPU)
// ============================================================================
#define SSTATUS_FS         (3L << 13)   // FS field mask (bits 13-14)
#define SSTATUS_FS_OFF     (0L << 13)   // 00 = Off
#define SSTATUS_FS_INITIAL (1L << 13)   // 01 = Initial (Clean)
#define SSTATUS_FS_CLEAN   (2L << 13)   // 10 = Clean
#define SSTATUS_FS_DIRTY   (3L << 13)   // 11 = Dirty

// ============================================================================
// RISC-V Exception/Trap Cause Codes
// ============================================================================
#define CAUSE_INSTRUCTION_MISALIGNED  0
#define CAUSE_INSTRUCTION_ACCESS_FAULT 1
#define CAUSE_ILLEGAL_INSTRUCTION     2
#define CAUSE_BREAKPOINT              3
#define CAUSE_LOAD_MISALIGNED         4
#define CAUSE_LOAD_ACCESS_FAULT       5
#define CAUSE_STORE_MISALIGNED        6
#define CAUSE_STORE_ACCESS_FAULT      7
#define CAUSE_USER_ECALL              8
#define CAUSE_SUPERVISOR_ECALL        9
#define CAUSE_INSTRUCTION_PAGE_FAULT  12
#define CAUSE_LOAD_PAGE_FAULT         13
#define CAUSE_STORE_PAGE_FAULT        15

// ============================================================================
// Helper Macros for FPU Exception Handling
// ============================================================================

// Check if FCSR has any exception flags set
#define FCSR_HAS_EXCEPTION(fcsr) (((fcsr) & FCSR_EXCEPTION_MASK) != 0)

// Fatal exception mask (exclude NX because inexact is non-fatal/expected)
#define FCSR_FATAL_MASK (FCSR_NV | FCSR_DZ | FCSR_OF | FCSR_UF)
#define FCSR_HAS_FATAL_EXCEPTION(x) (((x) & FCSR_FATAL_MASK) != 0)

// Check if sstatus has FPU disabled
#define SSTATUS_FPU_DISABLED(sstatus) (((sstatus) & SSTATUS_FS) == SSTATUS_FS_OFF)

// Enable FPU in sstatus (set to Initial state)
#define SSTATUS_ENABLE_FPU(sstatus) ((sstatus & ~SSTATUS_FS) | SSTATUS_FS_INITIAL)

// ============================================================================
// RISC-V Instruction Opcode Masks (Simplified FP Detection)
// ============================================================================

// RISC-V instruction format opcodes
#define OPCODE_MASK       0x7F        // bits 0-6
#define OPCODE_LOAD_FP    0x07        // FLW, FLD (floating-point load)
#define OPCODE_STORE_FP   0x27        // FSW, FSD (floating-point store)
#define OPCODE_FMADD      0x43        // FMADD.S, FMADD.D
#define OPCODE_FMSUB      0x47        // FMSUB.S, FMSUB.D
#define OPCODE_FNMSUB     0x4B        // FNMSUB.S, FNMSUB.D
#define OPCODE_FNMADD     0x4F        // FNMADD.S, FNMADD.D
#define OPCODE_FP_OP      0x53        // FADD, FSUB, FMUL, FDIV, etc.

// Helper macro to check if instruction is FP operation
#define IS_FP_INSTRUCTION(inst) ( \
  ((inst) & OPCODE_MASK) == OPCODE_LOAD_FP   || \
  ((inst) & OPCODE_MASK) == OPCODE_STORE_FP  || \
  ((inst) & OPCODE_MASK) == OPCODE_FMADD     || \
  ((inst) & OPCODE_MASK) == OPCODE_FMSUB     || \
  ((inst) & OPCODE_MASK) == OPCODE_FNMSUB    || \
  ((inst) & OPCODE_MASK) == OPCODE_FNMADD    || \
  ((inst) & OPCODE_MASK) == OPCODE_FP_OP )

static inline uint64
r_mstatus()
{
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r" (x) );
  return x;
}

static inline void 
w_mstatus(uint64 x)
{
  asm volatile("csrw mstatus, %0" : : "r" (x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void 
w_mepc(uint64 x)
{
  asm volatile("csrw mepc, %0" : : "r" (x));
}

// Supervisor Status Register, sstatus

#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

static inline uint64
r_sstatus()
{
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

static inline void 
w_sstatus(uint64 x)
{
  asm volatile("csrw sstatus, %0" : : "r" (x));
}

// Supervisor Interrupt Pending
static inline uint64
r_sip()
{
  uint64 x;
  asm volatile("csrr %0, sip" : "=r" (x) );
  return x;
}

static inline void 
w_sip(uint64 x)
{
  asm volatile("csrw sip, %0" : : "r" (x));
}

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
static inline uint64
r_sie()
{
  uint64 x;
  asm volatile("csrr %0, sie" : "=r" (x) );
  return x;
}

static inline void 
w_sie(uint64 x)
{
  asm volatile("csrw sie, %0" : : "r" (x));
}

// Machine-mode Interrupt Enable
#define MIE_STIE (1L << 5)  // supervisor timer
static inline uint64
r_mie()
{
  uint64 x;
  asm volatile("csrr %0, mie" : "=r" (x) );
  return x;
}

static inline void 
w_mie(uint64 x)
{
  asm volatile("csrw mie, %0" : : "r" (x));
}

// supervisor exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void 
w_sepc(uint64 x)
{
  asm volatile("csrw sepc, %0" : : "r" (x));
}

static inline uint64
r_sepc()
{
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}

// Write supervisor counter enable register
static inline void
w_scounteren(uint64 x)
{
  asm volatile("csrw scounteren, %0" : : "r" (x));
}

// Machine Exception Delegation
static inline uint64
r_medeleg()
{
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r" (x) );
  return x;
}

static inline void 
w_medeleg(uint64 x)
{
  asm volatile("csrw medeleg, %0" : : "r" (x));
}

// Machine Interrupt Delegation
static inline uint64
r_mideleg()
{
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r" (x) );
  return x;
}

static inline void 
w_mideleg(uint64 x)
{
  asm volatile("csrw mideleg, %0" : : "r" (x));
}

// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void 
w_stvec(uint64 x)
{
  asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline uint64
r_stvec()
{
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}

// Supervisor Timer Comparison Register
static inline uint64
r_stimecmp()
{
  uint64 x;
  // asm volatile("csrr %0, stimecmp" : "=r" (x) );
  asm volatile("csrr %0, 0x14d" : "=r" (x) );
  return x;
}

static inline void 
w_stimecmp(uint64 x)
{
  // asm volatile("csrw stimecmp, %0" : : "r" (x));
  asm volatile("csrw 0x14d, %0" : : "r" (x));
}

// Machine Environment Configuration Register
static inline uint64
r_menvcfg()
{
  uint64 x;
  // asm volatile("csrr %0, menvcfg" : "=r" (x) );
  asm volatile("csrr %0, 0x30a" : "=r" (x) );
  return x;
}

static inline void 
w_menvcfg(uint64 x)
{
  // asm volatile("csrw menvcfg, %0" : : "r" (x));
  asm volatile("csrw 0x30a, %0" : : "r" (x));
}

// Physical Memory Protection
static inline void
w_pmpcfg0(uint64 x)
{
  asm volatile("csrw pmpcfg0, %0" : : "r" (x));
}

static inline void
w_pmpaddr0(uint64 x)
{
  asm volatile("csrw pmpaddr0, %0" : : "r" (x));
}

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// supervisor address translation and protection;
// holds the address of the page table.
static inline void 
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64
r_satp()
{
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x) );
  return x;
}

// Supervisor Trap Cause
static inline uint64
r_scause()
{
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x) );
  return x;
}

// Supervisor Trap Value
static inline uint64
r_stval()
{
  uint64 x;
  asm volatile("csrr %0, stval" : "=r" (x) );
  return x;
}

// Machine-mode Counter-Enable
static inline void 
w_mcounteren(uint64 x)
{
  asm volatile("csrw mcounteren, %0" : : "r" (x));
}

static inline uint64
r_mcounteren()
{
  uint64 x;
  asm volatile("csrr %0, mcounteren" : "=r" (x) );
  return x;
}

// machine-mode cycle counter
static inline uint64
r_time()
{
  uint64 x;
  asm volatile("csrr %0, time" : "=r" (x) );
  return x;
}

// enable device interrupts
static inline void
intr_on()
{
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// disable device interrupts
static inline void
intr_off()
{
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// are device interrupts enabled?
static inline int
intr_get()
{
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

static inline uint64
r_sp()
{
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}

// read and write tp, the thread pointer, which xv6 uses to hold
// this core's hartid (core number), the index into cpus[].
static inline uint64
r_tp()
{
  uint64 x;
  asm volatile("mv %0, tp" : "=r" (x) );
  return x;
}

static inline void 
w_tp(uint64 x)
{
  asm volatile("mv tp, %0" : : "r" (x));
}

static inline uint64
r_ra()
{
  uint64 x;
  asm volatile("mv %0, ra" : "=r" (x) );
  return x;
}

// flush the TLB.
static inline void
sfence_vma()
{
  // the zero, zero means flush all TLB entries.
  asm volatile("sfence.vma zero, zero");
}

// machine-mode cycle counter (you already have this)
static inline uint64
r_cycle()
{
  uint64 x;
  asm volatile("csrr %0, cycle" : "=r" (x));
  return x;
}

// machine-mode instruction counter (you already have this)
static inline uint64
r_instret()
{
  uint64 x;
  asm volatile("csrr %0, instret" : "=r" (x));
  return x;
}


typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs

#endif // __ASSEMBLER__

#define PGSIZE 4096 // bytes per page
#define PGSHIFT 12  // bits of offset within a page

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4) // user can access

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// extract the three 9-bit page table indices from a virtual address.
#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))