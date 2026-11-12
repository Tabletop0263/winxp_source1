/*++
Copyright (c) Microsoft Corporation

Module Name:

    ntoskrnl.c

Abstract:

    This module contains the kernel initialization code for the PowerPC
    (Broadway/G3) port of Windows XP (NT 5.1).

    KeInitializeKernel is the arch-independent kernel initialization entry.
    It performs phase-0/phase-1 setup scaffolding and then calls the
    PPC-specific KeInitializeKernelPpc() to install the PCR, exception
    vectors, Decrementer, and enable interrupts.

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"

/* Kernel phase tracking (mirrors i386's KeBootPhase). */
static ULONG KeBootPhase = 0;

/* Loader block (PHW_DESCRIPTION from entii arcldr) passed in during boot. */
static PVOID KeLoaderBlock = NULL;

/*--------------------------------------------------------------------------
    KeInitializeKernel - arch-independent kernel initialization entry.

    Called by the kernel entry stub (ntos_entry.S / _start).  Performs
    arch-independent phase setup, then delegates to the PPC-specific
    KeInitializeKernelPpc() for PCR/vector/Decrementer/MSR setup.

Arguments:

    LoaderBlock - PHW_DESCRIPTION passed by entii's arcldr (r3 on entry).
                  Stored so the HAL can read clock frequency and memory map.

Return Value:

    STATUS_SUCCESS.

--*/
NTSTATUS
KeInitializeKernel(
    IN PVOID LoaderBlock
    )
{
    NTSTATUS Status;

    /* Remember the loader block for HAL/phase init to consume. */
    KeLoaderBlock = LoaderBlock;

    /* Phase 0: executive/object/lock initialization happens here in a full
     * port. For this minimal boot port we record the phase and proceed to
     * arch-specific init. */
    KeBootPhase = 0;

    /* PPC-specific kernel initialization (PCR, vectors, Decrementer, MSR). */
    Status = KeInitializeKernelPpc(KeLoaderBlock);

    /* Phase 1 would run the rest of executive init (timer, profile, etc.). */
    KeBootPhase = 1;

    return Status;
}

/*--------------------------------------------------------------------------
    KeGetBootPhase - return the current kernel boot phase.
--------------------------------------------------------------------------*/
ULONG
KeGetBootPhase(
    VOID
    )
{
    return KeBootPhase;
}
