/*++

Copyright (c) Microsoft Corporation

Module Name:

    critsect.c

Abstract:

    PowerPC (Broadway/G3) implementation of user mode critical sections
    (RtlEnterCriticalSection, RtlLeaveCriticalSection, RtlTryEnterCriticalSection).

    Interlocking uses the PowerPC lwarx/stwcx. load-reserve/store-conditional
    pair for SMP-safe atomic read-modify-write on the G3 core.

    NOTE: Compiles standalone under
    powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c.

Environment:

    Any mode.

--*/

#include <stddef.h>

/* SAL annotations (Windows source-code analysis) -- no-op on this target. */
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
 * Minimal self-contained type definitions.
 *------------------------------------------------------------------------*/

typedef void                     VOID;
typedef char                     CHAR;
typedef unsigned char            UCHAR;
typedef unsigned short           USHORT;
typedef unsigned long            ULONG;
typedef unsigned long long       ULONG64;
typedef long                     LONG;
typedef long long                LONG64;
typedef unsigned long            DWORD;
typedef unsigned long            ULONG_PTR;
typedef long long                LONG_PTR;
typedef unsigned long long       ULONGLONG;
typedef void                    *PVOID;
typedef void                    *HANDLE;
typedef int                      BOOL;
typedef int                      NTSTATUS;
typedef int                      BOOLEAN;
typedef long                     LONG32;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0)
#endif

/* RTL_CRITICAL_SECTION (minimal, PowerPC layout) */
typedef struct _RTL_CRITICAL_SECTION {
    LONG   LockCount;
    LONG   RecursionCount;
    HANDLE OwningThread;
    ULONG  SpinCount;
} RTL_CRITICAL_SECTION, *PRTL_CRITICAL_SECTION;

/*--------------------------------------------------------------------------
 * PowerPC atomic primitives (lwarx/stwcx.)
 *------------------------------------------------------------------------*/

static __inline__ LONG
InterlockedIncrementPPC(volatile LONG *Addend)
{
    LONG new;

    __asm__ __volatile__ (
        "1: lwarx   %0, 0, %2\n"
        "   addi   %0, %0, 1\n"
        "   stwcx. %0, 0, %2\n"
        "   bne-   1b\n"
        : "=&r" (new), "+m" (*Addend)
        : "r" (Addend)
        : "cc", "memory"
    );

    return new;
}

static __inline__ LONG
InterlockedDecrementPPC(volatile LONG *Addend)
{
    LONG new;

    __asm__ __volatile__ (
        "1: lwarx   %0, 0, %2\n"
        "   addi   %0, %0, -1\n"
        "   stwcx. %0, 0, %2\n"
        "   bne-   1b\n"
        : "=&r" (new), "+m" (*Addend)
        : "r" (Addend)
        : "cc", "memory"
    );

    return new;
}

static __inline__ LONG
InterlockedCompareExchangePPC(volatile LONG *Destination,
                              LONG Exchange,
                              LONG Comperand)
{
    LONG old;

    __asm__ __volatile__ (
        "1: lwarx   %0, 0, %2\n"
        "   cmpw    %0, %3\n"
        "   bne-    2f\n"
        "   stwcx.  %4, 0, %2\n"
        "   bne-    1b\n"
        "2:\n"
        : "=&r" (old), "+m" (*Destination)
        : "r" (Destination), "r" (Comperand), "r" (Exchange)
        : "cc", "memory"
    );

    return old;
}

/*--------------------------------------------------------------------------
 * RtlpWaitForCriticalSection / RtlpUnWaitCriticalSection stubs.
 *------------------------------------------------------------------------*/

NTSTATUS
RtlpWaitForCriticalSection(PRTL_CRITICAL_SECTION CriticalSection)
{
    (void)CriticalSection;
    /* Stub: would block on a kernel event / spinlock. */
    return STATUS_SUCCESS;
}

NTSTATUS
RtlpUnWaitCriticalSection(PRTL_CRITICAL_SECTION CriticalSection)
{
    (void)CriticalSection;
    /* Stub: would wake a waiting thread. */
    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
 * RtlEnterCriticalSection
 *------------------------------------------------------------------------*/

NTSTATUS
RtlEnterCriticalSection(
    IN PRTL_CRITICAL_SECTION CriticalSection
    )
{
    HANDLE Thread;

    Thread = (HANDLE)0;   /* stub: no TEB available; current thread unknown */

    if (Thread == CriticalSection->OwningThread) {
        InterlockedIncrementPPC(&CriticalSection->LockCount);
        CriticalSection->RecursionCount += 1;
        return STATUS_SUCCESS;
    }

    if (CriticalSection->SpinCount != 0) {
        do {
            if (CriticalSection->LockCount == -1L) {
                if (InterlockedCompareExchangePPC(&CriticalSection->LockCount,
                                                  0L,
                                                  -1L) == -1L) {
                    CriticalSection->OwningThread = Thread;
                    CriticalSection->RecursionCount = 1;
                    return STATUS_SUCCESS;
                }
            } else if (CriticalSection->LockCount > 0L) {
                break;
            }
            CriticalSection->SpinCount -= 1;
        } while (CriticalSection->SpinCount != 0);
    }

    if (InterlockedIncrementPPC(&CriticalSection->LockCount) != 0) {
        RtlpWaitForCriticalSection(CriticalSection);
    }

    CriticalSection->OwningThread = Thread;
    CriticalSection->RecursionCount = 1;
    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
 * RtlLeaveCriticalSection
 *------------------------------------------------------------------------*/

NTSTATUS
RtlLeaveCriticalSection(
    IN PRTL_CRITICAL_SECTION CriticalSection
    )
{
    if ((CriticalSection->RecursionCount -= 1) == 0) {
        CriticalSection->OwningThread = NULL;
        if (InterlockedDecrementPPC(&CriticalSection->LockCount) >= 0) {
            RtlpUnWaitCriticalSection(CriticalSection);
        }
    }

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
 * RtlTryEnterCriticalSection
 *------------------------------------------------------------------------*/

BOOLEAN
RtlTryEnterCriticalSection (
    IN PRTL_CRITICAL_SECTION CriticalSection
    )
{
    HANDLE Thread;

    Thread = (HANDLE)0;

    if (Thread == CriticalSection->OwningThread) {
        InterlockedIncrementPPC(&CriticalSection->LockCount);
        CriticalSection->RecursionCount += 1;
        return TRUE;
    }

    if (InterlockedCompareExchangePPC(&CriticalSection->LockCount,
                                      0L,
                                      -1L) == -1L) {
        CriticalSection->OwningThread = Thread;
        CriticalSection->RecursionCount = 1;
        return TRUE;
    } else {
        return FALSE;
    }
}
