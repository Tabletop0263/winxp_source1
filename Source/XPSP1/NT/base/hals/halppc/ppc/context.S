.section ".text"
.machine ppc
.globl HalpSwapContext
.type HalpSwapContext, @function
.align 4
HalpSwapContext:
        /* Save outgoing context (3 = save area) */
        mfmsr   0
        stw     0, 0(3)
        mflr    0
        stw     0, 4(3)
        mfcr    0
        stw     0, 8(3)
        mfxer   0
        stw     0, 12(3)
        mfctr   0
        stw     0, 16(3)

        /* Save general purpose registers 0-31 */
        stw     0, 20(3)
        stw     1, 24(3)
        stw     2, 28(3)
        stw     3, 32(3)
        stw     4, 36(3)
        stw     5, 40(3)
        stw     6, 44(3)
        stw     7, 48(3)
        stw     8, 52(3)
        stw     9, 56(3)
        stw     10, 60(3)
        stw     11, 64(3)
        stw     12, 68(3)
        stw     13, 72(3)
        stw     14, 76(3)
        stw     15, 80(3)
        stw     16, 84(3)
        stw     17, 88(3)
        stw     18, 92(3)
        stw     19, 96(3)
        stw     20, 100(3)
        stw     21, 104(3)
        stw     22, 108(3)
        stw     23, 112(3)
        stw     24, 116(3)
        stw     25, 120(3)
        stw     26, 124(3)
        stw     27, 128(3)
        stw     28, 132(3)
        stw     29, 136(3)
        stw     30, 140(3)
        stw     31, 144(3)

        /* Switch stack pointer to incoming thread's stack */
        lwz     0, 24(4)        /* 0 = incoming SP */
        mr      1, 0

        /* Restore incoming context (4 = restore area) */
        lwz     0, 0(4)         /* MSR */
        mtmsr   0
        lwz     0, 4(4)         /* LR */
        mtlr    0
        lwz     0, 8(4)         /* CR */
        mtcr    0
        lwz     0, 12(4)        /* XER */
        mtxer   0
        lwz     0, 16(4)        /* CTR */
        mtctr   0

        /* Restore general purpose registers 0-31 */
        lwz     0, 20(4)
        lwz     1, 24(4)
        lwz     2, 28(4)
        lwz     3, 32(4)
        lwz     4, 36(4)
        lwz     5, 40(4)
        lwz     6, 44(4)
        lwz     7, 48(4)
        lwz     8, 52(4)
        lwz     9, 56(4)
        lwz     10, 60(4)
        lwz     11, 64(4)
        lwz     12, 68(4)
        lwz     13, 72(4)
        lwz     14, 76(4)
        lwz     15, 80(4)
        lwz     16, 84(4)
        lwz     17, 88(4)
        lwz     18, 92(4)
        lwz     19, 96(4)
        lwz     20, 100(4)
        lwz     21, 104(4)
        lwz     22, 108(4)
        lwz     23, 112(4)
        lwz     24, 116(4)
        lwz     25, 120(4)
        lwz     26, 124(4)
        lwz     27, 128(4)
        lwz     28, 132(4)
        lwz     29, 136(4)
        lwz     30, 140(4)
        lwz     31, 144(4)

        /* Return to incoming thread */
        cmplwi  5, 0
        beq     1f
        mtctr   5
        bctr
1:      blr
        .size HalpSwapContext, .-HalpSwapContext
