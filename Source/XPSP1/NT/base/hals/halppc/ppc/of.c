/*
 *  of.c - Open Firmware (IEEE 1275) client interface for Broadway/G3.
 *
 *  PowerPC HAL primitive for Windows XP (NT 5.1) port.
 *
 *  On classic PowerPC systems (and Broadway-class OpenFirmware platforms),
 *  firmware exposes device-tree properties through the OF client interface.
 *  This module provides:
 *    - HalpOFClientCall - generic OF client callback invocation
 *    - HalpOFGetProperty - fetch a property from the OF device tree
 *    - HalpGetClockFrequency - discover processor/bus clock from OF
 *    - HalpGetMemoryMap - walk the /memory "reg" property for the memory map
 *
 *  Build: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c of.c
 */

#include <stddef.h>
#include <string.h>

typedef long NTSTATUS;
typedef unsigned long ULONG;
typedef unsigned long ULONG_PTR;
typedef void VOID;
typedef int BOOLEAN;
typedef void *PVOID;
typedef unsigned long *PULONG;
typedef unsigned char UCHAR;
typedef char CHAR;

#define STATUS_SUCCESS      ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#define TRUE  1
#define FALSE 0

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif
#ifndef OPTIONAL
#define OPTIONAL
#endif

/*--------------------------------------------------------------------------
    Open Firmware client interface.

    A freestanding OF client program is entered by firmware with a pointer
    to the OF client services callback in a register (typically r3 on PPC
    OF clients).  We store that pointer here so later calls can locate the
    firmware.  Until the pointer is initialized (by the boot stub), all
    OF calls return STATUS_UNSUCCESSFUL and callers use compile-time
    defaults.
--------------------------------------------------------------------------*/

typedef struct _OF_CLIENT {
    /* Firmware-provided client services callback entry point. */
    VOID (*Callback)(VOID);
    /* Argument registers passed to the callback (a0-a7 / r3-r10). */
    ULONG Args[8];
} OF_CLIENT, *POF_CLIENT;

static OF_CLIENT HalpOFClient;
static BOOLEAN HalpOFClientInstalled = FALSE;

/* OF client service "phandle" argument indices (typical IEEE 1275 layout). */
#define OF_ARG_PHANDLE    0   /* 1st arg: package phandle */
#define OF_ARG_PROPNAME   1   /* 2nd arg: property name (or string) */
#define OF_ARG_PROPLEN    2   /* 3rd arg: prop buffer length */
#define OF_ARG_PROPBUF    3   /* 4th arg: prop buffer */
#define OF_ARG_RETLEN     4   /* 5th arg: actual length returned */

/* OF client service "call" method selector (firmware-specific). */
#define OF_METHOD_CALL        0x00000001   /* generic method call */
#define OF_METHOD_GETPROP     0x00000002   /* get a property */
#define OF_METHOD_CLAIM       0x00000003   /* claim memory */
#define OF_METHOD_CLOCKFREQ   0x00000004   /* clock-frequency */

/*--------------------------------------------------------------------------
    HalpOFSetClient - record the firmware client-services callback.
    Called once during phase-0 boot before any OF property access.
--------------------------------------------------------------------------*/
VOID
HalpOFSetClient(
    IN VOID (*Callback)(VOID)
    )
{
    HalpOFClient.Callback = Callback;
    HalpOFClientInstalled = TRUE;
}

/*--------------------------------------------------------------------------
    HalpOFClientCall - invoke an Open Firmware client service.

    Loads arguments into GPRs (r3-r8) and branches to the firmware-provided
    client-services callback (stored in CTR).  The callback returns its
    result in r3 (per the IEEE 1275 PowerPC client convention).  Returns 0
    (failure) when the client callback has not been installed by the boot
    stub.
--------------------------------------------------------------------------*/
static ULONG
HalpOFClientCall(
    IN ULONG Service,
    IN ULONG Arg1,
    IN ULONG Arg2,
    IN ULONG Arg3,
    IN ULONG Arg4,
    IN ULONG Arg5
    )
{
    register ULONG result asm("r3");

    if (!HalpOFClientInstalled || HalpOFClient.Callback == NULL) {
        return 0;
    }

    result = 0;
    __asm__ __volatile__ (
        "mr 3, %1\n\t"   /* Service selector */
        "mr 4, %2\n\t"   /* Arg1 */
        "mr 5, %3\n\t"   /* Arg2 */
        "mr 6, %4\n\t"   /* Arg3 */
        "mr 7, %5\n\t"   /* Arg4 */
        "mr 8, %6\n\t"   /* Arg5 */
        "mtctr %7\n\t"   /* Callback entry point into CTR */
        "bctrl\n\t"      /* Call callback; return value in r3 */
        : "+r" (result)
        : "r" (Service), "r" (Arg1), "r" (Arg2), "r" (Arg3), "r" (Arg4),
          "r" (Arg5), "r" (HalpOFClient.Callback)
        : "r4", "r5", "r6", "r7", "r8", "ctr", "lr", "memory"
    );

    return result;
}

