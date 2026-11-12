/*++

Copyright (c) Microsoft Corporation

Module Name:

    pcr.c

Abstract:

    PowerPC Processor Control Region (PCR) setup and storage.

    The PCR is the per-processor control block. On Broadway/G3 there is
    no architecturally mandated PCR register; this port reserves a static
    storage area in memory and provides an accessor plus a helper that
    loads the PCR address into GPR r2 (TOC/global pointer convention).

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"

/*
 * Storage for the boot processor PCR.  Exported so the kernel entry
 * stub (ntos_entry.S) can reference the same instance and load its
 * address into GPR r2.  In a full port this is allocated from the PCR
 * page during phase 0 init.
 */
struct _KPCR HalpPcrStorage;

/* Pointer to the current PCR (set during init). */
static volatile struct _KPCR *HalpPcr = &HalpPcrStorage;

/*
 * KeSetPcr - install the PCR pointer for the current processor.
 */
VOID
KeSetPcr(
    IN struct _KPCR *Pcr
    )
{
    HalpPcr = Pcr;
}

/*
 * KeGetPcr - return the current processor's PCR.
 */
struct _KPCR *
KeGetPcr(
    VOID
    )
{
    return (struct _KPCR *)HalpPcr;
}

/*
 * HalpGetPcrAddress - return the address of the current PCR.
 * Used by assembly code that needs to locate the PCR in memory.
 */
struct _KPCR *
HalpGetPcrAddress(VOID)
{
    return (struct _KPCR *)HalpPcr;
}

/*
 * HalpSetPcrRegister - load the PCR address into GPR r2 (TOC base)
 * so that assembly code can reference the PCR via r2.
 */
VOID
HalpSetPcrRegister(
    IN struct _KPCR *Pcr
    )
{
    __asm__ __volatile__ (
        "mr 2, %0\n"      /* r2 = PCR pointer (TOC/global pointer) */
        :: "r" (Pcr)
        : "r2"
    );
}

/*
 * KeGetExceptionList - return the head of the current thread's SEH
 * registration chain from the PCR. On PowerPC there is no FS:[0], so
 * the chain head lives in the PCR (set by ppc.S/TrapFrame setup).
 */
PVOID
KeGetExceptionList(
    VOID
    )
{
    return HalpPcr->ExceptionList;
}

/*
 * KeBugCheckEx - fatal bugcheck for unhandled kernel exceptions.
 * Stub: spins forever (a real port halts/dumps and reboots).
 */
VOID
KeBugCheckEx(
    IN ULONG BugCheckCode,
    IN ULONG_PTR BugCheckParameter1,
    IN ULONG_PTR BugCheckParameter2,
    IN ULONG_PTR BugCheckParameter3,
    IN ULONG_PTR BugCheckParameter4
    )
{
    (void)BugCheckCode;
    (void)BugCheckParameter1;
    (void)BugCheckParameter2;
    (void)BugCheckParameter3;
    (void)BugCheckParameter4;

    for (;;) {
        __asm__ __volatile__ ("nop");
    }
}
