# NTDLL PowerPC (Broadway/G3) Porting Notes
#
# Windows XP (NT 5.1) -> PowerPC G3 port
# Directory: base/ntdll/ppc/
#
# This document maps each i386 arch-specific file under
# base/ntdll/i386/ to its PowerPC counterpart (done / todo), and
# lists G3-specific considerations.

# ================================================================
# G3 / Broadway specific notes
# ================================================================
# - CPU: PowerPC 750-class (Broadway is a 750cx/750fx derivative).
# - No Altivec/VMX: compile with -mno-altivec.
# - Big-endian by default (G3 can switch; NT PPC ran big-endian).
# - No x87 FPU: i386 emulator/*.asm (x87 FPU emulation) is not needed
#   on PowerPC (it has IEEE-754 FP in hardware). Marked N/A below.
# - No segment registers (CS/DS/ES/SS/FS/GS): i386 npxnp.c / ntnapntr.asm
#   x87/segment handling maps to no-op stubs.
# - No EFLAGS: condition codes live in CR/XER; i386 EFLAGS-dependent
#   code has no direct equivalent.
# - Atomic ops: use lwarx/stwcx load-reserve/store-conditional
#   (see base/hals/halppc/ppc/atomic.c).
# - Registers: GPRs r0-r31, SP=r1, LR, CR, XER, CTR, MSR, SRR0/SRR1.
# - Assembler note: powerpc-linux-gnu-as requires bare register numbers
#   (0, 1, 31), NOT r0/r1/r31. Use .machine ppc.
#
# ================================================================
# i386 -> ppc file map
# ================================================================
#
# Legend:
#   [DONE]  PPC equivalent exists in base/ntdll/ppc/
#   [TODO]  No PPC equivalent yet
#   [N/A]   Not applicable to PowerPC (x86-only feature)

# --- Critical sections ---
critsect.asm   -> critsect.c    [DONE]  RtlEnter/Leave/TryEnterCriticalSection
                                        implemented with real PowerPC lwarx/stwcx.
                                        atomics (not __sync_* wrappers).

# --- x87 FPU emulator (i386 only, N/A on PowerPC) ---
emarith.asm            [N/A]  x87 FPU emulator - not needed (PPC has HW FPU)
emdecode.asm           [N/A]  x87 instruction decode - N/A
emdisp.asm             [N/A]  x87 dispatch - N/A
emerror.asm            [N/A]  x87 error handling - N/A
emf386.asm             [N/A]  x87 387-specific - N/A
emfadd.asm             [N/A]  x87 FADD - N/A
emfcom.asm             [N/A]  x87 FCOM - N/A
emfconst.asm           [N/A]  x87 constants - N/A
emfdiv.asm             [N/A]  x87 FDIV - N/A
emfinit.asm            [N/A]  x87 FINIT - N/A
emfmisc.asm            [N/A]  x87 misc - N/A
emfmul.asm             [N/A]  x87 FMUL - N/A
emfprem.asm            [N/A]  x87 FPREM - N/A
emfsqrt.asm            [N/A]  x87 FSQRT - N/A
emftran.asm            [N/A]  x87 transcendental - N/A
emload.asm             [N/A]  x87 load - N/A
emlsbcd.asm            [N/A]  x87 BCD - N/A
emlsenv.asm            [N/A]  x87 env - N/A
emround.asm            [N/A]  x87 round - N/A
emstack.inc            [N/A]  x87 stack include - N/A
emstore.asm            [N/A]  x87 store - N/A
emtrig.asm             [N/A]  x87 trig - N/A
emulator.asm           [N/A]  x87 emulator entry - N/A
emxtract.asm           [N/A]  x87 extract - N/A

# --- Loader context / thunk ---
ldrctx.c    -> ldrctx.c    [DONE]  LdrpRelocateStartContext for PPC
                                        (relocates IntA0/r3 on PPC vs Eax on i386)
ldrthunk.asm -> ldrthunk.c  [DONE]  LdrInitializeThunk for PPC
                                        (forwards to LdrpInitialize; PPC has no
                                        fixed stack-layout context record walk)

# --- NPX / x87 exception handling ---
npxnp.c        -> npxnp.c    [DONE]  NpxNpReadCSEip / NpxNpSkipInstruction
                                        stubs (no x87 on PPC; no-op/placeholder)
nt386npx.inc   -> (none)     [N/A]  i386 NPX include - not applicable
npxemltr.def   -> (none)     [N/A]  x87 emulator linker def - not applicable

# --- i386-specific assembly ---
ntnapntr.asm   -> (none)     [TODO]  i386 NATIVE AP (apic?) - no PPC equivalent yet

# --- New PPC-only files (no i386 counterpart) ---
context.h      [NEW]  PPC KTRAP_FRAME + CONTEXT struct definitions
context.c      [NEW]  RtlCaptureContext / RtlRestoreContext for PPC
dispatch.c     [NEW]  RtlDispatchException / RtlVirtualUnwind /
                                        RtlLookupFunctionEntry / RtlRaiseException
                                        PPC stubs

# ================================================================
# Files implemented in base/ntdll/ppc/ (DONE)
# ================================================================
#   critsect.c   - critical sections (DONE)
#   ldrctx.c     - loader context relocation (DONE)
#   ldrthunk.c   - loader initialization thunk (DONE)
#   npxnp.c      - NPX/x87 fault stubs (DONE)
#   context.h    - PPC trap frame + CONTEXT (NEW, Phase 3)
#   context.c    - context capture/restore (NEW, Phase 3)
#   dispatch.c   - exception dispatch (NEW, Phase 3)

# ================================================================
# Still TODO
# ================================================================
# - ntnapntr.asm equivalent (investigate purpose first)
# - Full RtlVirtualUnwind unwind-table support
# - SEH frame-based handler registration
# - Integration with base/hals/halppc (trap frame fill-in from vectors)
