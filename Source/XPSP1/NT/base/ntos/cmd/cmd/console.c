/*++

Copyright (c) Microsoft Corporation

Module Name:

    console.c

Abstract:

    Minimal gamepad-driven console / line editor for the PowerPC
    (Broadway/G3) port of Windows XP (NT 5.1).  Self-contained: only
    depends on ppctx.h and wxptypes.h plus HAL display/input functions:

      - HalpDisplayPrintf / HalpPutChar  - output
      - HalpGetControllerState            - poll the PAD
      - HalpControllerMapButton           - map PAD buttons to virtual keys

    Virtual-key mapping (PAD buttons -> console keys):
      PAD_BUTTON_A    -> '\r' (Enter)
      PAD_BUTTON_B    -> '\b' (Backspace)
      PAD_BUTTON_X    -> 'x'  (toggle echo / placeholder)
      PAD_BUTTON_Y    -> ' '  (space)
      PAD_BUTTON_UP   -> up-arrow (history, stubbed)
      PAD_BUTTON_DOWN -> down-arrow (history, stubbed)
      PAD_BUTTON_LEFT -> left (cursor, stubbed)
      PAD_BUTTON_RIGHT-> right  (cursor, stubbed)
      PAD_BUTTON_START-> '\r' (Enter)
      PAD_BUTTON_SELECT-> '\b' (Backspace)
      PAD_BUTTON_L/R  -> not mapped (reserved)

    Printable characters are entered by holding the corresponding face
    button for a debounced poll period; this minimal port does not have a
    full keyboard matrix, so only a small printable subset is exposed via
    the PAD (A/B/X/Y + D-pad).  The line editor supports backspace and
    enter, with a fixed input buffer.

Author:

    (port)

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"
#include "wxptypes.h"

/*--------------------------------------------------------------------------
    Blink indicator - declared in ppctx.h (implemented in
    hals/halppc/ppc/display.c).  HalpDisplayTickBlink() toggles the
    kernel-alive block at (8,8) ~once per second.  Called each poll
    iteration here so the block keeps flashing even if the Decrementer
    interrupt never fires on real hardware.
--------------------------------------------------------------------------*/
VOID HalpDisplayTickBlink(VOID);

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

/*--------------------------------------------------------------------------
    Console input buffer.
--------------------------------------------------------------------------*/
#define CONSOLE_LINE_MAX 128

static CHAR ConsoleLine[CONSOLE_LINE_MAX];
static ULONG ConsoleLineLen = 0;

/*--------------------------------------------------------------------------
    Virtual-key definitions (console-side).
--------------------------------------------------------------------------*/
#define VK_RETURN  0x0D
#define VK_BACK    0x08
#define VK_SPACE   0x20
#define VK_ESCAPE  0x1B

/*--------------------------------------------------------------------------
    HalpControllerMapButton - declared in ppctx.h.  Maps PAD button bits
    to a virtual-key value (0 = no mapping).
--------------------------------------------------------------------------*/
ULONG
HalpControllerMapButton(
    IN ULONG PadButtons
    );

/*--------------------------------------------------------------------------
    ConsoleReadLine - read a line of input from the gamepad.

    Polls the controller state, debounces button presses, maps buttons to
    virtual keys via HalpControllerMapButton, and edits ConsoleLine.
    Returns when Enter (VK_RETURN) is pressed.  The line (without the
    trailing newline) is left in ConsoleLine and ConsoleLineLen is updated.

    NOTE: This is a polling, blocking read.  Because this port has no
    scheduler, the shell thread owns the CPU while in this function.
--------------------------------------------------------------------------*/
VOID
ConsoleReadLine(
    VOID
    )
{
    CONTROLLER_STATE State;
    ULONG PreviousButtons = 0;
    BOOLEAN Done = FALSE;

    /* Debounce state: track which buttons were pressed last poll so we
     * only generate one key event per press (not per poll while held). */
    for (;;) {
        /* Flash the kernel-alive block each poll so a human can tell the
         * kernel is running even if the Decrementer interrupt never fires. */
        HalpDisplayTickBlink();

        HalpGetControllerState(&State);

        /* Buttons newly pressed since last poll. */
        ULONG Pressed = State.Buttons & ~PreviousButtons;

        if (Pressed != 0) {
            ULONG Vk = HalpControllerMapButton(Pressed);

            switch (Vk) {
            case VK_RETURN:
                /* End the line. */
                ConsoleLine[ConsoleLineLen] = '\0';
                Done = TRUE;
                break;

            case VK_BACK:
                if (ConsoleLineLen > 0) {
                    ConsoleLineLen--;
                    ConsoleLine[ConsoleLineLen] = '\0';
                    HalpPutChar('\b');
                    HalpPutChar(' ');
                    HalpPutChar('\b');
                }
                break;

            case VK_SPACE:
                if (ConsoleLineLen < CONSOLE_LINE_MAX - 1) {
                    ConsoleLine[ConsoleLineLen++] = ' ';
                    ConsoleLine[ConsoleLineLen] = '\0';
                    HalpPutChar(' ');
                }
                break;

            default:
                if (Vk != 0 && ConsoleLineLen < CONSOLE_LINE_MAX - 1) {
                    CHAR Ch = (CHAR)Vk;
                    /* Only accept printable ASCII. */
                    if (Ch >= 0x20 && Ch <= 0x7E) {
                        ConsoleLine[ConsoleLineLen++] = Ch;
                        ConsoleLine[ConsoleLineLen] = '\0';
                        HalpPutChar(Ch);
                    }
                }
                break;
            }
        }

        PreviousButtons = State.Buttons;

        if (Done) {
            HalpPutChar('\r');
            HalpPutChar('\n');
            return;
        }
    }
}

/*--------------------------------------------------------------------------
    CmdInitializeConsole - initialize the console (clear input buffer).
--------------------------------------------------------------------------*/
VOID
CmdInitializeConsole(
    VOID
    )
{
    ConsoleLineLen = 0;
    ConsoleLine[0] = '\0';
}

/*--------------------------------------------------------------------------
    CmdShutdownConsole - shutdown the console (no-op for bump allocator).
--------------------------------------------------------------------------*/
VOID
CmdShutdownConsole(
    VOID
    )
{
    return;
}

/*--------------------------------------------------------------------------
    CmdGetLine - return pointer to the current input line buffer.
--------------------------------------------------------------------------*/
PCHAR
CmdGetLine(
    VOID
    )
{
    return ConsoleLine;
}

/*--------------------------------------------------------------------------
    CmdGetLineLen - return current input line length.
--------------------------------------------------------------------------*/
ULONG
CmdGetLineLen(
    VOID
    )
{
    return ConsoleLineLen;
}

/*--------------------------------------------------------------------------
    CmdSetLine - replace the current input line (used by dispatch after
    reading).  Clears the buffer.
--------------------------------------------------------------------------*/
VOID
CmdSetLine(
    IN PCHAR Line,
    IN ULONG Length
    )
{
    ULONG copyLen = Length;
    if (copyLen > CONSOLE_LINE_MAX - 1) {
        copyLen = CONSOLE_LINE_MAX - 1;
    }
    WxpMemCopy(ConsoleLine, Line, copyLen);
    ConsoleLine[copyLen] = '\0';
    ConsoleLineLen = copyLen;
}

/*--------------------------------------------------------------------------
    CmdClearLine - clear the input buffer.
--------------------------------------------------------------------------*/
VOID
CmdClearLine(
    VOID
    )
{
    ConsoleLineLen = 0;
    ConsoleLine[0] = '\0';
}
