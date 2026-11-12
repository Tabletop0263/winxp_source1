/*++

Copyright (c) Microsoft Corporation

Module Name:

    exceptn.c

Abstract:

    This module implements the code necessary to dispatch exceptions
    to the proper mode and invoke the exception dispatcher on
    PowerPC.  It is the PPC analog of base/ntos/ke/i386/exceptn.c.

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"

/*
 * Minimal forward declarations shared with the user-mode NTDLL
 * exception dispatcher (base/ntdll/ppc/dispatch.c).
 */
typedef long EXCEPTION_DISPOSITION;
#define ExceptionContinueExecution 0
#define ExceptionContinueSearch    1

typedef struct _EXCEPTION_RECORD {
    long            ExceptionCode;
    unsigned long   ExceptionFlags;
    struct _EXCEPTION_RECORD *ExceptionRecord;
    void           *ExceptionAddress;
    unsigned long   NumberParameters;
    unsigned long   ExceptionInformation[15];
} EXCEPTION_RECORD, *PEXCEPTION_RECORD;

typedef struct _CONTEXT {
    unsigned long   ContextFlags;
    unsigned long   Msr;
    unsigned long   Srr0;
    unsigned long   Srr1;
    unsigned long   Lr;
    unsigned long   Cr;
    unsigned long   Xer;
    unsigned long   Ctr;
    unsigned long   Iar;
    unsigned long   Gpr0;
    unsigned long   Gpr1;                 /* stack pointer */
    unsigned long   Gpr2;
    unsigned long   Gpr3;
    unsigned long   Gpr4;
    unsigned long   Gpr5;
    unsigned long   Gpr6;
    unsigned long   Gpr7;
    unsigned long   Gpr8;
    unsigned long   Gpr9;
    unsigned long   Gpr10;
    unsigned long   Gpr11;
    unsigned long   Gpr12;
    unsigned long   Gpr14;
    unsigned long   Gpr15;
    unsigned long   Gpr16;
    unsigned long   Gpr17;
    unsigned long   Gpr18;
    unsigned long   Gpr19;
    unsigned long   Gpr20;
    unsigned long   Gpr21;
    unsigned long   Gpr22;
    unsigned long   Gpr23;
    unsigned long   Gpr24;
    unsigned long   Gpr25;
    unsigned long   Gpr26;
    unsigned long   Gpr27;
    unsigned long   Gpr28;
    unsigned long   Gpr29;
    unsigned long   Gpr30;
    unsigned long   Gpr31;
} CONTEXT, *PCONTEXT;

VOID
RtlCaptureContext(OUT PCONTEXT Context);

EXCEPTION_DISPOSITION
RtlDispatchException(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN OUT PCONTEXT ContextRecord
    );

#define STATUS_ILLEGAL_INSTRUCTION ((long)0xC000001DL)
#define KERNEL_MODE_EXCEPTION_NOT_HANDLED ((long)0xC0000045L)

/*
 * Cross-module externs (types now defined above).
 */
extern EXCEPTION_DISPOSITION RtlDispatchException(IN PEXCEPTION_RECORD, IN OUT PCONTEXT);
extern PVOID KeGetExceptionList(VOID);
extern VOID KeBugCheckEx(IN ULONG, IN ULONG_PTR, IN ULONG_PTR, IN ULONG_PTR, IN ULONG_PTR);

#define CONTEXT_CONTROL   0x00010001L   /* MSR, SRR0/SRR1, LR, CR, XER, CTR */
#define CONTEXT_INTEGER   0x00010002L   /* GPRs r0-r31 */
#define CONTEXT_FULL      (CONTEXT_CONTROL | CONTEXT_INTEGER)

