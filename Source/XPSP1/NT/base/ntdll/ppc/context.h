/*
 *  context.h - PowerPC (Broadway/G3) trap frame and context structures
 *
 *  NTDLL architecture header for Windows XP (NT 5.1) port to PowerPC G3.
 *
 *  Defines KTRAP_FRAME and CONTEXT layouts for PowerPC:
 *    GPRs r0-r31, SP (r1), LR, CR, XER, CTR, MSR, SRR0/SRR1
 *
 *  Build reference: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec
 */

#ifndef _PPC_CONTEXT_H_
#define _PPC_CONTEXT_H_

#include <stddef.h>

/* SAL annotations -- no-op on this target. */
#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif
#ifndef OPTIONAL
#define OPTIONAL
#endif

/* Basic NT types (self-contained; no DDK headers required). */
typedef long           NTSTATUS;
typedef unsigned long  ULONG;
typedef unsigned long  ULONG_PTR;
typedef unsigned long  DWORD;
typedef unsigned long long ULONGLONG;
typedef long           LONG;
typedef long long      LONGLONG;
typedef void           VOID;
typedef void          *PVOID;
typedef char           CHAR;
typedef unsigned char  UCHAR;
typedef unsigned short USHORT;
typedef int            BOOL;
typedef int            BOOLEAN;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* Processor mode */
typedef enum _KPROCESSOR_MODE {
    KernelMode = 0,
    UserMode   = 1
} KPROCESSOR_MODE;

typedef ULONG *PULONG;

/*
 * KTRAP_FRAME - PowerPC trap frame.
 *
 * Built by the low-level interrupt/exception entry code (halppc
 * interrupt vectors) when a trap/exception occurs. Mirrors the i386
 * KTRAP_FRAME concept but for PowerPC registers.
 *
 * Layout (byte offsets) - must match base/ntos/inc/ppc/ppctx.h and
 * base/hals/halppc/ppc/ppc.S (ppc.S writes these exact offsets):
 *
 *   0x00  DbgEbp  (debug)
 *   0x04  DbgEip  (debug)
 *   0x08  DbgArgMark (debug)
 *   0x0c  DbgArgPointer (debug)
 *   0x10  TempSegCs
 *   0x14  TempEsp (SP / r1 at trap time)
 *   0x18  Gpr0
 *   0x1c  Gpr3
 *   0x20  Gpr4
 *   0x24  Gpr5
 *   0x28  Gpr6
 *   0x2c  Gpr7
 *   0x30  Gpr8
 *   0x34  Gpr9
 *   0x38  Gpr10
 *   0x3c  Gpr11
 *   0x40  Gpr12
 *   0x44  PreviousPreviousMode
 *   0x48  ExceptionList
 *   0x4c  SegFs   (PCR pointer)
 *   0x50  Gpr14 ... 0x94 Gpr31
 *   0x98  ErrCode
 *   0x9c  Iar   (instruction address / return PC, copied from SRR0)
 *   0xa0  Msr
 *   0xa4  Lr
 *   0xa8  Cr
 *   0xac  Xer
 *   0xb0  Ctr
 *   0xb4  Srr0
 *   0xb8  Srr1
 *   0xbc  TrapNumber
 *   0xc0  PreviousMode
 *
 * KTRAP_FRAME_LENGTH = 0xc4
 */
typedef struct _KTRAP_FRAME {
    ULONG   DbgEbp;
    ULONG   DbgEip;
    ULONG   DbgArgMark;
    ULONG   DbgArgPointer;
    ULONG   TempSegCs;
    ULONG   TempEsp;          /* SP (r1) at trap time */

    /* Volatile / caller-saved GPRs (r0, r3-r12) */
    ULONG   Gpr0;
    ULONG   Gpr3;
    ULONG   Gpr4;
    ULONG   Gpr5;
    ULONG   Gpr6;
    ULONG   Gpr7;
    ULONG   Gpr8;
    ULONG   Gpr9;
    ULONG   Gpr10;
    ULONG   Gpr11;
    ULONG   Gpr12;

    ULONG   PreviousPreviousMode;
    PVOID   ExceptionList;
    PVOID   SegFs;

    /* Non-volatile / callee-saved GPRs (r14-r31) */
    ULONG   Gpr14;
    ULONG   Gpr15;
    ULONG   Gpr16;
    ULONG   Gpr17;
    ULONG   Gpr18;
    ULONG   Gpr19;
    ULONG   Gpr20;
    ULONG   Gpr21;
    ULONG   Gpr22;
    ULONG   Gpr23;
    ULONG   Gpr24;
    ULONG   Gpr25;
    ULONG   Gpr26;
    ULONG   Gpr27;
    ULONG   Gpr28;
    ULONG   Gpr29;
    ULONG   Gpr30;
    ULONG   Gpr31;

    /* Control registers */
    ULONG   ErrCode;
    ULONG   Iar;              /* instruction address / PC (from SRR0) */
    ULONG   Msr;
    ULONG   Lr;
    ULONG   Cr;
    ULONG   Xer;
    ULONG   Ctr;
    ULONG   Srr0;
    ULONG   Srr1;
    ULONG   TrapNumber;
    KPROCESSOR_MODE PreviousMode;
} KTRAP_FRAME, *PKTRAP_FRAME;

