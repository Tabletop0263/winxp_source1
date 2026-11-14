/*
 *  pad.c - Wii/GameCube PAD (Serial Interface) controller driver
 *
 *  Windows XP (NT 5.1) port for Broadway/PowerPC G3 (GameCube/Wii).
 *
 *  Reads controller state directly from the Broadway SI (Serial Interface)
 *  hardware registers. Exposes HalpGetControllerState() so the kernel/HID
 *  input subsystem can poll gamepad/keyboard state.
 *
 *  Broadway SI register map (gamepad channel 0):
 *    0xCD006400  SI status        (read)
 *    0xCD006404  SI data          (read/write)
 *    0xCD006408  SI control       (read/write)
 *    0xCD00640C  SI baud          (read/write)
 *    0xCD006410  SI interrupt     (read/write)
 *
 *  PAD command sequence per libogc:
 *    - Write 0x40 (PAD status request) to SI data, polling busy bit.
 *    - Read 6 bytes: status byte + 5 data bytes (buttons + sticks).
 *
 *  Build: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c pad.c
 */

#include "ppctx.h"

/*--------------------------------------------------------------------------
    Broadway SI (Serial Interface) register base (gamepad).

    NOTE: On Broadway/G3, EXI2 (SD card) is at 0xCD006400 and SI (Serial
    Interface / gamepad) is at 0xCD006000.  These must not collide -- the
    previous 0xCD006400 base overlapped EXI2 and corrupted SD registers
    before HalpSdInit() ran, causing an instant crash on real hardware.
--------------------------------------------------------------------------*/
#define SI_BASE            ((volatile unsigned long *)0xCD006000UL)
#define SI_REG_STATUS      SI_BASE[0]   /* 0xCD006000 */
#define SI_REG_DATA        SI_BASE[1]   /* 0xCD006004 */
#define SI_REG_CONTROL     SI_BASE[2]   /* 0xCD006008 */
#define SI_REG_BAUD        SI_BASE[3]   /* 0xCD00600C */
#define SI_REG_INTERRUPT   SI_BASE[4]   /* 0xCD006010 */

/* SI status register bits. */
#define SI_STATUS_BUSY     0x00000001UL /* SI busy transferring */
#define SI_STATUS_RX_READY 0x00000002UL /* receive data ready */

/* SI control register bits. */
#define SI_CONTROL_CHAN0   0x00000000UL /* EXI channel 0 (gamepad port 1) */
#define SI_CONTROL_CHAN1   0x00000001UL /* EXI channel 1 (gamepad port 2) */

/* PAD command bytes (libogc convention). */
#define PAD_CMD_STATUS     0x40UL  /* request controller status */

/*--------------------------------------------------------------------------
    Controller button bitmask (PAD / gamepad).
--------------------------------------------------------------------------*/
#define PAD_BUTTON_A       0x0001UL
#define PAD_BUTTON_B       0x0002UL
#define PAD_BUTTON_X       0x0004UL
#define PAD_BUTTON_Y       0x0008UL
#define PAD_BUTTON_START   0x0010UL
#define PAD_BUTTON_SELECT  0x0020UL
#define PAD_BUTTON_UP      0x0040UL
#define PAD_BUTTON_DOWN    0x0080UL
#define PAD_BUTTON_LEFT    0x0100UL
#define PAD_BUTTON_RIGHT   0x0200UL
#define PAD_BUTTON_L       0x0400UL
#define PAD_BUTTON_R       0x0800UL
#define PAD_BUTTON_ZL      0x1000UL
#define PAD_BUTTON_ZR      0x2000UL

/*--------------------------------------------------------------------------
    Controller state snapshot.
--------------------------------------------------------------------------*/
typedef struct _CONTROLLER_STATE {
    ULONG   Buttons;          /* PAD_BUTTON_* bitmask of currently pressed */
    USHORT  StickX;           /* left stick X (-128..127) */
    USHORT  StickY;           /* left stick Y (-128..127) */
    BOOLEAN Connected;        /* whether a controller is present */
} CONTROLLER_STATE, *PCONTROLLER_STATE;

static CONTROLLER_STATE HalpControllerState;

/*--------------------------------------------------------------------------
    HalpWaitSiIdle - wait until the SI bus is not busy.
--------------------------------------------------------------------------*/
static VOID
HalpWaitSiIdle(VOID)
{
    volatile ULONG timeout = 0x10000UL;
    while ((SI_REG_STATUS & SI_STATUS_BUSY) && timeout--) {
        __asm__ __volatile__ ("nop");
    }
}

/*--------------------------------------------------------------------------
    HalpSiWrite - write a byte to the SI data register.
--------------------------------------------------------------------------*/
static VOID
HalpSiWrite(IN UCHAR Byte)
{
    HalpWaitSiIdle();
    SI_REG_DATA = (ULONG)Byte;
    HalpWaitSiIdle();
}

/*--------------------------------------------------------------------------
    HalpSiRead - read a byte from the SI data register.
--------------------------------------------------------------------------*/
static UCHAR
HalpSiRead(VOID)
{
    HalpWaitSiIdle();
    return (UCHAR)(SI_REG_DATA & 0xFFUL);
}

