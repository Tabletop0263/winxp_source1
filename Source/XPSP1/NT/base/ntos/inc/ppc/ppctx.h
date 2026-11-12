/*++

Copyright (c) Microsoft Corporation

Module Name:

    ppctx.h

Abstract:

    The PowerPC (Broadway/G3) hardware specific header file for the
    Windows XP (NT 5.1) kernel port.

    Defines:
      - _KTRAP_FRAME - PowerPC trap frame (mirrors i386's KTRAP_FRAME
        concept, using PPC registers: GPRs, MSR, SRR0/SRR1, LR, CR,
        XER, CTR)
      - _KPCR / _KPRCB stubs for PPC
      - SPR numbers and MSR bit definitions

Author:

    (port)

Environment:

    Kernel mode only.

Revision History:

--*/

#ifndef _PPCTXH_
#define _PPCTXH_

#if defined(_NTDRIVER_) || defined(_NTDDK_) || defined(_NTIFS_) || defined(_NTHAL_) || defined(_NTOSP_) || defined(_BLDR_)
#error "ppcctx.h is for kernel/hal internal use only"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PowerPC Special Purpose Register numbers (classic PPC / G3).
 */
#define SPR_SRR0     0   /* Machine Status Save/Restore Register 0 */
#define SPR_SRR1     1   /* Machine Status Save/Restore Register 1 */
#define SPR_LR       8   /* Link Register (also via mflr/mtler) */
#define SPR_CTR      9   /* Count Register (also via mfctr/mtctr) */
#define SPR_XER      1   /* Fixed-Point Exception Register (also via mfxer/mtxer) */
#define SPR_DEC      22  /* Decrementer */
#define SPR_TB_U     268 /* TimeBase Upper */
#define SPR_TB_L     269 /* TimeBase Lower */
#define SPR_PVR      286 /* Processor Version Register */

/*
 * Machine State Register (MSR) bit definitions (classic PPC/G3).
 */
#define MSR_EE   0x00008000UL   /* External interrupt enable */
#define MSR_PR   0x00000010UL   /* Problem state (user=1, supervisor=0) */
#define MSR_FP   0x00000020UL   /* Floating point available */
#define MSR_ME   0x00000004UL   /* Machine check enable */
#define MSR_FE0  0x00000010UL   /* Floating point exception mode 0 */
#define MSR_FE1  0x00000020UL   /* Floating point exception mode 1 */
#define MSR_SE   0x00000002UL   /* Single-step trace enable */
#define MSR_RI   0x00001000UL   /* Recoverable interrupt */
#define MSR_IR   0x00000010UL   /* Instruction relocation */
#define MSR_DR   0x00000020UL   /* Data relocation */

/*
 * Exception / trap vector offsets (classic PowerPC, 0x100-aligned).
 */
#define PPC_VEC_RESET            0x000
#define PPC_VEC_MACHINE_CHECK    0x100
#define PPC_VEC_DATA_STORAGE     0x300
#define PPC_VEC_INST_STORAGE     0x400
#define PPC_VEC_EXTERNAL         0x500
#define PPC_VEC_ALIGNMENT        0x600
#define PPC_VEC_PROGRAM          0x700
#define PPC_VEC_FP_UNAVAILABLE   0x800
#define PPC_VEC_DECREMENTER      0x900
#define PPC_VEC_SYSCALL          0xC00
#define PPC_VEC_TRACE            0xD00
#define PPC_VEC_FP_ASSIST        0xE00

/*
 * Basic NT kernel types (self-contained subset).
 */
typedef long           NTSTATUS;
typedef unsigned long  ULONG;
typedef unsigned long  ULONG_PTR;
typedef unsigned char  UCHAR;
typedef unsigned short USHORT;
typedef long           LONG;
typedef void           VOID;
typedef void          *PVOID;
typedef char           CHAR;
typedef unsigned long  KAFFINITY;
typedef unsigned char  KIRQL;
typedef long           KPRIORITY;
typedef long           LONG_PTR;
typedef ULONG_PTR      SPFN_NUMBER;
typedef ULONG_PTR      PFN_NUMBER;

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
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
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

/*
 * Processor mode.
 */
