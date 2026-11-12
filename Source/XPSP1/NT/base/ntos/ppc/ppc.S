#  ppc.S - PowerPC interrupt vector entry
#
#  PowerPC HAL/kernel primitive for Windows XP (NT 5.1) port.
#
#  Implements HalpVectorEntry (low-level exception vector wrapper that
#  builds a KTRAP_FRAME). HalpSwapContext lives in the HAL
#  (base/hals/halppc/ppc/context.S) to avoid a duplicate symbol when
#  HAL and kernel are linked into a single image.
#
#  Build: powerpc64le-linux-gnu-as -m32 -o ppc.o ppc.S
#

        .text

#----------------------------------------------------------------------------
# VOID HalpVectorEntry(
#     IN ULONG VectorNumber   # r3 = exception/vector number
# );
#
# Low-level exception vector wrapper. Saves all registers into a
# KTRAP_FRAME on the current stack, then calls HalpInterruptEntry(Vector,
# TrapFrame). The KTRAP_FRAME layout matches ppctx.h.
#
# NOTE: bare register numbers used (not r0/r1 names) per Phase 2 assembler fix.
#----------------------------------------------------------------------------
        .globl  HalpVectorEntry
        .type   HalpVectorEntry, @function
HalpVectorEntry:
        # Allocate KTRAP_FRAME on the stack (0xc4 bytes)
        stwu    1, -0xc4(1)

        # Save volatile GPRs r0, r3-r12
        stw     0,  0x18(1)       # Gpr0
        stw     3,  0x1c(1)       # Gpr3
        stw     4,  0x20(1)       # Gpr4
        stw     5,  0x24(1)       # Gpr5
        stw     6,  0x28(1)       # Gpr6
        stw     7,  0x2c(1)       # Gpr7
        stw     8,  0x30(1)       # Gpr8
        stw     9,  0x34(1)       # Gpr9
        stw     10, 0x38(1)       # Gpr10
        stw     11, 0x3c(1)       # Gpr11
        stw     12, 0x40(1)       # Gpr12

        # Save non-volatile GPRs r14-r31
        stw     14, 0x50(1)
        stw     15, 0x54(1)
        stw     16, 0x58(1)
        stw     17, 0x5c(1)
        stw     18, 0x60(1)
        stw     19, 0x64(1)
        stw     20, 0x68(1)
        stw     21, 0x6c(1)
        stw     22, 0x70(1)
        stw     23, 0x74(1)
        stw     24, 0x78(1)
        stw     25, 0x7c(1)
        stw     26, 0x80(1)
        stw     27, 0x84(1)
        stw     28, 0x88(1)
        stw     29, 0x8c(1)
        stw     30, 0x90(1)
        stw     31, 0x94(1)

        # Save SP at TempEsp field (offset 0x14)
        stw     1,  0x14(1)

        # Save control registers
        mflr    0
        stw     0,  0xa4(1)       # Lr
        mfcr    0
        stw     0,  0xa8(1)       # Cr
        mfxer   0
        stw     0,  0xac(1)       # Xer
        mfctr   0
        stw     0,  0xb0(1)       # Ctr
        mfspr   0, 0
        stw     0,  0xb4(1)       # Srr0
        mfspr   0, 1
        stw     0,  0xb8(1)       # Srr1
        mfmsr   0
        stw     0,  0xa0(1)       # Msr

        # Trap number already in r3; store it at TrapNumber (offset 0xbc)
        stw     3,  0xbc(1)

        # PreviousMode = KernelMode (0)
        li      0, 0
        stw     0,  0xc0(1)

        # PreviousPreviousMode = 0, ExceptionList = NULL
        li      0, 0
        stw     0,  0x44(1)       # PreviousPreviousMode
        stw     0,  0x48(1)       # ExceptionList

        # r3 = VectorNumber (first arg), r4 = trap frame pointer (second arg)
        mr      4, 1
        bl      HalpInterruptEntry

        # Halt/loop (rfi + restore would go here in a full port)
        b       .

        .size   HalpVectorEntry, .-HalpVectorEntry
