#  ntos_entry.S - PowerPC kernel entry stub
#
#  Wires ntoskrnl.c -> KeInitializeKernel -> KeInitializeKernelPpc:
#    1. Provides the exception vector page at 0x000 (classic PPC exception
#       vectors) which branches to HalpVectorEntry for each vector.
#    2. Reserves PCR storage in memory and loads its address into GPR r2
#       (the PowerPC TOC/global-pointer convention used by this port).
#    3. Accepts the loader block (PHW_DESCRIPTION from entii's arcldr) in GPR r3
#       and passes it to KeInitializeKernel().
#    4. Calls KeInitializeKernel() (arch-independent entry in ntoskrnl.c)
#       which performs phase setup and delegates to KeInitializeKernelPpc().
#    5. Idles with external interrupts enabled, waiting for the Decrementer
#       tick (vector 0x900) which is dispatched via HalpVectorEntry and
#       returns via rfi.
#
#  Called via entii's ModeSwitchEntry: r3 = PHW_DESCRIPTION (K0 virtual addr).
#
#  Build: powerpc64le-linux-gnu-as -m32 -o ntos_entry.o ntos_entry.S
#         (or via gcc driver with -m32 -mcpu=750 -mno-altivec)

        .text

        .globl  _start
        .type   _start, @function

        .globl  HalpVectorEntry
        .globl  KeInitializeKernel
        .globl  KeInitializeKernelPpc
        .globl  HalpPcrStorage      # defined in pcr.c
        .globl  KeLoaderBlock       # loader block (PHW_DESCRIPTION) passed by entii arcldr

        .text

#----------------------------------------------------------------------------
# Exception vector page (0x000 - 0xFFF).  On classic PowerPC/G3 exceptions
# vector to fixed addresses starting at 0x000.  Each entry is a branch
# instruction to HalpVectorEntry with the vector number in r3.
#
# Vector offsets (from ppctx.h):
#   0x000 Reset             0x100 Machine Check     0x200 (unused)
#   0x300 Data Storage      0x400 Instruction Storage 0x500 External
#   0x600 Alignment         0x700 Program           0x800 FP Unavailable
#   0x900 Decrementer       0xA00 (unused)          0xB00 (unused)
#   0xC00 System Call       0xD00 Trace             0xE00 FP Assist
#----------------------------------------------------------------------------
        .section .vectors, "ax", @progbits
        .align 2
_vectors:
# Vector 0x000 - Reset
        .org 0x000
        li      3, 0x000
        b       HalpVectorEntry
# Vector 0x100 - Machine Check
        .org 0x100
        li      3, 0x100
        b       HalpVectorEntry
# Vector 0x200 - (unused)
        .org 0x200
        li      3, 0x200
        b       HalpVectorEntry
# Vector 0x300 - Data Storage
        .org 0x300
        li      3, 0x300
        b       HalpVectorEntry
# Vector 0x400 - Instruction Storage
        .org 0x400
        li      3, 0x400
        b       HalpVectorEntry
# Vector 0x500 - External Interrupt
        .org 0x500
        li      3, 0x500
        b       HalpVectorEntry
# Vector 0x600 - Alignment
        .org 0x600
        li      3, 0x600
        b       HalpVectorEntry
# Vector 0x700 - Program
        .org 0x700
        li      3, 0x700
        b       HalpVectorEntry
# Vector 0x800 - FP Unavailable
        .org 0x800
        li      3, 0x800
        b       HalpVectorEntry
# Vector 0x900 - Decrementer
        .org 0x900
        li      3, 0x900
        b       HalpVectorEntry
# Vector 0xA00 - (unused)
        .org 0xa00
        li      3, 0xa00
        b       HalpVectorEntry
# Vector 0xB00 - (unused)
        .org 0xb00
        li      3, 0xb00
        b       HalpVectorEntry
# Vector 0xC00 - System Call
        .org 0xc00
        li      3, 0xc00
        b       HalpVectorEntry
# Vector 0xD00 - Trace
        .org 0xd00
        li      3, 0xd00
        b       HalpVectorEntry
# Vector 0xE00 - FP Assist
        .org 0xe00
        li      3, 0xe00
        b       HalpVectorEntry

        .previous
        .text

#----------------------------------------------------------------------------
# Loader block storage.  The PHW_DESCRIPTION pointer (entii arcldr's
# PHYSADDR_LOAD / K0 virtual address) arrives in GPR r3 from ModeSwitchEntry.
# We save it here so KeInitializeKernel can retrieve it and pass it to
# KeInitializeKernelPpc -> HAL init (clock frequency, memory map).
#----------------------------------------------------------------------------
        .section .data
        .align 2
KeLoaderBlock:
        .word   0                   # filled at runtime by _start
        .previous
        .text

#----------------------------------------------------------------------------
# _start - kernel entry point (reset vector target after boot).
#
#   r3 = PHW_DESCRIPTION loader block (from entii ModeSwitchEntry, K0 virtual)
#   r2 = address of PCR storage (HalpPcrStorage)
#   call KeInitializeKernel(r3)   # arch-independent entry (ntoskrnl.c)
#   idle with interrupts enabled, waiting for the Decrementer tick (rfi return)
#----------------------------------------------------------------------------
_start:
        # Save the loader block pointer (r3) into KeLoaderBlock.
        lis     4, KeLoaderBlock@ha
        addi    4, 4, KeLoaderBlock@l
        stw     3, 0(4)           # *KeLoaderBlock = r3 (loader block pointer)

        # Load PCR address into GPR r2 (TOC/global pointer convention)
        lis     3, HalpPcrStorage@ha
        addi    3, 3, HalpPcrStorage@l
        mr      2, 3              # r2 = PCR pointer

        # Reload the saved loader block pointer into r3 (first arg).
        lwz     3, 0(4)           # r3 = *KeLoaderBlock = loader block pointer

        # Call arch-independent kernel initialization (ntoskrnl.c)
        bl      KeInitializeKernel

        # Idle loop with external interrupts enabled.
        # KeInitializeKernelPpc (HalpInstallVectors / HalpInitializeDecrementer)
        # already set MSR.EE and programmed the Decrementer (vector 0x900).
        # Spin here; each Decrementer tick is dispatched via
        # HalpVectorEntry -> HalpInterruptEntry -> KiExceptionDispatch and
        # returns to this loop via rfi.  A real port would schedule threads
        # here instead of spinning.
1:      # Re-enable external interrupts defensively (MSR.EE = 0x8000).
        ori     0, 0, 0x8000      # MSR.EE
        mtmsr   0
        isync
        nop                      # wait for interrupt (decrementer tick)
        b       1b

        .size   _start, .- _start

        .section .rodata
        .align 2
_start_entry:
        .word   _start

        .text
