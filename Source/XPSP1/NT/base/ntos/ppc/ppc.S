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
#         (or via gcc driver with -m32 -mcpu=750 -mno-altivec)
#
#  HalpVectorEntry is invoked from the exception vector page at 0x000
#  (built by ntos_entry.S) with:
#    r3 = VectorNumber
#    SRR0 = resume PC, SRR1 = MSR at exception time (set by hardware)
#  It builds a KTRAP_FRAME on the stack, calls HalpInterruptEntry(Vector,
#  TrapFrame), then restores all registers and executes rfi.

        .text

#----------------------------------------------------------------------------
# VOID HalpVectorEntry(
#     IN ULONG VectorNumber   # r3 = exception/vector number
# );
#
# Low-level exception vector wrapper. Saves all registers into a
# KTRAP_FRAME on the current stack, then calls HalpInterruptEntry(Vector,
# TrapFrame). After HalpInterruptEntry returns, restores all registers
# and executes rfi to resume the interrupted context.
#
# KTRAP_FRAME layout (from ppctx.h, 0xc4 bytes):
#   0x00 DbgEbp  0x04 DbgEip  0x08 DbgArgMark  0x0c DbgArgPointer
#   0x10 TempSegCs  0x14 TempEsp (SP at trap time)
#   0x18 Gpr0   0x1c Gpr3   0x20 Gpr4   0x24 Gpr5   0x28 Gpr6   0x2c Gpr7
#   0x30 Gpr8   0x34 Gpr9   0x38 Gpr10  0x3c Gpr11  0x40 Gpr12
#   0x44 PreviousPreviousMode  0x48 ExceptionList  0x4c SegFs (PCR ptr)
#   0x50 Gpr14  0x54 Gpr15  0x58 Gpr16  0x5c Gpr17  0x60 Gpr18  0x64 Gpr19
#   0x68 Gpr20  0x6c Gpr21  0x70 Gpr22  0x74 Gpr23  0x78 Gpr24  0x7c Gpr25
#   0x80 Gpr26  0x84 Gpr27  0x88 Gpr28  0x8c Gpr29  0x90 Gpr30  0x94 Gpr31
#   0x98 ErrCode  0x9c Iar (SRR0/resume PC)  0xa0 Msr
#   0xa4 Lr  0xa8 Cr  0xac Xer  0xb0 Ctr  0xb4 Srr0  0xb8 Srr1
#   0xbc TrapNumber  0xc0 PreviousMode
#----------------------------------------------------------------------------
        .globl  HalpVectorEntry
        .type   HalpVectorEntry, @function