typedef enum _KPROCESSOR_MODE {
    KernelMode = 0,
    UserMode   = 1
} KPROCESSOR_MODE, *PKPROCESSOR_MODE;

/*--------------------------------------------------------------------------
    KTRAP_FRAME - PowerPC trap frame

    Built by the HAL interrupt/exception entry code when a trap or
    exception occurs. Mirrors the i386 KTRAP_FRAME concept but uses
    PowerPC registers (GPRs r0-r31, SP=r1, LR, CR, XER, CTR, MSR,
    SRR0/SRR1).

    Layout (byte offsets):
      0x00  DbgEbp  (debug)
      0x04  DbgEip  (debug)
      0x08  DbgArgMark (debug)
      0x0c  DbgArgPointer (debug)
      0x10  TempSegCs
      0x14  TempEsp
      0x18  Gpr0  (volatile, caller-saved)
      0x1c  Gpr3
      0x20  Gpr4
      0x24  Gpr5
      0x28  Gpr6
      0x2c  Gpr7
      0x30  Gpr8
      0x34  Gpr9
      0x38  Gpr10
      0x3c  Gpr11
      0x40  Gpr12
      0x44  PreviousPreviousMode
      0x48  ExceptionList
      0x4c  SegFs   (PCR pointer)
      0x50  Gpr14   (non-volatile)
      0x54  Gpr15
      0x58  Gpr16
      0x5c  Gpr17
      0x60  Gpr18
      0x64  Gpr19
      0x68  Gpr20
      0x6c  Gpr21
      0x70  Gpr22
      0x74  Gpr23
      0x78  Gpr24
      0x7c  Gpr25
      0x80  Gpr26
      0x84  Gpr27
      0x88  Gpr28
      0x8c  Gpr29
      0x90  Gpr30
      0x94  Gpr31
      0x98  ErrCode
      0x9c  Iar     (instruction address / PC)
      0xa0  Msr
      0xa4  Lr
      0xa8  Cr
      0xac  Xer
      0xb0  Ctr
      0xb4  Srr0
      0xb8  Srr1
      0xbc  TrapNumber
      0xc0  PreviousMode
--------------------------------------------------------------------------*/