/*--------------------------------------------------------------------------
    HalpOFGetProperty - fetch a property from the OF device tree.

    Invokes the OF "getprop" client service.  Returns TRUE if the property
    was found and copied into Buffer; FALSE otherwise.  When OF is not
    wired, returns FALSE so callers use compile-time defaults.
--------------------------------------------------------------------------*/
static BOOLEAN
HalpOFGetProperty(
    IN ULONG Phandle,
    IN const CHAR *PropName,
    OUT PVOID Buffer,
    IN ULONG BufferSize,
    OUT PULONG ActualSize OPTIONAL
    )
{
    ULONG ret;

    if (!HalpOFClientInstalled || HalpOFClient.Callback == NULL) {
        return FALSE;
    }

    /* OF "getprop" client service:
     *   r3 = service selector (OF_METHOD_GETPROP)
     *   r4 = phandle
     *   r5 = property name (string pointer)
     *   r6 = prop buffer length
     *   r7 = prop buffer
     *   r8 = actual length returned (pointer)
     * Returns non-zero in r3 if the property was found.
     */
    ret = HalpOFClientCall(
        OF_METHOD_GETPROP,
        Phandle,
        (ULONG)(unsigned long)PropName,
        BufferSize,
        (ULONG)(unsigned long)Buffer,
        ActualSize ? (ULONG)(unsigned long)ActualSize : 0
        );

    if (ret != 0) {
        if (ActualSize != NULL) {
            *ActualSize = ret;
        }
        return TRUE;
    }

    return FALSE;
}

/*--------------------------------------------------------------------------
    HalpGetClockFrequency - discover the processor/bus clock rate.

    Reads the "/cpus/PowerPC,G3@0" "clock-frequency" property (or the
    "timebase-frequency" property) from Open Firmware.  On Broadway/G3 the
    Decrementer decrements at bus_clock / 4, so this value is used to
    program SPR_DEC for the desired system-tick interval.

    Falls back to a compile-time default (485 MHz bus / 4 = 121 MHz DEC
    rate) if OF is not available.
--------------------------------------------------------------------------*/
ULONG
HalpGetClockFrequency(
    VOID
    )
{
    ULONG freq = 0;

    /* Try OF "/cpus/PowerPC,G3@0" clock-frequency. */
    if (HalpOFGetProperty(0x00000000, "clock-frequency", &freq, sizeof(freq), NULL)) {
        return freq;
    }

    /* Try "/cpus/PowerPC,G3@0" timebase-frequency as a secondary source. */
    if (HalpOFGetProperty(0x00000000, "timebase-frequency", &freq, sizeof(freq), NULL)) {
        return freq;
    }

    /* Fallback: a sane Broadway-class default.  Platforms should override. */
    return 122880000UL; /* 122.88 MHz default decrementer input rate */
}

/*--------------------------------------------------------------------------
    Memory map entry (one /memory "reg" range).
--------------------------------------------------------------------------*/
typedef struct _MEMORY_ENTRY {
    ULONG_PTR Address;
    ULONG_PTR Size;
    ULONG     Type; /* 1 = RAM, 2 = ROM, 3 = reserved, 4 = ACPI, etc. */
} MEMORY_ENTRY, *PMEMORY_ENTRY;

#define MAX_MEMORY_ENTRIES 16

static MEMORY_ENTRY HalpMemoryMap[MAX_MEMORY_ENTRIES];
static ULONG HalpMemoryMapCount = 0;

