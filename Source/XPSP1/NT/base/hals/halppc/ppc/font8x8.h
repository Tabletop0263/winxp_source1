#ifndef _FONT8X8_H_
#define _FONT8X8_H_

/*
 * 8x8 pixel font for PowerPC framebuffer text output.
 * Each character is 8 bytes, one byte per row, MSB = leftmost pixel.
 * Covers printable ASCII 0x20-0x7F (96 chars).
 */

extern const unsigned char font8x8_basic[96][8];

#endif /* _FONT8X8_H_ */
