/*
 *  context.c - PowerPC (Broadway/G3) context capture / restore
 *
 *  NTDLL architecture primitive for Windows XP (NT 5.1) port to PowerPC.
 *
 *  Implements RtlCaptureContext and RtlRestoreContext for PowerPC,
 *  saving/restoring GPRs, MSR, SRR0/SRR1, LR, CR, XER, CTR into a
 *  CONTEXT structure.
 *
 *  Build: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c context.c
 */

#include "context.h"

/* SAL no-op macros (already defined in context.h, but included here
 * for clarity when compiled standalone). */
#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif
#ifndef OPTIONAL
#define OPTIONAL
#endif

static __inline__ ULONG __get_MSR(void)
{
    ULONG msr;
    __asm__ __volatile__ ("mfmsr %0" : "=r" (msr));
    return msr;
}

static __inline__ void __set_MSR(ULONG msr)
{
    __asm__ __volatile__ ("mtmsr %0" :: "r" (msr));
}

static __inline__ ULONG __get_LR(void)
{
    ULONG lr;
    __asm__ __volatile__ ("mflr %0" : "=r" (lr));
    return lr;
}

static __inline__ void __set_LR(ULONG lr)
{
    __asm__ __volatile__ ("mtlr %0" :: "r" (lr));
}

static __inline__ ULONG __get_CR(void)
{
    ULONG cr;
    __asm__ __volatile__ ("mfcr %0" : "=r" (cr));
    return cr;
}

static __inline__ void __set_CR(ULONG cr)
{
    __asm__ __volatile__ ("mtcr %0" :: "r" (cr));
}

static __inline__ ULONG __get_XER(void)
{
    ULONG xer;
    __asm__ __volatile__ ("mfxer %0" : "=r" (xer));
    return xer;
}

static __inline__ void __set_XER(ULONG xer)
{
    __asm__ __volatile__ ("mtxer %0" :: "r" (xer));
}

static __inline__ ULONG __get_CTR(void)
{
    ULONG ctr;
    __asm__ __volatile__ ("mfctr %0" : "=r" (ctr));
    return ctr;
}

static __inline__ void __set_CTR(ULONG ctr)
{
    __asm__ __volatile__ ("mtctr %0" :: "r" (ctr));
}

static __inline__ ULONG __get_SRR0(void)
{
    ULONG srr0;
    __asm__ __volatile__ ("mfspr %0, 0" : "=r" (srr0));
    return srr0;
}

static __inline__ void __set_SRR0(ULONG val)
{
    __asm__ __volatile__ ("mtspr 0, %0" :: "r" (val));
}

static __inline__ ULONG __get_SRR1(void)
{
    ULONG srr1;
    __asm__ __volatile__ ("mfspr %0, 1" : "=r" (srr1));
    return srr1;
}

static __inline__ void __set_SRR1(ULONG val)
{
    __asm__ __volatile__ ("mtspr 1, %0" :: "r" (val));
}

/*
 * RtlCaptureContext - capture the current CPU state into a CONTEXT.
 *
 * PowerPC implementation: reads MSR, SRR0/SRR1, LR, CR, XER, CTR,
 * and all GPRs. No x87/MMX/segment registers exist on PowerPC.
 */
VOID
RtlCaptureContext(OUT PCONTEXT Context)
{
    Context->ContextFlags = CONTEXT_FULL;
    Context->Msr   = __get_MSR();
    Context->Srr0  = __get_SRR0();
    Context->Srr1  = __get_SRR1();
    Context->Lr    = __get_LR();
    Context->Cr    = __get_CR();
    Context->Xer   = __get_XER();
    Context->Ctr   = __get_CTR();

    /* Iar (program counter) is not directly readable; use SRR0 as
     * the best available approximation of the current PC. */
    Context->Iar   = Context->Srr0;

    /* Capture GPRs via inline assembly with explicit register loads. */
    __asm__ __volatile__ (
        "lwz   0, 0(%0)\n"      /* dummy to satisfy clobber; actual below */
        : /* no output */
        : "r" (&Context->Gpr0)
        : "memory", "r0"
    );

    /* Individual GPR captures - each stored directly into Context. */
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
        : /* no output */
        : "r" (&Context->Gpr0)
        : "memory"
    );
}

/*
 * RtlRestoreContext - restore CPU state from a CONTEXT.
 *
 * PowerPC implementation: writes MSR, SRR0/SRR1, LR, CR, XER, CTR,
 * and all GPRs, then branches to Context->Iar (PC).
 *
 * Returns nothing; control transfers to the restored context's PC.
 */
VOID
RtlRestoreContext(IN PCONTEXT Context, IN PKTRAP_FRAME TrapFrame OPTIONAL)
{
    (void)TrapFrame; /* not used in this stub */

    __set_MSR(Context->Msr);
    __set_SRR0(Context->Srr0);
    __set_SRR1(Context->Srr1);
    __set_LR(Context->Lr);
    __set_CR(Context->Cr);
    __set_XER(Context->Xer);
    __set_CTR(Context->Ctr);

    /* Restore GPRs via inline assembly. */
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
        : /* no output */
        : "r" (&Context->Gpr0)
        : "memory"
    );

    /* Branch to restored PC (Iar/Srr0). */
    __asm__ __volatile__ (
        "mtctr   %0\n"
        "bctr\n"
        :: "r" (Context->Iar)
    );
}
