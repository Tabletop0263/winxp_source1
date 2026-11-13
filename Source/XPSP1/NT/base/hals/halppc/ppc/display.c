/*
 *  display.c - PowerPC (Broadway/G3) framebuffer display driver
 *
 *  Windows XP (NT 5.1) port for Broadway/PowerPC G3 (GameCube/Wii).
 *
 *  Uses the PHW_DESCRIPTION framebuffer info passed by entii's arcldr
 *  (MemoryLength[0] splash, FrameBufferBase/Length/Width/Height/Stride)
 *  to render text directly into the framebuffer memory.
 *
 *  Build: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c display.c
 */

#include "ppctx.h"
#include "font8x8.h"

/* Framebuffer state - filled from PHW_DESCRIPTION during HAL init. */
static volatile unsigned char *HalpFrameBuffer = NULL;
static ULONG   HalpFrameBufferWidth  = 640;
static ULONG   HalpFrameBufferHeight = 480;
static ULONG   HalpFrameBufferStride = 640 * 2;   /* default: RGB565 = 2 bytes/px */

/* Cursor position (in characters). */
static ULONG HalpCursorX = 0;
static ULONG HalpCursorY = 0;

/* Text color (default: white foreground, black background). */
static ULONG HalpFgColor = 0xFFFFFFFFUL;   /* white */
static ULONG HalpBgColor = 0x00000000UL;   /* black */

/* Character cell dimensions (from 8x8 font). */
#define DISPLAY_CHAR_WIDTH  8
#define DISPLAY_CHAR_HEIGHT 8

/*--------------------------------------------------------------------------
    HalpDisplayInit - initialize the framebuffer display from PHW_DESCRIPTION.

    Called by KeInitializeKernelPpc after the loader block is available.
    Reads FrameBufferBase/Width/Height/Stride from the PHW_DESCRIPTION.
--------------------------------------------------------------------------*/
VOID
HalpDisplayInit(
    IN PVOID LoaderBlock
    )
{
    /* PHW_DESCRIPTION layout (from entii inc/hwdesc.h):
     *   offset 0  MemoryLength[0]
     *   offset 4  MemoryLength[1]
     *   offset 8  DdrIpcBase
     *   offset 12 DdrIpcLength
     *   offset 16 DecrementerFrequency
     *   offset 20 RtcBias
     *   offset 24 FpFlags
     *   offset 28 FrameBufferBase
     *   offset 32 FrameBufferLength
     *   offset 36 FrameBufferWidth
     *   offset 40 FrameBufferHeight
     *   offset 44 FrameBufferStride
     *   offset 48 GxFifoBase
     *   offset 52 Padding
     */

    if (LoaderBlock == NULL) {
        /* No loader block: use safe defaults. */
        HalpFrameBuffer      = (volatile unsigned char *)(unsigned long)0xC0000000UL;
        HalpFrameBufferWidth  = 640;
        HalpFrameBufferHeight = 480;
        HalpFrameBufferStride = 640 * 2;
        return;
    }

    HalpFrameBuffer     = (volatile unsigned char *)(unsigned long)
                             (*(ULONG *)((ULONG_PTR)LoaderBlock + 28));
    HalpFrameBufferWidth  = *(ULONG *)((ULONG_PTR)LoaderBlock + 36);
    HalpFrameBufferHeight = *(ULONG *)((ULONG_PTR)LoaderBlock + 40);
    HalpFrameBufferStride = *(ULONG *)((ULONG_PTR)LoaderBlock + 44);

    /* Sanity check stride. */
    if (HalpFrameBufferStride == 0) {
        HalpFrameBufferStride = HalpFrameBufferWidth * 2;
    }

    /* Reset cursor. */
    HalpCursorX = 0;
    HalpCursorY = 0;
}

