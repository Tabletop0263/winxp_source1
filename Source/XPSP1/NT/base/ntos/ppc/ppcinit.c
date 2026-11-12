/*++

Copyright (c) Microsoft Corporation

Module Name:

    ppcinit.c

Abstract:

    PowerPC (Broadway/G3) kernel initialization stubs.

    This module contains code to manipulate PowerPC hardware structures
    used only by the kernel.  It is the PPC analog of
    base/ntos/ke/i386/i386init.c.

Author:

    (port)

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ppctx.h"

/* Kernel exception dispatcher (ntos/ppc/exceptn.c). */
VOID
KiExceptionDispatch(IN PKTRAP_FRAME TrapFrame);

/* PCR (ntos/ppc/pcr.c) - defined there; declared extern here. */
extern struct _KPCR HalpPcrStorage;
VOID KeSetPcr(IN struct _KPCR *Pcr);
VOID HalpSetPcrRegister(IN struct _KPCR *Pcr);
PVOID KeGetExceptionList(VOID);
VOID KeBugCheckEx(IN ULONG, IN ULONG_PTR, IN ULONG_PTR, IN ULONG_PTR, IN ULONG_PTR);

/* HAL vector installation (hals/halppc/ppc/interrupt.c) */
VOID HalpInstallVectors(VOID (*Entry)(ULONG));

/* Decrementer/timebase setup (hals/halppc/ppc/interrupt.c) */
VOID HalpInitializeDecrementer(IN ULONG DecrementerFrequency);

/* OpenFirmware clock discovery (hals/halppc/ppc/of.c) */
ULONG HalpGetClockFrequency(VOID);

/* Loader block (PHW_DESCRIPTION from entii arcldr) passed during boot.
 * Stored here so the HAL can read DecrementerFrequency / memory map. */
static PVOID KeLoaderBlock = NULL;

/*--------------------------------------------------------------------------
    KiInitializeMachineType - record the PPC machine type and version.

    Reads the Processor Version Register (PVR) to identify the CPU
    (Broadway is a 750cx/750fx derivative; PVR identifies the exact
    revision) and stores it for later use.
--------------------------------------------------------------------------*/
VOID
KiInitializeMachineType(
    VOID
    )
{
    ULONG pvr;

    pvr = HalpGetSPR(SPR_PVR);

    /* Stub: record the PVR value (no global exported yet). */
    (void)pvr;
}

/*--------------------------------------------------------------------------
    KeInitializeKernelPpc - PPC-specific kernel initialization entry.

    Called from the kernel entry stub (ntos_entry.S / _start) after the
    PCR address has been loaded into GPR r2.  Performs PPC-specific setup:
    PCR installation, IDT/vector table installation, Decrementer setup,
    and machine-type identification.

    LoaderBlock is the PHW_DESCRIPTION passed by entii's arcldr; the HAL
    uses its DecrementerFrequency field to program the Decrementer.
--------------------------------------------------------------------------*/
NTSTATUS
KeInitializeKernelPpc(
    IN PVOID LoaderBlock
    )
{
    /* Remember the loader block for HAL init to consume. */
    KeLoaderBlock = LoaderBlock;

    /* Install the PCR pointer for this processor. */
    KeSetPcr(&HalpPcrStorage);

    /* Load the PCR address into GPR r2 so assembly code can reference it. */
    HalpSetPcrRegister(&HalpPcrStorage);

    /* Install the PowerPC exception vectors (0x000 page -> HalpVectorEntry). */
    HalpInstallVectors(NULL);

    /* Record the PPC machine type from the PVR. */
    KiInitializeMachineType();

    /* Decrementer/timebase initialization:
     * Program SPR_DEC from the loader-block-discovered clock rate so the
     * system tick fires at the correct interval, then enable external
     * interrupts (MSR.EE) so the Decrementer interrupt (vector 0x900)
     * fires.
     *
     * If no loader block or zero frequency is provided, fall back to
     * HalpGetClockFrequency() (OF / compile-time default).
     */
    if (KeLoaderBlock != NULL) {
        /* PHW_DESCRIPTION layout:
         *   offset 0  MemoryLength[0]
         *   offset 4  MemoryLength[1]
         *   offset 8  DdrIpcBase
         *   offset 12 DdrIpcLength
         *   offset 16 DecrementerFrequency  <-- bus clock / 4, used for SPR_DEC
         */
        ULONG dec_freq = *(ULONG *)((ULONG_PTR)KeLoaderBlock + 16);
        if (dec_freq != 0) {
            HalpInitializeDecrementer(dec_freq);
        } else {
            HalpInitializeDecrementer(HalpGetClockFrequency());
        }
    } else {
        HalpInitializeDecrementer(HalpGetClockFrequency());
    }

    return STATUS_SUCCESS;
}