#define KTRAP_FRAME_LENGTH  (sizeof(KTRAP_FRAME))
#define KTRAP_FRAME_ALIGN   (sizeof(ULONG))

/*
 * CONTEXT - PowerPC thread context (for Get/SetThreadContext,
 * exception dispatch, context switching).
 *
 * Mirrors the i386 CONTEXT but uses PowerPC registers. No x87/MMX/
 * segment registers exist on PowerPC.
 */
#define CONTEXT_POWERPC      0x00010000L
#define CONTEXT_CONTROL      (CONTEXT_POWERPC | 0x00000001L)   /* MSR, SRR0/SRR1, LR, CR, XER, CTR */
#define CONTEXT_INTEGER      (CONTEXT_POWERPC | 0x00000002L)   /* GPRs r0-r31 */
#define CONTEXT_FULL         (CONTEXT_CONTROL | CONTEXT_INTEGER)

typedef struct _CONTEXT {
    ULONG   ContextFlags;        /* which parts are valid */
    ULONG   Msr;                 /* machine state register */
    ULONG   Srr0;                /* save/restore r0? no - return PC */
    ULONG   Srr1;                /* return MSR */
    ULONG   Lr;                  /* link register */
    ULONG   Cr;                  /* condition register */
    ULONG   Xer;                 /* fixed-point exception */
    ULONG   Ctr;                 /* count register */
    ULONG   Iar;                 /* instruction address (PC) */
    ULONG   Gpr0;
    ULONG   Gpr1;                /* stack pointer */
    ULONG   Gpr2;
    ULONG   Gpr3;
    ULONG   Gpr4;
    ULONG   Gpr5;
    ULONG   Gpr6;
    ULONG   Gpr7;
    ULONG   Gpr8;
    ULONG   Gpr9;
    ULONG   Gpr10;
    ULONG   Gpr11;
    ULONG   Gpr12;
    ULONG   Gpr13;
    ULONG   Gpr14;
    ULONG   Gpr15;
    ULONG   Gpr16;
    ULONG   Gpr17;
    ULONG   Gpr18;
    ULONG   Gpr19;
    ULONG   Gpr20;
    ULONG   Gpr21;
    ULONG   Gpr22;
    ULONG   Gpr23;
    ULONG   Gpr24;
    ULONG   Gpr25;
    ULONG   Gpr26;
    ULONG   Gpr27;
    ULONG   Gpr28;
    ULONG   Gpr29;
    ULONG   Gpr30;
    ULONG   Gpr31;
} CONTEXT, *PCONTEXT;

/*
 * Exception record (minimal, self-contained).
 */
typedef struct _EXCEPTION_RECORD {
    NTSTATUS   ExceptionCode;
    ULONG      ExceptionFlags;
    struct _EXCEPTION_RECORD *ExceptionRecord;
    PVOID      ExceptionAddress;
    ULONG      NumberParameters;
    ULONG_PTR  ExceptionInformation[15];
} EXCEPTION_RECORD, *PEXCEPTION_RECORD;

/* Exception codes */
#define STATUS_ACCESS_VIOLATION      ((NTSTATUS)0xC0000005L)
#define STATUS_DATATYPE_MISALIGNMENT ((NTSTATUS)0x80000002L)
#define STATUS_BREAKPOINT            ((NTSTATUS)0x80000003L)
#define STATUS_SINGLE_STEP           ((NTSTATUS)0x80000004L)
#define STATUS_ILLEGAL_INSTRUCTION   ((NTSTATUS)0xC000001DL)

#endif /* _PPC_CONTEXT_H_ */
