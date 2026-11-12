/*++
Ported to Broadway/PowerPC G3 (NT 5.1) - stub

Module Name:

    display.c

Abstract:

    PowerPC stub equivalent of base/boot/startup/i386/display.c
    Original: video/display initialization for the SU (startup) module.
    This stub is self-contained and compiles as a PowerPC ELF object.

--*/

/* Self-contained constants for the PPC port. */
#define PPC_DISPLAY_STUB 1
#define PPC_BOOT_SUCCESS 0

/* Entry point for the PowerPC startup stub. */
int main(void)
{
    return PPC_BOOT_SUCCESS;
}
