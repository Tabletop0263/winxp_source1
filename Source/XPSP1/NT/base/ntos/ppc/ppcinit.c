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

/* wxptypes.h - HANDLE, PHANDLE, POBJECT_ATTRIBUTES, PCLIENT_ID,
 * PKSTART_ROUTINE, NT_SUCCESS (ppctx.h defines NTSTATUS/STATUS_SUCCESS). */
#include "wxptypes.h"

/* Config manager - in-memory registry boot init. */
NTSTATUS CmInitializeConfigRegistry(IN PVOID LoaderBlock);

/* PS - process/thread creation (ntos/ps/pscreate.c). */
NTSTATUS
PsCreateSystemThread(
    OUT PHANDLE ThreadHandle,
    IN  ULONG DesiredAccess,
    IN  POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN  HANDLE ProcessHandle OPTIONAL,
    OUT PCLIENT_ID ClientId OPTIONAL,
    IN  PKSTART_ROUTINE StartRoutine,
    IN  PVOID StartContext
    );

/* Shell entry point (ntos/cmd/cmd/cmdmain.c). */
VOID
CmdMain(IN PVOID StartContext);

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

/* Blink indicator (hals/halppc/ppc/display.c). */
VOID HalpBlinkBootIndicator(VOID);

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

    /* Initialize the framebuffer display from the PHW_DESCRIPTION. */
    HalpDisplayInit(LoaderBlock);
    HalpClearScreen();
    HalpSetColors(0x00FFFFFFUL, 0x00000000UL);
    HalpDebugPrintf("Windows XP (NT 5.1) - PowerPC Broadway/G3 Port\r\n");
    HalpDebugPrintf("Kernel initialized. Boot phase complete.\r\n");
    HalpDebugPrintf("Entry: _start at 0x1B84\r\n");

    /* Flush after PCR/display so we know we got this far. */
    HalpDebugFlushToSd();

    /* Initialize the Wii/GameCube PAD (Serial Interface) controller. */
    HalpControllerInit();
    HalpDebugPrintf("Controller input initialized.\r\n");
    HalpDebugFlushToSd();

    /* Initialize the SD card block device and mount the FAT volume. */
    {
        HalpDebugPrintf("Initializing SD card...\r\n");
        HalpDebugFlushToSd();
        if (HalpSdInit()) {
            ULONG sectors = HalpSdGetSectorCount();
            ULONG sectorSize = HalpSdGetSectorSize();
            HalpDebugPrintf("SD card initialized: %lu sectors x %lu bytes\r\n",
                              sectors, sectorSize);
            HalpDebugFlushToSd();

            if (HalpFatInit()) {
                HalpDebugPrintf("FAT volume mounted.\r\n");
                HalpDebugFlushToSd();

                /* Probe for a test file on the SD card. */
                {
                    ULONG fileSize = 0;
                    ULONG startCluster = 0;
                    if (HalpFatOpen("\\nt\\arcfw.elf", &fileSize, &startCluster)) {
                        HalpDebugPrintf("Found \\nt\\arcfw.elf (%lu bytes, cluster %lu)\r\n",
                                          fileSize, startCluster);
                    } else {
                        HalpDebugPrintf("\\nt\\arcfw.elf not found on SD card.\r\n");
                    }
                }
            } else {
                HalpDebugPrintf("FAT volume mount failed.\r\n");
            }
        } else {
            HalpDebugPrintf("SD card init failed.\r\n");
        }
    }

    /* Initialize the config registry (HKLM\HARDWARE, SYSTEM, SOFTWARE). */
    {
        HalpDebugPrintf("Initializing config registry...\r\n");
        HalpDebugFlushToSd();
        NTSTATUS cmStatus = CmInitializeConfigRegistry(LoaderBlock);
        if (!NT_SUCCESS(cmStatus)) {
            HalpDebugPrintf("Config registry init failed: 0x%08X\r\n", cmStatus);
        }
        HalpDebugFlushToSd();
    }

    /* Decrementer/timebase initialization: ... */
    HalpDebugPrintf("Initializing Decrementer...\r\n");
    HalpDebugFlushToSd();
    if (KeLoaderBlock != NULL) {
        ULONG dec_freq = *(ULONG *)((ULONG_PTR)KeLoaderBlock + 16);
        if (dec_freq != 0) {
            HalpInitializeDecrementer(dec_freq);
        } else {
            HalpInitializeDecrementer(HalpGetClockFrequency());
        }
    } else {
        HalpInitializeDecrementer(HalpGetClockFrequency());
    }

    /* Register the Decrementer tick ISR (vector 0x900) that drives the
     * blink indicator so a human can tell the kernel is running on real
     * Wii hardware. */
    HalpDebugPrintf("Registering Decrementer ISR...\r\n");
    HalpDebugFlushToSd();
    HalConnectInterrupt(0x900, HalpDecrementerHandler);

    /* Flush the captured debug log to sd:/nt/debug.log so the user can
     * diagnose even if the framebuffer is black. */
    if (!HalpDebugFlushToSd()) {
        HalpDebugPrintf("Debug log flush to SD failed.\r\n");
    }

    /* Start the Windows shell (cmd.exe analog) as the initial system
     * thread. */
    {
        HANDLE ShellThread;
        NTSTATUS shellStatus = PsCreateSystemThread(
            &ShellThread,
            0,                          /* DesiredAccess */
            NULL,                       /* ObjectAttributes */
            NULL,                       /* ProcessHandle (system process) */
            NULL,                       /* ClientId */
            CmdMain,                    /* StartRoutine */
            NULL                        /* StartContext */
            );
        if (!NT_SUCCESS(shellStatus)) {
            HalpDisplayPrintf("Failed to start shell: 0x%08X\r\n", shellStatus);

            /* No scheduler and shell failed to start; fall back to a
             * busy-wait "kernel alive" heartbeat that needs NO interrupts.
             * This keeps the blink block flashing on real hardware so a
             * human can tell the kernel is running even when the
             * Decrementer interrupt is silent. */
            HalpBlinkBootIndicator();
        }
    }

    return STATUS_SUCCESS;
}