typedef struct _KTRAP_FRAME {
    ULONG   DbgEbp;
    ULONG   DbgEip;
    ULONG   DbgArgMark;
    ULONG   DbgArgPointer;
    ULONG   TempSegCs;
    ULONG   TempEsp;

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
    ULONG   Iar;
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

/*--------------------------------------------------------------------------
    KPRCB - PowerPC Processor Control Block (minimal stub).
--------------------------------------------------------------------------*/
struct _KPRCB {
    ULONG   MinorVersion;
    ULONG   MajorVersion;
    struct _KPCR *Prcb;
    struct _KPCR *SelfPcr;
    KAFFINITY SetMember;
    UCHAR   Number;
    UCHAR   Reserved;
    USHORT  BuildType;
    ULONG   PrcbData;                   /* placeholder */
    /* ... expanded in later phases ... */
};

/*--------------------------------------------------------------------------
    KPCR - PowerPC Processor Control Region (architecturally defined
    section only; mirrors i386's KPCR layout).

    On PowerPC the PCR is typically addressed via a fixed register or
    a known memory location (GPR r2 on some platforms, or a dedicated
    storage area). This header defines the layout; placement is
    platform/firmware dependent (Broadway OpenFirmware specifics TBD).
--------------------------------------------------------------------------*/

typedef struct _KPCR {
    struct _KPCR *SelfPcr;              /* flat address of this PCR   */
    struct _KPRCB *Prcb;                /* pointer to Prcb            */
    KIRQL   Irql;
    ULONG   IRR;
    ULONG   IrrActive;
    ULONG   IDR;
    PVOID   KdVersionBlock;

    /* Interrupt descriptor table (PPC equivalent: vector table) */
    PVOID   Idt;                         /* not used on PPC; reserved  */
    PVOID   Gdt;                         /* not used on PPC; reserved  */
    PVOID   Tss;                         /* not used on PPC; reserved  */

    USHORT  MajorVersion;
    USHORT  MinorVersion;
    KAFFINITY SetMember;
    ULONG   StallScaleFactor;
    UCHAR   DebugActive;
    UCHAR   Number;

    UCHAR   Spare0;
    UCHAR   SecondLevelCacheAssociativity;
    ULONG   VdmAlert;
    ULONG   KernelReserved[14];
    ULONG   SecondLevelCacheSize;
    ULONG   HalReserved[16];

    /* End of architecturally defined section */

    ULONG   InterruptMode;
    UCHAR   Spare1;
    ULONG   KernelReserved2[17];
    struct _KPRCB PrcbData;

    /* Exception dispatching: head of the thread's SEH registration chain.
     * On PowerPC there is no FS:[0]; the chain head lives here. */
    PVOID   ExceptionList;
} KPCR, *PKPCR;

/*--------------------------------------------------------------------------
    KeInitializeKernelPpc - PPC-specific kernel initialization entry.
    
    LoaderBlock is the PHW_DESCRIPTION passed by entii's arcldr; the HAL
    uses it to read clock frequency (DecrementerFrequency) and memory map.
--------------------------------------------------------------------------*/
NTSTATUS
KeInitializeKernelPpc(
    IN PVOID LoaderBlock
    );

/*--------------------------------------------------------------------------
    Inline SPR/MSR accessors (shared between HAL and kernel).
--------------------------------------------------------------------------*/

static __inline__ ULONG
HalpGetMSR(VOID)
{
    ULONG msr;
    __asm__ __volatile__ ("mfmsr %0" : "=r" (msr));
    return msr;
}

static __inline__ VOID
HalpSetMSR(IN ULONG Msr)
{
    __asm__ __volatile__ ("mtmsr %0" :: "r" (Msr));
    __asm__ __volatile__ ("isync");
}

static __inline__ ULONG
HalpGetSRR0(VOID)
{
    ULONG srr0;
    __asm__ __volatile__ ("mfspr %0, 0" : "=r" (srr0));
    return srr0;
}

static __inline__ VOID
HalpSetSRR0(IN ULONG Value)
{
    __asm__ __volatile__ ("mtspr 0, %0" :: "r" (Value));
}

static __inline__ ULONG
HalpGetSRR1(VOID)
{
    ULONG srr1;
    __asm__ __volatile__ ("mfspr %0, 1" : "=r" (srr1));
    return srr1;
}

static __inline__ VOID
HalpSetSRR1(IN ULONG Value)
{
    __asm__ __volatile__ ("mtspr 1, %0" :: "r" (Value));
}

static __inline__ ULONG
HalpGetLR(VOID)
{
    ULONG lr;
    __asm__ __volatile__ ("mflr %0" : "=r" (lr));
    return lr;
}

static __inline__ VOID
HalpSetLR(IN ULONG Value)
{
    __asm__ __volatile__ ("mtlr %0" :: "r" (Value));
}

static __inline__ ULONG
HalpGetCTR(VOID)
{
    ULONG ctr;
    __asm__ __volatile__ ("mfctr %0" : "=r" (ctr));
    return ctr;
}

static __inline__ VOID
HalpSetCTR(IN ULONG Value)
{
    __asm__ __volatile__ ("mtctr %0" :: "r" (Value));
}

static __inline__ ULONG
HalpGetXER(VOID)
{
    ULONG xer;
    __asm__ __volatile__ ("mfxer %0" : "=r" (xer));
    return xer;
}

static __inline__ VOID
HalpSetXER(IN ULONG Value)
{
    __asm__ __volatile__ ("mtxer %0" :: "r" (Value));
}

static __inline__ ULONG
HalpGetSPR(IN ULONG Spr)
{
    ULONG val;
    __asm__ __volatile__ ("mfspr %0, %1" : "=r" (val) : "r" (Spr));
    return val;
}

static __inline__ VOID
HalpSetSPR(IN ULONG Spr, IN ULONG Value)
{
    __asm__ __volatile__ ("mtspr %0, %1" :: "r" (Spr), "r" (Value));
}

/*
 * _M_PPC / _POWERPC macro definitions for conditional compilation.
 */
#if !defined(_M_PPC)
#define _M_PPC
#endif

#ifdef __cplusplus
}
#endif

#endif /* _PPCTXH_ */