/*--------------------------------------------------------------------------
    KeContextFromKframes - copy trap frame into a CONTEXT record.

    PPC analog of i386's KeContextFromKframes. Selectively copies control
    and integer register state from the trap frame into the context record
    based on ContextFlags.
--------------------------------------------------------------------------*/
static VOID
KeContextFromKframes(
    IN PKTRAP_FRAME TrapFrame,
    IN OUT PCONTEXT ContextFrame
    )
{
    if ((ContextFrame->ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL) {
        ContextFrame->Msr   = TrapFrame->Msr;
        ContextFrame->Srr0  = TrapFrame->Srr0;
        ContextFrame->Srr1  = TrapFrame->Srr1;
        ContextFrame->Lr    = TrapFrame->Lr;
        ContextFrame->Cr    = TrapFrame->Cr;
        ContextFrame->Xer   = TrapFrame->Xer;
        ContextFrame->Ctr   = TrapFrame->Ctr;
        ContextFrame->Iar   = TrapFrame->Iar;
    }

    if ((ContextFrame->ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER) {
        ContextFrame->Gpr0  = TrapFrame->Gpr0;
        ContextFrame->Gpr1  = TrapFrame->TempEsp; /* SP (r1) at trap time */
        ContextFrame->Gpr3  = TrapFrame->Gpr3;
        ContextFrame->Gpr4  = TrapFrame->Gpr4;
        ContextFrame->Gpr5  = TrapFrame->Gpr5;
        ContextFrame->Gpr6  = TrapFrame->Gpr6;
        ContextFrame->Gpr7  = TrapFrame->Gpr7;
        ContextFrame->Gpr8  = TrapFrame->Gpr8;
        ContextFrame->Gpr9  = TrapFrame->Gpr9;
        ContextFrame->Gpr10 = TrapFrame->Gpr10;
        ContextFrame->Gpr11 = TrapFrame->Gpr11;
        ContextFrame->Gpr12 = TrapFrame->Gpr12;
        ContextFrame->Gpr14 = TrapFrame->Gpr14;
        ContextFrame->Gpr15 = TrapFrame->Gpr15;
        ContextFrame->Gpr16 = TrapFrame->Gpr16;
        ContextFrame->Gpr17 = TrapFrame->Gpr17;
        ContextFrame->Gpr18 = TrapFrame->Gpr18;
        ContextFrame->Gpr19 = TrapFrame->Gpr19;
        ContextFrame->Gpr20 = TrapFrame->Gpr20;
        ContextFrame->Gpr21 = TrapFrame->Gpr21;
        ContextFrame->Gpr22 = TrapFrame->Gpr22;
        ContextFrame->Gpr23 = TrapFrame->Gpr23;
        ContextFrame->Gpr24 = TrapFrame->Gpr24;
        ContextFrame->Gpr25 = TrapFrame->Gpr25;
        ContextFrame->Gpr26 = TrapFrame->Gpr26;
        ContextFrame->Gpr27 = TrapFrame->Gpr27;
        ContextFrame->Gpr28 = TrapFrame->Gpr28;
        ContextFrame->Gpr29 = TrapFrame->Gpr29;
        ContextFrame->Gpr30 = TrapFrame->Gpr30;
        ContextFrame->Gpr31 = TrapFrame->Gpr31;
    }
}

/*--------------------------------------------------------------------------
    KeContextToKframes - copy a CONTEXT record back into the trap frame.

    PPC analog of i386's KeContextToKframes. Used when an exception handler
    has modified the context and asked to continue execution; the modified
    state is written back so the interrupt exit (rfi) resumes there.
--------------------------------------------------------------------------*/
static VOID
KeContextToKframes(
    IN OUT PKTRAP_FRAME TrapFrame,
    IN PCONTEXT ContextFrame,
    IN ULONG ContextFlags
    )
{
    if ((ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL) {
        TrapFrame->Msr   = ContextFrame->Msr;
        TrapFrame->Srr0  = ContextFrame->Srr0;
        TrapFrame->Srr1  = ContextFrame->Srr1;
        TrapFrame->Lr    = ContextFrame->Lr;
        TrapFrame->Cr    = ContextFrame->Cr;
        TrapFrame->Xer   = ContextFrame->Xer;
        TrapFrame->Ctr   = ContextFrame->Ctr;
        TrapFrame->Iar   = ContextFrame->Iar;
    }

    if ((ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER) {
        TrapFrame->TempEsp = ContextFrame->Gpr1;  /* SP (r1) */
        TrapFrame->Gpr0    = ContextFrame->Gpr0;
        TrapFrame->Gpr3    = ContextFrame->Gpr3;
        TrapFrame->Gpr4    = ContextFrame->Gpr4;
        TrapFrame->Gpr5    = ContextFrame->Gpr5;
        TrapFrame->Gpr6    = ContextFrame->Gpr6;
        TrapFrame->Gpr7    = ContextFrame->Gpr7;
        TrapFrame->Gpr8    = ContextFrame->Gpr8;
        TrapFrame->Gpr9    = ContextFrame->Gpr9;
        TrapFrame->Gpr10   = ContextFrame->Gpr10;
        TrapFrame->Gpr11   = ContextFrame->Gpr11;
        TrapFrame->Gpr12   = ContextFrame->Gpr12;
        TrapFrame->Gpr14   = ContextFrame->Gpr14;
        TrapFrame->Gpr15   = ContextFrame->Gpr15;
        TrapFrame->Gpr16   = ContextFrame->Gpr16;
        TrapFrame->Gpr17   = ContextFrame->Gpr17;
        TrapFrame->Gpr18   = ContextFrame->Gpr18;
        TrapFrame->Gpr19   = ContextFrame->Gpr19;
        TrapFrame->Gpr20   = ContextFrame->Gpr20;
        TrapFrame->Gpr21   = ContextFrame->Gpr21;
        TrapFrame->Gpr22   = ContextFrame->Gpr22;
        TrapFrame->Gpr23   = ContextFrame->Gpr23;
        TrapFrame->Gpr24   = ContextFrame->Gpr24;
        TrapFrame->Gpr25   = ContextFrame->Gpr25;
        TrapFrame->Gpr26   = ContextFrame->Gpr26;
        TrapFrame->Gpr27   = ContextFrame->Gpr27;
        TrapFrame->Gpr28   = ContextFrame->Gpr28;
        TrapFrame->Gpr29   = ContextFrame->Gpr29;
        TrapFrame->Gpr30   = ContextFrame->Gpr30;
        TrapFrame->Gpr31   = ContextFrame->Gpr31;
    }
}

/*--------------------------------------------------------------------------
    KiExceptionDispatch - dispatch a kernel/trap exception.

    Called from the HAL interrupt entry (HalpInterruptEntry) with a fully
    populated KTRAP_FRAME.  Converts the trap frame into a CONTEXT record,
    builds an EXCEPTION_RECORD from the trap number, and hands them to the
    user-mode NTDLL dispatcher (RtlDispatchException).  Based on the
    disposition:
      * ExceptionContinueExecution  -> restore trap frame from context,
                                       return so the interrupt exit rfi's
                                       to the restored PC.
      * otherwise                   -> no handler handled it; bugcheck.
--------------------------------------------------------------------------*/
VOID
KiExceptionDispatch(
    IN PKTRAP_FRAME TrapFrame
    )
{
    EXCEPTION_RECORD record;
    CONTEXT context;
    EXCEPTION_DISPOSITION disp;

    /* Build a CONTEXT from the trap frame (ppctx.h layout). */
    context.ContextFlags = CONTEXT_FULL;
    KeContextFromKframes(TrapFrame, &context);

    /* Map the trap number to an NT exception code. */
    record.ExceptionCode      = STATUS_ILLEGAL_INSTRUCTION;
    record.ExceptionFlags     = 0;
    record.ExceptionRecord    = NULL;
    record.ExceptionAddress   = (void *)(unsigned long)TrapFrame->Iar;
    record.NumberParameters   = 0;

    /* Hand to the NTDLL exception dispatcher (frame-based SEH walk). */
    disp = RtlDispatchException(&record, &context);

    if (disp == ExceptionContinueExecution) {
        /* A handler handled the exception and wants to resume at the
         * (possibly modified) PC in the context.  Write the context back
         * into the trap frame so the interrupt exit rfi's there. */
        KeContextToKframes(TrapFrame, &context, context.ContextFlags);
        return;
    }

    /* Unhandled kernel exception. */
    KeBugCheckEx(
        KERNEL_MODE_EXCEPTION_NOT_HANDLED,
        (ULONG)record.ExceptionCode,
        (ULONG_PTR)record.ExceptionAddress,
        (ULONG_PTR)TrapFrame,
        0);
}

/*
 * KiUnexpectedInterrupt32 - unexpected interrupt handler.
 *
 * Catch-all for vectors that have no installed ISR.
 */
VOID
KiUnexpectedInterrupt32(
    VOID
    )
{
    /* Stub: spin. */
    for (;;) {
        __asm__ __volatile__ ("nop");
    }
}
