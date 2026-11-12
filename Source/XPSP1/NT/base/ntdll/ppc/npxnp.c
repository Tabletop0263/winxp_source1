/*++

Copyright (c) 2026  Microsoft Corporation

Module Name:

    npxnp.c

Abstract:

    PowerPC (Broadway/G3) stub for i386/npxnp.c.
    This module contains support for non-Flat mode NPX faults when
    the application has its CR0_EM bit clear.

    NOTE: PowerPC has no x87 NPX (no CR0_EM, no ES:[EIP] segment override).
    This is a porting stub that compiles standalone under
    powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c.

Environment:

    User Mode only

--*/

#include <stddef.h>
#include <stdint.h>

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
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

/*--------------------------------------------------------------------------
 * Minimal self-contained type definitions.
 *------------------------------------------------------------------------*/

#ifndef _PPC_NPXNP_TYPES_
#define _PPC_NPXNP_TYPES_

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
typedef unsigned char            BYTE;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* PowerPC context (minimal). The i386 NPX helpers read SegCs/Eip from a
 * CONTEXT; PowerPC has no SegCs/Eip, so we keep Iar (program counter) and
 * leave segment handling as a no-op stub. */
typedef struct _CONTEXT {
    ULONG64 IntA0;   /* r3 */
    ULONG64 IntA1;   /* r4 */
    ULONG64 IntA2;   /* r5 */
    ULONG64 IntA3;   /* r6 */
    ULONG64 IntA4;   /* r7 */
    ULONG64 IntGp;   /* r2 */
    ULONG64 Iar;     /* instruction address register (program counter) */
} CONTEXT, *PCONTEXT;

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) ((void)(P))
#endif

#endif /* _PPC_NPXNP_TYPES_ */

/*--------------------------------------------------------------------------
 * NpxNpReadCSEip
 *
 * i386: reads a byte from ES:[EIP] in the caller's context and advances EIP.
 * PowerPC stub: reads a byte from the address in Context->Iar (treating the
 * context's Iar as a flat linear address) and advances Iar by one.
 *
 * Returns the byte read.
 *------------------------------------------------------------------------*/

UCHAR
NpxNpReadCSEip (
    IN PCONTEXT Context
    )
{
    volatile UCHAR *p;
    UCHAR ibyte;

    /* Stub: treat Context->Iar as a flat address. No segment override on PPC. */
    p = (volatile UCHAR *)(uintptr_t)Context->Iar;
    ibyte = *p;
    Context->Iar += 1;   /* advance instruction pointer */
    return ibyte;
}

/*--------------------------------------------------------------------------
 * NpxNpSkipInstruction
 *
 * i386: moves the instruction pointer forward over the current NPX (x87 ESC)
 * instruction, accounting for prefixes, ModR/M, SIB and displacements.
 * PowerPC stub: there are no x87 ESC instructions on PowerPC; advance Iar
 * past a single NPX-like opcode as a no-op placeholder.
 *------------------------------------------------------------------------*/

VOID
NpxNpSkipInstruction (
    IN PCONTEXT Context
    )
{
    UCHAR ibyte;
    UCHAR Mod, rm;
    BOOLEAN fPrefix;
    BOOLEAN Address32Bits;

    Address32Bits = TRUE;     /* PowerPC is naturally 32-bit address capable */
    fPrefix = TRUE;

    /* Read prefixes (stub: limited set; real port must decode PPC instructions) */
    while (fPrefix) {
        ibyte = NpxNpReadCSEip(Context);
        switch (ibyte) {
            case 0x66:  /* operand size override (placeholder) */
            case 0x67:  /* address size override (placeholder) */
                break;
            default:
                fPrefix = FALSE;
                break;
        }
    }

    /* Handle first byte of NPX instruction (placeholder ESC range 0xD8-0xDF) */
    if (ibyte == 0x9B) {
        /* FWait equivalent - single byte, done */
        return;
    }

    if (ibyte < 0xD8 || ibyte > 0xDF) {
        /* Not an NPX ESC instruction */
        return;
    }

    /* Get ModR/M byte */
    ibyte = NpxNpReadCSEip(Context);
    if (ibyte > 0xBF) {
        return;
    }

    Mod = (UCHAR)(ibyte >> 6);
    rm  = (UCHAR)(ibyte & 0x7);

    if (Address32Bits) {
        static const UCHAR MOD32[] = { 0, 1, 4, 0 };
        Context->Iar += MOD32[Mod];
        if (Mod == 0 && rm == 5) {
            Context->Iar += 4;   /* disp32 */
        }
        if (rm == 4) {
            ibyte = NpxNpReadCSEip(Context);   /* SIB byte */
            if (Mod == 0 && (ibyte & 7) == 5) {
                Context->Iar += 4;   /* disp32 */
            }
        }
    } else {
        static const UCHAR MOD16[] = { 0, 1, 2, 0 };
        Context->Iar += MOD16[Mod];
        if (Mod == 0 && rm == 6) {
            Context->Iar += 2;   /* disp16 */
        }
    }

    UNREFERENCED_PARAMETER(Address32Bits);
}
