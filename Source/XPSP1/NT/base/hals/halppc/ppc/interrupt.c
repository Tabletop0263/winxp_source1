/*
 *  interrupt.c - PowerPC (Broadway/G3) interrupt dispatch
 *
 *  PowerPC HAL primitive for Windows XP (NT 5.1) port.
 *
 *  Implements HalBeginInterrupt, HalEndInterrupt, HalConnectInterrupt,
 *  HalpInstallVectors, and HalpInterruptEntry. HalpInterruptEntry now
 *  builds a PowerPC KTRAP_FRAME and dispatches to the kernel exception
 *  dispatcher (KiExceptionDispatch in ntos/ppc/exceptn.c), closing the
 *  HAL -> kernel exception path.
 *
 *  Build: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c interrupt.c
 */

#include <stddef.h>

/* KTRAP_FRAME, SPR/MSR accessors, and vector offsets shared with ntos/ppc */
#include "ppctx.h"

/* OpenFirmware clock discovery (of.c) */
ULONG HalpGetClockFrequency(VOID);

typedef unsigned long ULONG;
typedef unsigned long ULONG_PTR;
typedef void VOID;
typedef int BOOLEAN;

#define TRUE  1
#define FALSE 0

/* Machine State Register (MSR) bit definitions for PowerPC */
#define MSR_EE   0x00008000UL   /* External interrupt enable */
#define MSR_RI   0x00001000UL   /* Recoverable interrupt */
#define MSR_PR   0x00000010UL   /* Problem state */
#define MSR_FP   0x00000020UL   /* Floating point available */
#define MSR_ME   0x00000004UL   /* Machine check enable */

/* Special Purpose Register access helpers */
static __inline__ ULONG __get_MSR(void)
{
    ULONG msr;
    __asm__ __volatile__ ("mfmsr %0" : "=r" (msr));
    return msr;
}

static __inline__ void __set_MSR(ULONG msr)
{
    __asm__ __volatile__ ("mtmsr %0" :: "r" (msr));
}

static __inline__ ULONG __get_SRR0(void)
{
    ULONG srr0;
    __asm__ __volatile__ ("mfspr %0, 0" : "=r" (srr0)); /* SRR0 = SPR 0 */
    return srr0;
}

static __inline__ ULONG __get_SRR1(void)
{
    ULONG srr1;
    __asm__ __volatile__ ("mfspr %0, 1" : "=r" (srr1)); /* SRR1 = SPR 1 */
    return srr1;
}

static __inline__ void __set_SRR0(ULONG val)
{
    __asm__ __volatile__ ("mtspr 0, %0" :: "r" (val));
}

static __inline__ void __set_SRR1(ULONG val)
{
    __asm__ __volatile__ ("mtspr 1, %0" :: "r" (val));
}

/*
 * Forward declaration: kernel exception dispatcher (ntos/ppc/exceptn.c).
 * Called by HalpInterruptEntry with a fully populated KTRAP_FRAME.
 */
VOID KiExceptionDispatch(IN PKTRAP_FRAME TrapFrame);

/* Decrementer / Timebase SPR */
#define SPR_DECREMENTER  22

/* PowerPC exception vector numbers (from ppctx.h / ppc.S). */
#define PPC_VEC_DECREMENTER  0x900   /* Decrementer timer tick */

/*
 * Per-CPU interrupt state tracking.
 * In a real port this would be in the PCR (Processor Control Region).
 */
static ULONG HalpInterruptCount = 0;
static BOOLEAN HalpInterruptsEnabled = FALSE;

/*
 * HalpInterruptDispatchTable - array of interrupt service routines.
 * Indexed by interrupt vector number. Null entries are unhandled.
 */
#define MAX_INTERRUPT_VECTORS 256
typedef VOID (*ISR_HANDLER)(VOID);
static ISR_HANDLER HalpInterruptDispatchTable[MAX_INTERRUPT_VECTORS] = { 0 };

/*
 * HalpDefaultInterruptHandler - catch-all for unhandled interrupts.
 */
static VOID
HalpDefaultInterruptHandler(VOID)
{
    /* Stubs: log and ignore in this port */
    HalpInterruptCount++;
}

/*
 * HalpDecrementerHandler - ISR for the Decrementer vector (0x900).
 *
 * The Decrementer fires at ~100 Hz.  On each tick we:
 *   1. Reload the Decrementer so it keeps firing periodically.
 *   2. Draw the blink indicator (top-left corner block toggles ~once/sec)
 *      so a human can tell the kernel is running.
 *
 * This deliberately does NOT call KiExceptionDispatch: timer ticks are
 * not exceptions and would otherwise trigger a bugcheck.
 */
VOID
HalpDecrementerHandler(VOID)
{
    /* Reload the Decrementer for the next tick (~10 ms at 100 Hz). */
    HalpSetSPR(SPR_DECREMENTER, 0x00989680UL);   /* 10,000,000 @ 1 GHz bus/4 */

    /* Draw the blink indicator (toggles ~once per second internally). */
    HalpDisplayTickBlink();

    HalpInterruptCount++;
}

/*
 * HalConnectInterrupt - register an ISR for a given vector.
 * Returns TRUE if the slot was free.
 */
BOOLEAN
HalConnectInterrupt(ULONG Vector, ISR_HANDLER Handler)
{
    if (Vector >= MAX_INTERRUPT_VECTORS)
        return FALSE;

    if (HalpInterruptDispatchTable[Vector] != 0)
        return FALSE;

    HalpInterruptDispatchTable[Vector] = Handler;
    return TRUE;
}