/*--------------------------------------------------------------------------
    HalpPollController - perform one PAD status read on channel 0.
--------------------------------------------------------------------------*/
static VOID
HalpPollController(VOID)
{
    UCHAR buf[6];
    ULONG buttons = 0;
    SHORT stick_x = 0, stick_y = 0;
    BOOLEAN connected = FALSE;

    /* Select channel 0 (gamepad port 1). */
    SI_REG_CONTROL = SI_CONTROL_CHAN0;

    /* Send PAD status request command. */
    HalpSiWrite(PAD_CMD_STATUS);

    /* Read 6 response bytes. */
    for (int i = 0; i < 6; i++) {
        buf[i] = HalpSiRead();
    }

    /* libogc PAD decode:
     *   byte0: status (0x00 = connected, 0xFF = no controller)
     *   byte1: buttons high
     *   byte2: buttons low
     *   byte3: stick X
     *   byte4: stick Y
     *   byte5: error/substatus
     */
    if (buf[0] == 0x00) {
        connected = TRUE;

        /* Buttons are active-low: 1 bit = not pressed. */
        ULONG btn_raw = ((ULONG)buf[1] << 8) | (ULONG)buf[2];
        ULONG btn_pressed = ~btn_raw & 0x0FFFUL;

        if (btn_pressed & 0x0001) buttons |= PAD_BUTTON_A;
        if (btn_pressed & 0x0002) buttons |= PAD_BUTTON_B;
        if (btn_pressed & 0x0004) buttons |= PAD_BUTTON_X;
        if (btn_pressed & 0x0008) buttons |= PAD_BUTTON_Y;
        if (btn_pressed & 0x0010) buttons |= PAD_BUTTON_START;
        if (btn_pressed & 0x0020) buttons |= PAD_BUTTON_SELECT;
        if (btn_pressed & 0x0040) buttons |= PAD_BUTTON_UP;
        if (btn_pressed & 0x0080) buttons |= PAD_BUTTON_DOWN;
        if (btn_pressed & 0x0100) buttons |= PAD_BUTTON_LEFT;
        if (btn_pressed & 0x0200) buttons |= PAD_BUTTON_RIGHT;
        if (btn_pressed & 0x0400) buttons |= PAD_BUTTON_L;
        if (btn_pressed & 0x0800) buttons |= PAD_BUTTON_R;

        /* Left stick: signed 8-bit centered at 0. */
        stick_x = (SHORT)(CHAR)buf[3];
        stick_y = (SHORT)(CHAR)buf[4];
    }

    HalpControllerState.Buttons   = buttons;
    HalpControllerState.StickX    = (USHORT)(stick_x & 0xFF);
    HalpControllerState.StickY    = (USHORT)(stick_y & 0xFF);
    HalpControllerState.Connected = connected;
}

/*--------------------------------------------------------------------------
    HalpGetControllerState - return the latest controller state snapshot.
--------------------------------------------------------------------------*/
VOID
HalpGetControllerState(
    OUT PCONTROLLER_STATE State
    )
{
    HalpPollController();
    *State = HalpControllerState;
}

/*--------------------------------------------------------------------------
    HalpControllerInit - initialize the SI controller interface.
--------------------------------------------------------------------------*/
VOID
HalpControllerInit(VOID)
{
    /* Reset state. */
    HalpControllerState.Buttons = 0;
    HalpControllerState.StickX  = 0;
    HalpControllerState.StickY  = 0;
    HalpControllerState.Connected = FALSE;

    /* One initial poll to detect presence. */
    HalpPollController();
}

/*--------------------------------------------------------------------------
    HalpControllerMapButton - translate a PAD button to a virtual key code.

    Minimal mapping for early boot/debug use. A full HID input stack would
    expand this into a proper HID usage table.
--------------------------------------------------------------------------*/
ULONG
HalpControllerMapButton(
    IN ULONG PadButtons
    )
{
    /* Return a simple ordinal for the first pressed button. */
    if (PadButtons & PAD_BUTTON_A)   return 0x41; /* 'A' */
    if (PadButtons & PAD_BUTTON_B)   return 0x42; /* 'B' */
    if (PadButtons & PAD_BUTTON_X)   return 0x58; /* 'X' */
    if (PadButtons & PAD_BUTTON_Y)   return 0x59; /* 'Y' */
    if (PadButtons & PAD_BUTTON_START) return 0x0D; /* Enter */
    if (PadButtons & PAD_BUTTON_UP)    return 0x26; /* VK_UP */
    if (PadButtons & PAD_BUTTON_DOWN)  return 0x28; /* VK_DOWN */
    if (PadButtons & PAD_BUTTON_LEFT)  return 0x25; /* VK_LEFT */
    if (PadButtons & PAD_BUTTON_RIGHT) return 0x27; /* VK_RIGHT */
    if (PadButtons & PAD_BUTTON_L)     return 0x4C; /* 'L' */
    if (PadButtons & PAD_BUTTON_R)     return 0x52; /* 'R' */
    return 0;
}
