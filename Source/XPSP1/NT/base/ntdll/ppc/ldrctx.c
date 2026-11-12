/*++

Copyright (c) 2026  Microsoft Corporation

Module Name:

    ldrctx.c

Abstract:

    PowerPC (Broadway/G3) stub for i386/ldrctx.c.
    This module contains support for relocating executables.

    NOTE: This is a porting stub. It defines the PowerPC context
    relocation primitives and compiles standalone under
    powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c.

Environment:

    User Mode only

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
 * Minimal self-contained type definitions (no external headers required).
 *------------------------------------------------------------------------*/

#ifndef _PPC_STD_TYPES_
#define _PPC_STD_TYPES_

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

/* PowerPC GPR file mapping (r0..r31).
 * IntA0..IntA4 correspond to argument/return GPRs r3..r7.
 * IntGp corresponds to r2 (TOC/global pointer base on PowerPC). */
typedef struct _CONTEXT {
    ULONG64 IntA0;   /* r3 */
    ULONG64 IntA1;   /* r4 */
    ULONG64 IntA2;   /* r5 */
    ULONG64 IntA3;   /* r6 */
    ULONG64 IntA4;   /* r7 */
    ULONG64 IntGp;   /* r2 */
    ULONG64 Iar;     /* instruction address register (program counter) */
} CONTEXT, *PCONTEXT;

#endif /* _PPC_STD_TYPES_ */

extern VOID CorExeMain(VOID);

/* CorExeMain is not ported yet; provide a stub so ldrctx.c links. */
VOID
CorExeMain(VOID)
{
    /* Stub: not implemented in this port. */
    return;
}

VOID
LdrpRelocateStartContext (
    IN PCONTEXT Context,
    IN LONG_PTR Diff
    )
/*++

Routine Description:

   PowerPC stub. This routine relocates the start context to mesh with the
   executable that has just been relocated.

   On PowerPC the entry/return value is carried in IntA0 (r3), so the
   relocation delta is applied there (mirroring the alpha port which
   relocates IntA0/IntGp). The i386 port relocates Eax.

Arguments:

   Context - Supplies a context that needs editing.

   Diff - Supplies the difference from the based address to the relocated
          address.

Return Value:

   None.

--*/
{
    Context->IntA0 += (ULONG64)Diff;
}

VOID
LdrpCorReplaceStartContext (
    IN PCONTEXT Context
    )
/*++

Routine Description:

   PowerPC stub. This routine replaces the initial address to run by one in
   mscoree.dll.

   On PowerPC the entry address is carried in IntA0 (r3).

Arguments:

   Context - Supplies a context that needs editing.

Return Value:

   None.

--*/
{
    Context->IntA0 = (ULONG64)(ULONG_PTR)CorExeMain;
}
