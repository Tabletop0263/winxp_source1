/*
 *  dispatch.c - PowerPC (Broadway/G3) exception dispatch
 *
 *  NTDLL architecture primitive for Windows XP (NT 5.1) port to PowerPC.
 *
 *  Implements RtlDispatchException, RtlVirtualUnwind, and
 *  RtlLookupFunctionEntry stubs for PowerPC. PowerPC has no x87 FPU,
 *  no segment registers (CS/DS/ES), and no EFLAGS - exceptions are
 *  delivered via SRR0/SRR1 with the program counter in Iar.
 *
 *  Build: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c dispatch.c
 */

#include "context.h"

/* SAL no-op macros. */
#ifndef IN
#define IN
#define OUT
#define OPTIONAL
#endif

/* Forward declarations: context capture/restore are implemented in
 * context.c (this same directory). Declared here so dispatch.c can use
 * them without needing context.h to pull in full definitions. */
VOID RtlCaptureContext(OUT PCONTEXT Context);
VOID RtlRestoreContext(IN PCONTEXT Context, IN PKTRAP_FRAME TrapFrame OPTIONAL);

/* Forward declaration: PCR-based SEH chain head (ntos/ppc/pcr.c).
 * On PowerPC there is no FS:[0]; the chain head lives in the PCR. */
PVOID KeGetExceptionList(VOID);

/* Forward declaration: the language-specific exception handler
 * callback type (mirrors i386's EXCEPTION_DISPOSITION). */
typedef enum _EXCEPTION_DISPOSITION {
    ExceptionContinueExecution = 0,
    ExceptionContinueSearch    = 1,
    ExceptionNestedException   = 2,
    ExceptionCollidedUnwind    = 3
} EXCEPTION_DISPOSITION;

typedef EXCEPTION_DISPOSITION (*PEXCEPTION_ROUTINE)(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PVOID DispatcherContext
    );

/*
 * EXCEPTION_REGISTRATION_RECORD - one node in the thread's SEH chain.
 * Mirrors i386's registration record: Next points to the previous record,
 * Handler is the language-specific handler callback.
 */
typedef struct _EXCEPTION_REGISTRATION_RECORD {
    struct _EXCEPTION_REGISTRATION_RECORD *Next;
    PEXCEPTION_ROUTINE                     Handler;
} EXCEPTION_REGISTRATION_RECORD, *PEXCEPTION_REGISTRATION_RECORD;

#define EXCEPTION_CHAIN_END ((PEXCEPTION_REGISTRATION_RECORD)0xFFFFFFFF)

/*
 * Function table entry (minimal). The i386 version uses
 * RUNTIME_FUNCTION; this is a PowerPC analog.
 */
typedef struct _RUNTIME_FUNCTION {
    ULONG_PTR BeginAddress;
    ULONG_PTR EndAddress;
    PVOID     UnwindInfo;
} RUNTIME_FUNCTION, *PRUNTIME_FUNCTION;

/*--------------------------------------------------------------------------
    Registered function table for unwind info.

    Each entry describes one module's exception/unwind table: a sorted
    array of RUNTIME_FUNCTION entries covering that module's code ranges.
--------------------------------------------------------------------------*/
#define MAX_FUNCTION_TABLES 8

typedef struct _FUNCTION_TABLE_ENTRY {
    PRUNTIME_FUNCTION Table;
    ULONG             EntryCount;
    ULONG_PTR         ImageBase;
} FUNCTION_TABLE_ENTRY;

static FUNCTION_TABLE_ENTRY HalpFunctionTables[MAX_FUNCTION_TABLES];
static ULONG HalpFunctionTableCount = 0;

/*--------------------------------------------------------------------------
    RtlAddFunctionTable - register a module's unwind function table.

    Called by the loader or by driver init to register the exception
    dispatch/unwind table for a module so RtlLookupFunctionEntry and
    RtlVirtualUnwind can locate unwind info for any PC in that module.
--------------------------------------------------------------------------*/
VOID
RtlAddFunctionTable(
    IN PRUNTIME_FUNCTION FunctionTable,
    IN ULONG             EntryCount,
    IN ULONG_PTR         ImageBase
    )
{
    ULONG i;

    /* Avoid duplicate registration of the same image base. */
    for (i = 0; i < HalpFunctionTableCount; i++) {
        if (HalpFunctionTables[i].ImageBase == ImageBase) {
            HalpFunctionTables[i].Table       = FunctionTable;
            HalpFunctionTables[i].EntryCount  = EntryCount;
            return;
        }
    }

    if (HalpFunctionTableCount < MAX_FUNCTION_TABLES) {
        HalpFunctionTables[HalpFunctionTableCount].Table      = FunctionTable;
        HalpFunctionTables[HalpFunctionTableCount].EntryCount = EntryCount;
        HalpFunctionTables[HalpFunctionTableCount].ImageBase   = ImageBase;
        HalpFunctionTableCount += 1;
    }
}