HalpVectorEntry:
        # r3 = VectorNumber (set by vector page), r1 = interrupted SP.
        # Allocate KTRAP_FRAME on the stack (0xc4 bytes).
        # stwu stores the old SP as a back chain at the new frame base.
        stwu    1, -0xc4(1)

        # Save volatile GPRs r0, r3-r12 (r1 = SP is saved at TempEsp).
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

        # Save non-volatile GPRs r14-r31.
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

        # Save SP at TempEsp (offset 0x14) = old interrupted SP (back chain).
        lwz     0, 0(1)           # r0 = old SP (stored by stwu as back chain)
        stw     0, 0x14(1)        # TempEsp = old interrupted SP

        # Clear debug fields (0x00-0x10).  r0 is 0 here.
        li      0, 0
        stw     0, 0x00(1)        # DbgEbp
        stw     0, 0x04(1)        # DbgEip
        stw     0, 0x08(1)        # DbgArgMark
        stw     0, 0x0c(1)        # DbgArgPointer
        stw     0, 0x10(1)        # TempSegCs

        # PreviousPreviousMode = 0, ExceptionList = NULL, SegFs = PCR (r2).
        stw     0, 0x44(1)        # PreviousPreviousMode = 0
        stw     0, 0x48(1)        # ExceptionList = NULL
        stw     2, 0x4c(1)        # SegFs = PCR pointer (r2)

        # Save control registers (r0 is scratch again).
        mflr    0
        stw     0, 0xa4(1)        # Lr
        mfcr    0
        stw     0, 0xa8(1)        # Cr
        mfxer   0
        stw     0, 0xac(1)        # Xer
        mfctr   0
        stw     0, 0xb0(1)        # Ctr
        mfspr   0, 0              # r0 = SRR0 (resume PC)
        stw     0, 0xb4(1)        # Srr0
        stw     0, 0x9c(1)        # Iar = SRR0 (resume PC for rfi)
        mfspr   0, 1              # r0 = SRR1 (MSR at exception)
        stw     0, 0xb8(1)        # Srr1
        mfmsr   0
        stw     0, 0xa0(1)        # Msr

        # Trap number already in r3; store at TrapNumber (0xbc).
        stw     3, 0xbc(1)        # TrapNumber = VectorNumber

        # PreviousMode = KernelMode (0).  r0 is still 0.
        stw     0, 0xc0(1)        # PreviousMode = 0

        # r3 = VectorNumber (first arg), r4 = trap frame pointer (second arg).
        mr      4, 1              # r4 = &KTRAP_FRAME
        bl      HalpInterruptEntry

        #----------------------------------------------------------------------------
        # Restore context after HalpInterruptEntry returns.
        # r1 still points to the KTRAP_FRAME (callee restores its own SP).
        #----------------------------------------------------------------------------

        # Restore non-volatile GPRs r14-r31.
        lwz     14, 0x50(1)
        lwz     15, 0x54(1)
        lwz     16, 0x58(1)
        lwz     17, 0x5c(1)
        lwz     18, 0x60(1)
        lwz     19, 0x64(1)
        lwz     20, 0x68(1)
        lwz     21, 0x6c(1)
        lwz     22, 0x70(1)
        lwz     23, 0x74(1)
        lwz     24, 0x78(1)
        lwz     25, 0x7c(1)
        lwz     26, 0x80(1)
        lwz     27, 0x84(1)
        lwz     28, 0x88(1)
        lwz     29, 0x8c(1)
        lwz     30, 0x90(1)
        lwz     31, 0x94(1)

        # Restore volatile GPRs r0, r3-r12 (r1 = SP restored last).
        lwz     0,  0x18(1)       # Gpr0
        lwz     3,  0x1c(1)       # Gpr3
        lwz     4,  0x20(1)       # Gpr4
        lwz     5,  0x24(1)       # Gpr5
        lwz     6,  0x28(1)       # Gpr6
        lwz     7,  0x2c(1)       # Gpr7
        lwz     8,  0x30(1)       # Gpr8
        lwz     9,  0x34(1)       # Gpr9
        lwz     10, 0x38(1)       # Gpr10
        lwz     11, 0x3c(1)       # Gpr11
        lwz     12, 0x40(1)       # Gpr12

        # Restore SRR0 (resume PC) and SRR1 (MSR) first — rfi needs these.
        lwz     0, 0x9c(1)        # r0 = Iar (SRR0 / resume PC)
        mtspr   0, 0              # SRR0 = Iar
        lwz     0, 0xb8(1)        # r0 = Srr1 (MSR to resume with)
        mtspr   1, 0              # SRR1 = MSR

        # Restore remaining control registers (informational; MSR is restored
        # by rfi from SRR1).
        lwz     0, 0xa4(1)        # Lr
        mtlr    0
        lwz     0, 0xa8(1)        # Cr
        mtcr    0
        lwz     0, 0xac(1)        # Xer
        mtxer   0
        lwz     0, 0xb0(1)        # Ctr
        mtctr   0

        # Restore SP r1 from TempEsp (old interrupted SP).
        lwz     1, 0x14(1)        # r1 = old interrupted SP

        # Return from interrupt: restores MSR from SRR1, branches to SRR0.
        rfi

        .size   HalpVectorEntry, .-HalpVectorEntry