/*--------------------------------------------------------------------------
    HalpMemoryMapRegister - record an OF-discovered memory range.
    Called by HalpGetMemoryMap when it parses "/memory" reg.
--------------------------------------------------------------------------*/
VOID
HalpMemoryMapRegister(
    IN ULONG_PTR Address,
    IN ULONG_PTR Size,
    IN ULONG Type
    );

/*--------------------------------------------------------------------------
    HalpGetMemoryMap - fill in the physical memory map from OF "/memory".

    Walks the "reg" property of the /memory package and records each
    contiguous physical range.  Returns the number of entries filled.
    When OF is not wired, returns a minimal default: the first 64 MiB
    as usable RAM starting at 0x00000000 (the vector page lives at 0).
--------------------------------------------------------------------------*/
ULONG
HalpGetMemoryMap(
    OUT PMEMORY_ENTRY Map,
    IN  ULONG MaxEntries
    )
{
    ULONG i;

    /* If OF already populated the map, copy it out. */
    if (HalpMemoryMapCount != 0) {
        ULONG count = HalpMemoryMapCount < MaxEntries ? HalpMemoryMapCount : MaxEntries;
        for (i = 0; i < count; i++) {
            Map[i] = HalpMemoryMap[i];
        }
        return count;
    }

    /* Try to fetch the "/memory" "reg" property from OF.  The reg property
     * is an array of (address, size) pairs; each pair is 8 bytes on a 32-bit
     * OF client.  We walk it entry by entry and register each range. */
    {
        UCHAR regbuf[256];
        ULONG reallen = 0;
        ULONG n;
        BOOLEAN ok;

        ok = HalpOFGetProperty(
                 0x00000000, /* /memory package phandle (stub: root) */
                 "reg",
                 regbuf,
                 sizeof(regbuf),
                 &reallen
                 );

        if (ok && reallen >= 8 && Map != NULL && MaxEntries >= 1) {
            /* Each entry is 2 cells (address, size) = 8 bytes on 32-bit OF. */
            n = reallen / 8;
            if (n > MAX_MEMORY_ENTRIES) {
                n = MAX_MEMORY_ENTRIES;
            }
            for (i = 0; i < n; i++) {
                ULONG_PTR addr = *(ULONG_PTR *)(regbuf + i * 8);
                ULONG_PTR size = *(ULONG_PTR *)(regbuf + i * 8 + 4);
                HalpMemoryMapRegister(addr, size, 1); /* RAM */
            }
            HalpMemoryMapCount = n;
        }
    }

    /* If OF produced no entries, fall back to a minimal default map. */
    if (HalpMemoryMapCount == 0) {
        (void)MaxEntries;
        if (Map != NULL && MaxEntries >= 1) {
            Map[0].Address = 0x00000000UL;
            Map[0].Size    = 0x04000000UL; /* 64 MiB */
            Map[0].Type    = 1;            /* RAM */
            HalpMemoryMap[0] = Map[0];
            HalpMemoryMapCount = 1;
        }
        return HalpMemoryMapCount;
    }

    /* Copy the OF-populated map out to the caller. */
    {
        ULONG count = HalpMemoryMapCount < MaxEntries ? HalpMemoryMapCount : MaxEntries;
        for (i = 0; i < count; i++) {
            Map[i] = HalpMemoryMap[i];
        }
        return count;
    }
}

/*--------------------------------------------------------------------------
    HalpMemoryMapRegister - record an OF-discovered memory range.
    Called by HalpGetMemoryMap when it parses "/memory" reg.
--------------------------------------------------------------------------*/
VOID
HalpMemoryMapRegister(
    IN ULONG_PTR Address,
    IN ULONG_PTR Size,
    IN ULONG Type
    )
{
    if (HalpMemoryMapCount < MAX_MEMORY_ENTRIES) {
        HalpMemoryMap[HalpMemoryMapCount].Address = Address;
        HalpMemoryMap[HalpMemoryMapCount].Size    = Size;
        HalpMemoryMap[HalpMemoryMapCount].Type    = Type;
        HalpMemoryMapCount += 1;
    }
}

/*--------------------------------------------------------------------------
    HalpOFClientPresent - query whether the OF client callback is wired.
--------------------------------------------------------------------------*/
BOOLEAN
HalpOFClientPresent(
    VOID
    )
{
    return HalpOFClientInstalled;
}
