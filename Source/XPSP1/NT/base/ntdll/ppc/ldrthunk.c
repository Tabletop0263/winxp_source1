/*++

Copyright (c) 2026  Microsoft Corporation

Module Name:

    ldrthunk.c

Abstract:

    PowerPC (Broadway/G3) stub for i386/ldrthunk.asm.
    This module implements the thunk for the LdrpInitialize APC routine.

    NOTE: This is a porting stub. It compiles standalone under
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

#ifndef _PPC_LDRTHUNK_TYPES_
#define _PPC_LDRTHUNK_TYPES_

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

typedef struct _CONTEXT {
    ULONG64 IntA0;   /* r3 */
    ULONG64 IntA1;   /* r4 */
    ULONG64 IntA2;   /* r5 */
    ULONG64 IntA3;   /* r6 */
    ULONG64 IntA4;   /* r7 */
    ULONG64 IntGp;   /* r2 */
    ULONG64 Iar;     /* instruction address register (program counter) */
} CONTEXT, *PCONTEXT;

#endif /* _PPC_LDRTHUNK_TYPES_ */

/* Forward declaration mirrored from ntdll. */
VOID LdrpInitialize(PVOID NormalContext, PVOID SystemArgument1,
                    PVOID SystemArgument2);

/* LdrpInitialize is not ported yet; provide a stub so ldrthunk.c links. */
VOID
LdrpInitialize(
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
{
    /* Stub: not implemented in this port. */
    (void)NormalContext;
    (void)SystemArgument1;
    (void)SystemArgument2;
    return;
}

/*--------------------------------------------------------------------------
 * LdrInitializeThunk
 *
 * PowerPC stub. On i386 this thunk computes a pointer to the context record
 * on the stack and jumps to LdrpInitialize with that pointer as its
 * parameter. On PowerPC there is no fixed stack-layout context record to
 * compute in the same way, so this stub simply forwards to LdrpInitialize
 * with a NULL context (the caller must supply the real context when porting
 * the full frame walk).
 *------------------------------------------------------------------------*/

VOID
LdrInitializeThunk(
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2,
    IN PCONTEXT Context OPTIONAL
    )
{
    /* NormalContext, SystemArgument1, SystemArgument2 are ignored on i386. */
    LdrpInitialize(Context, SystemArgument1, SystemArgument2);
}
