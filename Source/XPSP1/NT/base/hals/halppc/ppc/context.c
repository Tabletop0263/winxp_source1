/*
 *  context.c - PowerPC (Broadway/G3) context switch routine
 *
 *  PowerPC HAL primitive for Windows XP (NT 5.1) port.
 *
 *  KiSwapContext equivalent: saves the outgoing thread's CPU state
 *  (GPRs r0-r31, SP r1, LR, CR, XER, CTR, MSR) and restores the
 *  incoming thread's state, then returns to the incoming thread's
 *  instruction pointer.
 *
 *  Implemented in C with inline assembly so it compiles reliably under
 *  powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c (no .S pipeline).
 *
 *  Calling convention (PowerPC System V / NT PPC):
 *    r3 = pointer to outgoing KTHREAD context save area (save here)
 *    r4 = pointer to incoming KTHREAD context save area (restore from)
 *    r5 = incoming thread's initial PC (entry point) or NULL to restore LR
 *
 *  Build: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c context.c
 */

#include <stddef.h>

/* SAL annotations (Windows source-code analysis) -- no-op on this target. */
#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif

typedef unsigned long ULONG;
typedef ULONG *PULONG;
typedef void VOID;

/* Context save area layout (offsets in bytes from context base):
 *  0   MSR
 *  4   LR
 *  8   CR
 * 12   XER
 * 16   CTR
 * 20..144  GPRs r0..r31 (r0@20, r1@24, ..., r31@144)
 */
#define CTX_MSR   0
#define CTX_LR    4
#define CTX_CR    8
#define CTX_XER   12
#define CTX_CTR   16
#define CTX_GPR0  20

/*
 * HalpSwapContext - switch from outgoing thread to incoming thread.
 *
 * Saves all CPU state to *OutContext, restores all CPU state from
 * *InContext, switches stack pointer, and branches to InEntry (if
 * non-NULL) or returns via the restored LR.
 */
VOID
HalpSwapContext(IN VOID *OutContext, IN VOID *InContext, IN VOID *InEntry)
{
    PULONG out = (PULONG)OutContext;
    PULONG in = (PULONG)InContext;
    ULONG gpr;

    /* ================================================================
     * SAVE outgoing context (MSR, LR, CR, XER, CTR)
     * ================================================================ */
    __asm__ __volatile__ ("mfmsr   %0" : "=r" (gpr));
    out[CTX_MSR / 4] = gpr;

    __asm__ __volatile__ ("mflr    %0" : "=r" (gpr));
    out[CTX_LR / 4] = gpr;

    __asm__ __volatile__ ("mfcr    %0" : "=r" (gpr));
    out[CTX_CR / 4] = gpr;

    __asm__ __volatile__ ("mfxer   %0" : "=r" (gpr));
    out[CTX_XER / 4] = gpr;

    __asm__ __volatile__ ("mfctr   %0" : "=r" (gpr));
    out[CTX_CTR / 4] = gpr;

    /* Save GPRs r0-r31 (explicit stores) */
    __asm__ __volatile__ (
        "stw   0, 0(%0)\n"
        "stw   1, 4(%0)\n"
        "stw   2, 8(%0)\n"
        "stw   3, 12(%0)\n"
        "stw   4, 16(%0)\n"
        "stw   5, 20(%0)\n"
        "stw   6, 24(%0)\n"
        "stw   7, 28(%0)\n"
        "stw   8, 32(%0)\n"
        "stw   9, 36(%0)\n"
        "stw   10, 40(%0)\n"
        "stw   11, 44(%0)\n"
        "stw   12, 48(%0)\n"
        "stw   13, 52(%0)\n"
        "stw   14, 56(%0)\n"
        "stw   15, 60(%0)\n"
        "stw   16, 64(%0)\n"
        "stw   17, 68(%0)\n"
        "stw   18, 72(%0)\n"
        "stw   19, 76(%0)\n"
        "stw   20, 80(%0)\n"
        "stw   21, 84(%0)\n"
        "stw   22, 88(%0)\n"
        "stw   23, 92(%0)\n"
        "stw   24, 96(%0)\n"
        "stw   25, 100(%0)\n"
        "stw   26, 104(%0)\n"
        "stw   27, 108(%0)\n"
        "stw   28, 112(%0)\n"
        "stw   29, 116(%0)\n"
        "stw   30, 120(%0)\n"
        "stw   31, 124(%0)\n"
        : /* no outputs */
        : "r" (&out[CTX_GPR0 / 4])
        : "memory"
    );

    /* ================================================================
     * SWITCH stack pointer to incoming thread's stack
     * ================================================================ */
    __asm__ __volatile__ (
        "lwz   %0, 24(%1)\n"         /* load incoming r1 (SP) from offset 24 */
        "mr    %1, %0\n"             /* r1 = incoming SP */
        : "=r" (gpr), "+r" (in)
        :
        : "memory"
    );

    /* ================================================================
     * RESTORE incoming context (MSR, LR, CR, XER, CTR)
     * ================================================================ */
    gpr = in[CTX_MSR / 4];
    __asm__ __volatile__ ("mtmsr   %0" :: "r" (gpr));

    gpr = in[CTX_LR / 4];
    __asm__ __volatile__ ("mtlr    %0" :: "r" (gpr));

    gpr = in[CTX_CR / 4];
    __asm__ __volatile__ ("mtcr    %0" :: "r" (gpr));

    gpr = in[CTX_XER / 4];
    __asm__ __volatile__ ("mtxer   %0" :: "r" (gpr));

    gpr = in[CTX_CTR / 4];
    __asm__ __volatile__ ("mtctr   %0" :: "r" (gpr));

    /* Restore GPRs r0-r31 (explicit loads) */
    __asm__ __volatile__ (
        "lwz   0, 0(%0)\n"
        "lwz   1, 4(%0)\n"
        "lwz   2, 8(%0)\n"
        "lwz   3, 12(%0)\n"
        "lwz   4, 16(%0)\n"
        "lwz   5, 20(%0)\n"
        "lwz   6, 24(%0)\n"
        "lwz   7, 28(%0)\n"
        "lwz   8, 32(%0)\n"
        "lwz   9, 36(%0)\n"
        "lwz   10, 40(%0)\n"
        "lwz   11, 44(%0)\n"
        "lwz   12, 48(%0)\n"
        "lwz   13, 52(%0)\n"
        "lwz   14, 56(%0)\n"
        "lwz   15, 60(%0)\n"
        "lwz   16, 64(%0)\n"
        "lwz   17, 68(%0)\n"
        "lwz   18, 72(%0)\n"
        "lwz   19, 76(%0)\n"
        "lwz   20, 80(%0)\n"
        "lwz   21, 84(%0)\n"
        "lwz   22, 88(%0)\n"
        "lwz   23, 92(%0)\n"
        "lwz   24, 96(%0)\n"
        "lwz   25, 100(%0)\n"
        "lwz   26, 104(%0)\n"
        "lwz   27, 108(%0)\n"
        "lwz   28, 112(%0)\n"
        "lwz   29, 116(%0)\n"
        "lwz   30, 120(%0)\n"
        "lwz   31, 124(%0)\n"
        : /* no outputs */
        : "r" (&in[CTX_GPR0 / 4])
        : "memory"
    );

    /* ================================================================
     * RETURN to incoming thread via LR (or branch to InEntry if set)
     * ================================================================ */
    if (InEntry != NULL) {
        __asm__ __volatile__ (
            "mtctr   %0\n"
            "bctr\n"
            :: "r" (InEntry)
        );
        /* unreachable */
    }

    __asm__ __volatile__ ("blr");
}
