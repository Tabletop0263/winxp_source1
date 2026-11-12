/*
 *  atomic.c - PowerPC (Broadway/G3) atomic interlocked operations
 *
 *  PowerPC HAL primitive for Windows XP (NT 5.1) port.
 *
 *  Implements InterlockedIncrement, InterlockedDecrement, and
 *  InterlockedCompareExchange using the PowerPC lwarx/stwcx
 *  load-reserve/store-conditional pair, which provides the
 *  atomic read-modify-write semantics required for SMP-safe
 *  kernel synchronization on the G3 core.
 *
 *  Build: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c atomic.c
 */

#include <stddef.h>

typedef long LONG;
typedef volatile long *PLONG;

#define InterlockedIncrement _InterlockedIncrementPPC
#define InterlockedDecrement _InterlockedDecrementPPC
#define InterlockedCompareExchange _InterlockedCompareExchangePPC

/*
 * _InterlockedIncrementPPC - atomically add 1 to *Addend, return new value.
 *
 * PowerPC implementation:
 *   lwarx r5, 0, r3     // load word and reserve address
 *   addi  r5, r5, 1     // increment
 *   stwcx. r5, 0, r3    // store conditional; clears CR0.EQ on failure
 *   bne-  -8            // retry if store failed
 */
LONG
_InterlockedIncrementPPC(PLONG Addend)
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

/*
 * _InterlockedDecrementPPC - atomically subtract 1 from *Addend, return new value.
 */
LONG
_InterlockedDecrementPPC(PLONG Addend)
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

/*
 * _InterlockedCompareExchangePPC - atomic compare-and-swap.
 *
 *   if (*Destination == Comperand) *Destination = Exchange;
 *   return old value of *Destination.
 */
LONG
_InterlockedCompareExchangePPC(PLONG Destination, LONG Exchange, LONG Comperand)
{
    LONG old;

    __asm__ __volatile__ (
        "1: lwarx   %0, 0, %2\n"
        "   cmpw    %0, %4\n"
        "   bne-    2f\n"
        "   stwcx.  %3, 0, %2\n"
        "   bne-    1b\n"
        "2:\n"
        : "=&r" (old), "+m" (*Destination)
        : "r" (Destination), "r" (Exchange), "r" (Comperand)
        : "cc", "memory"
    );

    return old;
}

/*
 * InterlockedExchangePPC - atomic swap, return old value.
 */
LONG
InterlockedExchange(PLONG Target, LONG Value)
{
    LONG old;

    __asm__ __volatile__ (
        "1: lwarx   %0, 0, %2\n"
        "   stwcx.  %3, 0, %2\n"
        "   bne-    1b\n"
        : "=&r" (old), "+m" (*Target)
        : "r" (Target), "r" (Value)
        : "cc", "memory"
    );

    return old;
}
