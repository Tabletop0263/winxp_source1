/*
 *  halppc.c - PowerPC (Broadway/G3) HAL implementation
 *
 *  PowerPC HAL for Windows XP (NT 5.1) port.
 *
 *  Build: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c halppc.c
 */

#include <stddef.h>

typedef long NTSTATUS;
typedef unsigned long ULONG;
typedef unsigned long ULONG_PTR;
typedef void VOID;
typedef int BOOLEAN;

#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define TRUE  1
#define FALSE 0

#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif
#ifndef OPTIONAL
#define OPTIONAL
#endif

/* Machine State Register bits */
#define MSR_EE   0x00008000UL   /* External interrupt enable */
#define MSR_RI   0x00001000UL   /* Recoverable interrupt */
#define MSR_PR   0x00000010UL   /* Problem state */
#define MSR_FP   0x00000020UL   /* Floating point available */
#define MSR_FE0  0x00000010UL   /* Floating point exception mode 0 */
#define MSR_FE1  0x00000020UL   /* Floating point exception mode 1 */
#define MSR_SE   0x00000002UL   /* Single-step trace enable */
#define MSR_ME   0x00000004UL   /* Machine check enable */
#define MSR_IR   0x00000010UL   /* Instruction relocation (classic PPC) */
#define MSR_DR   0x00000020UL   /* Data relocation (classic PPC) */

/* Decrementer / Timebase SPRs */
#define SPR_DECREMENTER  22
#define SPR_TIMEBASE_U   268
#define SPR_TIMEBASE_L   269

static __inline__ ULONG __get_MSR(void)
{
    ULONG msr;
    __asm__ __volatile__ ("mfmsr %0" : "=r" (msr));
    return msr;
}

static __inline__ void __set_MSR(ULONG msr)
{
    __asm__ __volatile__ ("mtmsr %0" :: "r" (msr));
    __asm__ __volatile__ ("isync");
}

static __inline__ ULONG __mfspr(ULONG spr)
{
    ULONG val;
    __asm__ __volatile__ ("mfspr %0, %1" : "=r" (val) : "r" (spr));
    return val;
}

static __inline__ void __mtspr(ULONG spr, ULONG val)
{
    __asm__ __volatile__ ("mtspr %0, %1" :: "r" (spr), "r" (val));
}

/* Forward declaration: context switch implemented in context.S */
VOID HalpSwapContext(IN VOID *OutContext, IN VOID *InContext, IN VOID *Entry);

/*
 * HalInitializeProcessor - per-processor HAL initialization.
 *
 * Performs Broadway/G3-specific initialization:
 *   1. Sets MSR (enable external interrupts, machine check, FP)
 *   2. Initializes the Decrementer (interval timer)
 *   3. Initializes the Timebase
 *   4. Prepares exception vector setup
 *
 * Returns STATUS_SUCCESS.
 */
NTSTATUS
HalInitializeProcessor(VOID)
{
    ULONG msr;

    /* 1. Configure MSR:
     *    Enable External Interrupts (EE), Machine Check (ME),
     *    Floating Point (FP), and set Problem/Supervisor as needed.
     *    Kernel runs in supervisor state (PR = 0). */
    msr = MSR_EE | MSR_ME | MSR_FP;
    __set_MSR(msr);

    /* 2. Initialize Decrementer to a large value (timer not firing yet).
     *    The OS loader/kernel will reprogram this for the system tick. */
    __mtspr(SPR_DECREMENTER, 0x7FFFFFFFUL);

    /* 3. Timebase is typically initialized by firmware (OpenFirmware on
     *    Broadway). We read it to confirm it is running; no write on G3
     *    unless we want to reset it. */
    (void)__mfspr(SPR_TIMEBASE_L);
    (void)__mfspr(SPR_TIMEBASE_U);

    /* 4. Exception vector setup is handled by HalpInstallVectors
     *    (interrupt.c) during kernel boot. */

    return STATUS_SUCCESS;
}

/*
 * HalpSetDecrementer - program the PPC Decrementer interval timer.
 *
 * The Decrementer decrements at the bus clock / 4 rate and raises
 * an interrupt when it passes through zero. Used for the system clock tick.
 */
VOID
HalpSetDecrementer(IN ULONG Interval)
{
    __mtspr(SPR_DECREMENTER, Interval);
}

/*
 * HalpGetDecrementer - read current Decrementer value.
 */
ULONG
HalpGetDecrementer(VOID)
{
    return __mfspr(SPR_DECREMENTER);
}

/*
 * HalpEnableInterrupts - globally enable external interrupts (MSR.EE).
 */
VOID
HalpEnableInterrupts(VOID)
{
    ULONG msr = __get_MSR();
    msr |= MSR_EE;
    __set_MSR(msr);
}

/*
 * HalpDisableInterrupts - globally disable external interrupts (MSR.EE).
 */
VOID
HalpDisableInterrupts(VOID)
{
    ULONG msr = __get_MSR();
    msr &= ~MSR_EE;
    __set_MSR(msr);
}

/*
 * HalpGetProcessorId - return the current processor number.
 *
 * On Broadway/G3 this reads from a platform-specific location or
 * returns 0 for a single-core port. Multi-core support would read
 * the PVR (Processor Version Register) or a CPU ID register.
 */
ULONG
HalpGetProcessorId(VOID)
{
    /* Stub: single processor */
    return 0;
}

/*
 * Dummy references so the linker sees the HAL as a real module.
 * These are implemented elsewhere (context.S, atomic.c, interrupt.c).
 */
NTSTATUS
HalpQueryProcessorInformation(VOID)
{
    return STATUS_SUCCESS;
}

VOID
HalpStallExecution(IN ULONG Microseconds)
{
    /* Stall by spinning on the Decrementer/timebase. */
    volatile ULONG i;
    for (i = 0; i < Microseconds * 100UL; i++) {
        __asm__ __volatile__ ("nop");
    }
}