/*
 * HalBeginInterrupt - mask interrupts and record entry.
 * Called at the start of interrupt processing.
 * Returns the previous interrupt state (TRUE if interrupts were enabled).
 */
BOOLEAN
HalBeginInterrupt(VOID)
{
    BOOLEAN wasEnabled = HalpInterruptsEnabled;

    /* Disable external interrupts by clearing MSR.EE */
    if (wasEnabled) {
        ULONG msr = __get_MSR();
        msr &= ~MSR_EE;
        __set_MSR(msr);
        HalpInterruptsEnabled = FALSE;
    }

    HalpInterruptCount++;
    return wasEnabled;
}

/*
 * HalEndInterrupt - end interrupt processing, optionally re-enable interrupts.
 * If PreviousState was TRUE, re-enable MSR.EE.
 */
VOID
HalEndInterrupt(BOOLEAN PreviousState)
{
    HalpInterruptCount--;

    if (PreviousState && !HalpInterruptsEnabled) {
        ULONG msr = __get_MSR();
        msr |= MSR_EE;
        __set_MSR(msr);
        HalpInterruptsEnabled = TRUE;
    }
}

/*
 * HalpInterruptEntry - common interrupt entry point.
 *
 * Called from the low-level vector wrapper (HalpVectorEntry in ppc.S)
 * which has already built a KTRAP_FRAME on the stack and passes its
 * pointer as the second argument. This routine:
 *   1. Calls HalBeginInterrupt
 *   2. Dispatches to the registered ISR
 *   3. Calls KiExceptionDispatch(TrapFrame) to hand the trap frame to
 *      the kernel exception dispatcher
 *   4. Calls HalEndInterrupt
 *
 * The rfi (return from interrupt) is emitted by the vector wrapper.
 */
VOID
HalpInterruptEntry(IN ULONG Vector, IN PKTRAP_FRAME TrapFrame)
{
    ISR_HANDLER handler;
    BOOLEAN prev;

    (void)TrapFrame; /* frame is built by the vector wrapper; passed to
                        KiExceptionDispatch once kernel dispatch is wired */

    prev = HalBeginInterrupt();

    /* Decrementer vector (0x900): timer tick drives the blink indicator.
     * Timer ticks are not exceptions - do NOT call KiExceptionDispatch.
     * Route directly to HalpDecrementerHandler instead of the dispatch
     * table / default handler path. */
    if (Vector == PPC_VEC_DECREMENTER) {
        HalpDecrementerHandler();
        HalEndInterrupt(prev);
        return;
    }

    if (Vector < MAX_INTERRUPT_VECTORS) {
        handler = HalpInterruptDispatchTable[Vector];
    } else {
        handler = 0;
    }

    if (handler != 0) {
        handler();
    } else {
        HalpDefaultInterruptHandler();
    }

    /* Hand the trap frame to the kernel exception dispatcher. */
    KiExceptionDispatch(TrapFrame);

    HalEndInterrupt(prev);
}

/*
 * HalpInstallVectors - install the PowerPC interrupt vector table.
 *
 * On the classic PowerPC/G3, exceptions vector to fixed addresses
 * starting at 0x000. On Book E, IVORs are used. This routine
 * programs SRR0/SRR1 and the external vector base so that
 * interrupts route to HalpInterruptEntry.
 *
 * In this port, it is a setup stub: it records the entry address
 * and enables MSR.EE. Actual vector table relocation requires
 * privileged access to the IVPR/IVOR registers or the 0x000
 * exception page, which is platform/firmware dependent.
 */
VOID
HalpInstallVectors(VOID (*Entry)(ULONG))
{
    /* Stubs: record entry point address */
    (void)Entry;

    /* Enable external interrupts globally */
    ULONG msr = __get_MSR();
    msr |= MSR_EE | MSR_ME;
    __set_MSR(msr);
    HalpInterruptsEnabled = TRUE;
}

/*--------------------------------------------------------------------------
    HalpInitializeDecrementer - program the Decrementer interval timer.

    Reads the processor/bus clock rate from Open Firmware (via
    HalpGetClockFrequency) and programs SPR_DEC so the Decrementer fires
    at the desired system-tick interval (10 ms = 100 Hz on Broadway/G3).
    On classic PowerPC the Decrementer decrements at bus_clock / 4.

    Then enables external interrupts (MSR.EE) so the Decrementer interrupt
    (vector 0x900) can fire.
--------------------------------------------------------------------------*/
VOID
HalpInitializeDecrementer(IN ULONG DecrementerFrequency)
{
    /* Desired system tick: 100 Hz (10 ms). */
    const ULONG TICKS_PER_SECOND = 100;

    /* Discover the decrementer input clock from Open Firmware / loader block. */
    ULONG dec_clk = DecrementerFrequency;
    if (dec_clk == 0) {
        dec_clk = HalpGetClockFrequency();
    }

    /* Compute the reload value: dec_clk / ticks_per_second.
     * Guard against divide-by-zero / implausible rates. */
    ULONG reload;
    if (dec_clk == 0 || dec_clk < TICKS_PER_SECOND) {
        reload = 0x00FFFFFFUL; /* max interval: timer effectively off */
    } else {
        reload = dec_clk / TICKS_PER_SECOND;
    }

    /* Program SPR_DEC. */
    HalpSetSPR(SPR_DEC, reload);

    /* Enable external interrupts so the Decrementer interrupt fires. */
    ULONG msr = __get_MSR();
    msr |= MSR_EE | MSR_ME;
    __set_MSR(msr);
    HalpInterruptsEnabled = TRUE;
}
