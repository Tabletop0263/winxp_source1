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
    HalpPixelWrite - write a 32-bit pixel to the framebuffer.

    The framebuffer is assumed to be 32-bit RGBA8888 (as arcldr sets up
    VI_DISPLAY_PIX_SZ based modes). We write in little-endian order since
    the kernel is compiled little-endian and ModeSwitchEntry enables
    MSR_ILE (little-endian mode) before jumping to us.
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

    offset = Y * HalpFrameBufferStride + X * 4;

    /* Write 32-bit pixel (little-endian). */
    *(volatile unsigned long *)(HalpFrameBuffer + offset) = Color;
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