/*
 * RtlLookupFunctionEntry - find the function table entry for a PC.
 *
 * PowerPC implementation: walks the registered function tables and
 * returns the entry whose [BeginAddress, EndAddress) range contains
 * ControlPc. Returns NULL if no table covers the PC (no unwind info
 * available for that address).
 */
PRUNTIME_FUNCTION
RtlLookupFunctionEntry(IN ULONG_PTR ControlPc)
{
    ULONG i, j;

    for (i = 0; i < HalpFunctionTableCount; i++) {
        PRUNTIME_FUNCTION tbl = HalpFunctionTables[i].Table;

        if (tbl == NULL) {
            continue;
        }

        /* Tables are assumed sorted by BeginAddress. Linear scan is fine
         * for the small tables this port uses. */
        for (j = 0; j < HalpFunctionTables[i].EntryCount; j++) {
            if (ControlPc >= tbl[j].BeginAddress &&
                ControlPc <  tbl[j].EndAddress) {
                return &tbl[j];
            }
        }
    }

    return NULL;
}

/*
 * RtlVirtualUnwind - virtually unwind a stack frame.
 *
 * PowerPC implementation: looks up the function table entry for ControlPc,
 * reads the non-volatile GPRs, LR, CR and saved PC from the trap frame into
 * the context record, and advances the context to the caller's frame using
 * the link register (the PPC return address). Returns TRUE if unwinding
 * succeeded (a valid unwind target was found), FALSE otherwise.
 */
BOOLEAN
RtlVirtualUnwind(
    IN ULONG HandlerType,
    IN ULONG_PTR ImageBase,
    IN ULONG_PTR ControlPc,
    IN OUT PKTRAP_FRAME TrapFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PVOID *HandlerData,
    IN OUT PULONG EstablisherFrame
    )
{
    PRUNTIME_FUNCTION function_entry;

    (void)HandlerType;
    (void)ImageBase;
    (void)HandlerData;

    if (TrapFrame == NULL || ContextRecord == NULL) {
        return FALSE;
    }

    /* Locate the unwind info for the current PC, if any. */
    function_entry = RtlLookupFunctionEntry(ControlPc);
    if (function_entry != NULL) {
        if (HandlerData != NULL) {
            *HandlerData = function_entry->UnwindInfo;
        }
    }

    /* Copy control registers from the trap frame into the context. */
    ContextRecord->Msr   = TrapFrame->Msr;
    ContextRecord->Srr0  = TrapFrame->Srr0;
    ContextRecord->Srr1  = TrapFrame->Srr1;
    ContextRecord->Lr    = TrapFrame->Lr;
    ContextRecord->Cr    = TrapFrame->Cr;
    ContextRecord->Xer   = TrapFrame->Xer;
    ContextRecord->Ctr   = TrapFrame->Ctr;
    ContextRecord->Iar   = TrapFrame->Iar; /* PC from trap frame */

    /* Copy non-volatile GPRs r14-r31. */
    if (ContextRecord->ContextFlags & CONTEXT_INTEGER) {
        ContextRecord->Gpr14 = TrapFrame->Gpr14;
        ContextRecord->Gpr15 = TrapFrame->Gpr15;
        ContextRecord->Gpr16 = TrapFrame->Gpr16;
        ContextRecord->Gpr17 = TrapFrame->Gpr17;
        ContextRecord->Gpr18 = TrapFrame->Gpr18;
        ContextRecord->Gpr19 = TrapFrame->Gpr19;
        ContextRecord->Gpr20 = TrapFrame->Gpr20;
        ContextRecord->Gpr21 = TrapFrame->Gpr21;
        ContextRecord->Gpr22 = TrapFrame->Gpr22;
        ContextRecord->Gpr23 = TrapFrame->Gpr23;
        ContextRecord->Gpr24 = TrapFrame->Gpr24;
        ContextRecord->Gpr25 = TrapFrame->Gpr25;
        ContextRecord->Gpr26 = TrapFrame->Gpr26;
        ContextRecord->Gpr27 = TrapFrame->Gpr27;
        ContextRecord->Gpr28 = TrapFrame->Gpr28;
        ContextRecord->Gpr29 = TrapFrame->Gpr29;
        ContextRecord->Gpr30 = TrapFrame->Gpr30;
        ContextRecord->Gpr31 = TrapFrame->Gpr31;

        /* Volatile GPRs r3-r12, r0. */
        ContextRecord->Gpr0  = TrapFrame->Gpr0;
        ContextRecord->Gpr3  = TrapFrame->Gpr3;
        ContextRecord->Gpr4  = TrapFrame->Gpr4;
        ContextRecord->Gpr5  = TrapFrame->Gpr5;
        ContextRecord->Gpr6  = TrapFrame->Gpr6;
        ContextRecord->Gpr7  = TrapFrame->Gpr7;
        ContextRecord->Gpr8  = TrapFrame->Gpr8;
        ContextRecord->Gpr9  = TrapFrame->Gpr9;
        ContextRecord->Gpr10 = TrapFrame->Gpr10;
        ContextRecord->Gpr11 = TrapFrame->Gpr11;
        ContextRecord->Gpr12 = TrapFrame->Gpr12;
    }

    /* On PowerPC the link register holds the return address.  Advance the
     * PC to the caller (LR) so the exception dispatcher can continue
     * walking up the stack.  SP comes from TempEsp (r1 at trap time). */
    ContextRecord->Iar   = TrapFrame->Lr;   /* return address */
    ContextRecord->Gpr1  = TrapFrame->TempEsp;

    /* The EstablisherFrame is the stack pointer at the time of the
     * exception (TempEsp/r1 captured in the trap frame). */
    *EstablisherFrame = (ULONG)(unsigned long)TrapFrame->TempEsp;

    return TRUE;
}