/*--------------------------------------------------------------------------
    HalpPixelWrite - write a 16-bit RGB565 pixel to the framebuffer.

    The framebuffer is RGB565 (2 bytes/pixel) as set up by arcldr via libogc
    default video mode (VI_DISPLAY_PIX_SZ = 2). We write in little-endian
    order since the kernel is compiled little-endian and ModeSwitchEntry
    enables MSR_ILE (little-endian mode) before jumping to us.
--------------------------------------------------------------------------*/
static VOID
HalpPixelWrite(
    IN ULONG X,
    IN ULONG Y,
    IN ULONG Color
    )
{
    ULONG offset;

    if (HalpFrameBuffer == NULL) {
        return;
    }

    if (X >= HalpFrameBufferWidth || Y >= HalpFrameBufferHeight) {
        return;
    }

    offset = Y * HalpFrameBufferStride + X * 2;

    /* Write 16-bit pixel (little-endian): low byte first, then high byte. */
    HalpFrameBuffer[offset]     = (unsigned char)(Color & 0xFF);
    HalpFrameBuffer[offset + 1] = (unsigned char)((Color >> 8) & 0xFF);
}

/*--------------------------------------------------------------------------
    HalpScroll - scroll the framebuffer up by one character line.
--------------------------------------------------------------------------*/
static VOID
HalpScroll(VOID)
{
    ULONG row_bytes, copy_bytes;

    if (HalpFrameBuffer == NULL) {
        return;
    }

    row_bytes = DISPLAY_CHAR_HEIGHT * HalpFrameBufferStride;
    copy_bytes = (HalpFrameBufferHeight - DISPLAY_CHAR_HEIGHT) * HalpFrameBufferStride;

    if (copy_bytes == 0) {
        return;
    }

    /* Shift framebuffer up by one character row. */
    for (ULONG i = 0; i < copy_bytes; i += 4) {
        *(volatile unsigned long *)(HalpFrameBuffer + i) =
            *(volatile unsigned long *)(HalpFrameBuffer + row_bytes + i);
    }

    /* Clear the bottom character row. */
    for (ULONG i = copy_bytes; i < copy_bytes + row_bytes; i += 4) {
        *(volatile unsigned long *)(HalpFrameBuffer + i) = HalpBgColor;
    }

    HalpCursorY -= DISPLAY_CHAR_HEIGHT;
}

/*--------------------------------------------------------------------------
    HalpPutChar - render a single character at the cursor position.

    Handles '\n' (newline) and advances the cursor.
--------------------------------------------------------------------------*/
VOID
HalpPutChar(
    IN CHAR Ch
    )
{
    if (HalpFrameBuffer == NULL) {
        return;
    }

    if (Ch == '\n') {
        HalpCursorX = 0;
        HalpCursorY += DISPLAY_CHAR_HEIGHT;
        if (HalpCursorY + DISPLAY_CHAR_HEIGHT > HalpFrameBufferHeight) {
            HalpScroll();
        }
        return;
    }

    if (Ch == '\r') {
        HalpCursorX = 0;
        return;
    }

    /* Only render printable ASCII. */
    if (Ch < 0x20 || Ch > 0x7F) {
        Ch = '?';
    }

    const unsigned char *glyph = font8x8_basic[Ch - 0x20];

    for (ULONG row = 0; row < DISPLAY_CHAR_HEIGHT; row++) {
        unsigned char bits = glyph[row];
        for (ULONG col = 0; col < DISPLAY_CHAR_WIDTH; col++) {
            ULONG color = (bits & 0x80) ? HalpFgColor : HalpBgColor;
            HalpPixelWrite(HalpCursorX * DISPLAY_CHAR_WIDTH + col,
                           HalpCursorY + row,
                           color);
            bits <<= 1;
        }
    }

    HalpCursorX += 1;

    /* Wrap to next line at screen edge. */
    ULONG max_chars_x = HalpFrameBufferWidth / DISPLAY_CHAR_WIDTH;
    if (HalpCursorX >= max_chars_x) {
        HalpCursorX = 0;
        HalpCursorY += DISPLAY_CHAR_HEIGHT;
        if (HalpCursorY + DISPLAY_CHAR_HEIGHT > HalpFrameBufferHeight) {
            HalpScroll();
        }
    }
}