/*
 * RtlDispatchException - dispatch an exception to registered handlers.
 *
 * PowerPC implementation: walks the thread's SEH registration chain
 * (kept in the PCR as ExceptionList), calling each language-specific
 * handler in turn with the exception record, a fresh EstablisherFrame
 * (the stack pointer), the context record, and a per-frame dispatcher
 * context. Stops on ExceptionContinueExecution (handler handled it) or
 * when the chain is exhausted (return ExceptionContinueSearch).
 *
 * This is the PPC analog of i386's RtlDispatchException walking the
 * FS:[0] frame-based SEH chain.
 */
EXCEPTION_DISPOSITION
RtlDispatchException(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN OUT PCONTEXT ContextRecord
    )
{
    PEXCEPTION_REGISTRATION_RECORD registration;
    EXCEPTION_DISPOSITION disposition;
    PVOID dispatcher_context = NULL;
    ULONG establisher_frame;

    (void)dispatcher_context; /* unused in this stub-port pass */

    /* Walk the registration chain head-to-tail. */
    registration = (PEXCEPTION_REGISTRATION_RECORD)KeGetExceptionList();

    while (registration != NULL && registration != EXCEPTION_CHAIN_END) {

        /* The EstablisherFrame is the stack pointer at the time of the
         * exception (TempEsp/r1 captured in the trap frame). i386 passes
         * the current ESP here; on PPC we pass Gpr1 from the context. */
        establisher_frame = (ULONG)ContextRecord->Gpr1;

        disposition = registration->Handler(
            ExceptionRecord,
            (PVOID)(unsigned long)establisher_frame,
            ContextRecord,
            dispatcher_context
            );

        if (disposition == ExceptionContinueExecution) {
            /* Handler handled the exception; resume at its (possibly
             * modified) PC. */
            return ExceptionContinueExecution;
        }

        if (disposition == ExceptionNestedException) {
            /* A nested exception occurred while handling this one;
             * restart the walk from the chain head. */
            registration = (PEXCEPTION_REGISTRATION_RECORD)KeGetExceptionList();
            continue;
        }

        /* ExceptionContinueSearch or ExceptionCollidedUnwind: try the
         * next registration record in the chain. */
        registration = registration->Next;
    }

    /* Chain exhausted - no handler claimed the exception. */
    return ExceptionContinueSearch;
}

/*
 * RtlRaiseException - raise an exception.
 *
 * PowerPC stub: calls RtlDispatchException; if no handler continues
 * execution, terminate (stub: return the exception code).
 */
NTSTATUS
RtlRaiseException(IN PEXCEPTION_RECORD ExceptionRecord)
{
    EXCEPTION_DISPOSITION disp;
    CONTEXT ctx;

    RtlCaptureContext(&ctx);

    disp = RtlDispatchException(ExceptionRecord, &ctx);

    if (disp == ExceptionContinueExecution) {
        /* Handler said continue - restore and resume. */
        RtlRestoreContext(&ctx, NULL);
        return STATUS_SUCCESS;
    }

    /* Unhandled exception - stub returns the code. */
    return ExceptionRecord->ExceptionCode;
}