/*--------------------------------------------------------------------------
    HalpPutString - render a null-terminated string.
--------------------------------------------------------------------------*/
VOID
HalpPutString(
    IN const CHAR *Str
    )
{
    while (*Str) {
        HalpPutChar(*Str);
        Str++;
    }
}

/*--------------------------------------------------------------------------
    HalpClearScreen - fill the framebuffer with the background color.
--------------------------------------------------------------------------*/
VOID
HalpClearScreen(VOID)
{
    if (HalpFrameBuffer == NULL) {
        return;
    }

    ULONG total = HalpFrameBufferHeight * HalpFrameBufferStride;
    for (ULONG i = 0; i < total; i += 4) {
        *(volatile unsigned long *)(HalpFrameBuffer + i) = HalpBgColor;
    }

    HalpCursorX = 0;
    HalpCursorY = 0;
}

/*--------------------------------------------------------------------------
    HalpSetColors - set foreground/background text colors.
--------------------------------------------------------------------------*/
VOID
HalpSetColors(
    IN ULONG FgColor,
    IN ULONG BgColor
    )
{
    HalpFgColor = FgColor;
    HalpBgColor = BgColor;
}

/*--------------------------------------------------------------------------
    Blink indicator - draws a flashing block at the top-left corner of the
    framebuffer so a human can tell the kernel is running even when there
    is no other visible output.

    Two modes:
      * HalpDisplayTickBlink()   - single toggle, called once per Decrementer
                                   tick when interrupts work (vector 0x900).
      * HalpBlinkBootIndicator() - busy-wait infinite loop that flashes the
                                   block repeatedly with NO interrupts needed.
                                   Use this as a "kernel alive" heartbeat at
                                   end of boot when interrupts may not fire.
--------------------------------------------------------------------------*/
static volatile ULONG HalpBlinkTickCount = 0;
static volatile BOOLEAN HalpBlinkVisible = FALSE;

/* Draw a solid colored block (filled square) at screen pixel coords. */
static VOID
HalpDrawBlinkBlock(
    IN ULONG X,
    IN ULONG Y,
    IN ULONG Size,
    IN BOOLEAN On
    )
{
    ULONG color = On ? HalpFgColor : HalpBgColor;
    ULONG row, col;

    if (HalpFrameBuffer == NULL) {
        return;
    }

    for (row = Y; row < Y + Size && row < HalpFrameBufferHeight; row++) {
        for (col = X; col < X + Size && col < HalpFrameBufferWidth; col++) {
            HalpPixelWrite(col, row, color);
        }
    }
}

/*--------------------------------------------------------------------------
    HalpDisplayTickBlink - single blink toggle per call.

    Called once per Decrementer tick (100 Hz). Toggles the blink block
    once per second (~100 ticks). No busy-wait; safe to call from ISR.
--------------------------------------------------------------------------*/
VOID
HalpDisplayTickBlink(VOID)
{
    /* Erase previous state, then toggle. */
    HalpDrawBlinkBlock(8, 8, 24, HalpBlinkVisible);

    HalpBlinkTickCount++;
    if (HalpBlinkTickCount >= 100) {   /* ~1 second at 100 Hz */
        HalpBlinkTickCount = 0;
        HalpBlinkVisible = (BOOLEAN)!HalpBlinkVisible;
    }

    /* Draw new state. */
    HalpDrawBlinkBlock(8, 8, 24, HalpBlinkVisible);
}

/*--------------------------------------------------------------------------
    HalpDiskActivityBlink - flash the disk activity block once.

    Called from SD read/write paths so a second block (near the kernel-alive
    block) flashes whenever the SD card is accessed. Use a distinct position
    so kernel-alive vs disk activity are distinguishable.
--------------------------------------------------------------------------*/
VOID
HalpDiskActivityBlink(VOID)
{
    /* Flash a small block at (40, 8) to indicate SD/disk activity. */
    HalpDrawBlinkBlock(40, 8, 16, TRUE);
    HalpStallExecution(200000);   /* ~200 ms on */
    HalpDrawBlinkBlock(40, 8, 16, FALSE);
    HalpStallExecution(200000);   /* ~200 ms off */
}

/*--------------------------------------------------------------------------
    HalpBlinkBootIndicator - busy-wait "kernel alive" heartbeat.

    Flashes the kernel-alive block at (8,8) ~once per second using a
    busy-wait stall. NO interrupts required. Loops forever; call this as
    the final step of boot when you want a guaranteed visual heartbeat
    on real hardware.

    Also flashes the disk-activity block at (40,8) whenever the SD card
    has been accessed since the last blink cycle (tracked by
    HalpDiskActivityPending).
--------------------------------------------------------------------------*/
volatile BOOLEAN HalpDiskActivityPending = FALSE;

VOID
HalpBlinkBootIndicator(VOID)
{
    volatile ULONG i;
    ULONG cycle;

    for (cycle = 0; ; cycle++) {
        /* Kernel-alive block: on for ~500 ms, off for ~500 ms. */
        HalpDrawBlinkBlock(8, 8, 24, TRUE);
        for (i = 0; i < 5000000UL; i++) { __asm__ __volatile__ ("nop"); }

        HalpDrawBlinkBlock(8, 8, 24, FALSE);

        /* Disk-activity block: flash if SD was accessed this cycle. */
        if (HalpDiskActivityPending) {
            HalpDiskActivityPending = FALSE;
            HalpDrawBlinkBlock(40, 8, 16, TRUE);
            for (i = 0; i < 1000000UL; i++) { __asm__ __volatile__ ("nop"); }
            HalpDrawBlinkBlock(40, 8, 16, FALSE);
        }

        for (i = 0; i < 5000000UL; i++) { __asm__ __volatile__ ("nop"); }
    }
}

/*--------------------------------------------------------------------------
    HalpDisplayPrintf - minimal printf-like formatted output.

    Supports %s, %c, %d, %u, %x, %p, %%. No floating point.
--------------------------------------------------------------------------*/
VOID
HalpDisplayPrintf(
    IN const CHAR *Fmt,
    ...
    )
{
    /* Simple variadic implementation using __builtin_va_list. */
    __builtin_va_list args;
    __builtin_va_start(args, Fmt);

    while (*Fmt) {
        if (*Fmt == '%') {
            Fmt++;
            switch (*Fmt) {
            case 's': {
                const CHAR *s = __builtin_va_arg(args, const CHAR *);
                HalpPutString(s ? s : "(null)");
                break;
            }
            case 'c': {
                CHAR c = (CHAR)__builtin_va_arg(args, int);
                HalpPutChar(c);
                break;
            }
            case 'd': {
                int d = __builtin_va_arg(args, int);
                CHAR buf[12];
                int i = 0, neg = 0;
                if (d < 0) { neg = 1; d = -d; }
                do { buf[i++] = (CHAR)('0' + d % 10); d /= 10; } while (d);
                if (neg) buf[i++] = '-';
                while (i--) HalpPutChar(buf[i]);
                break;
            }
            case 'u': {
                unsigned u = __builtin_va_arg(args, unsigned);
                CHAR buf[12];
                int i = 0;
                do { buf[i++] = (CHAR)('0' + u % 10); u /= 10; } while (u);
                while (i--) HalpPutChar(buf[i]);
                break;
            }
            case 'x':
            case 'p': {
                unsigned long v = (unsigned long)__builtin_va_arg(args, unsigned long);
                CHAR buf[16];
                int i = 0;
                if (v == 0) { HalpPutChar('0'); break; }
                while (v) {
                    unsigned nib = v & 0xF;
                    buf[i++] = (CHAR)(nib < 10 ? '0' + nib : 'a' + nib - 10);
                    v >>= 4;
                }
                while (i--) HalpPutChar(buf[i]);
                break;
            }
            case '%':
                HalpPutChar('%');
                break;
            default:
                HalpPutChar(*Fmt);
                break;
            }
        } else {
            HalpPutChar(*Fmt);
        }
        Fmt++;
    }

    __builtin_va_end(args);
}
