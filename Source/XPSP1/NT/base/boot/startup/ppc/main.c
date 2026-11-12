/*++
Ported to Broadway/PowerPC G3 (NT 5.1) - stub

Module Name:

    main.c

Abstract:

    PowerPC stub equivalent of base/boot/startup/i386/main.c
    Original: main entry of the SU module, transitions x86 from real-mode
    to flat 32-bit protected/paging mode, then transfers to OS loader.
    This stub is self-contained and compiles as a PowerPC ELF object.

--*/

/* Self-contained constants for the PPC port. */
#define PPC_MAIN_STUB 1
#define PPC_BOOT_SUCCESS 0

/* Entry point for the PowerPC startup stub. */
int main(void)
{
    return PPC_BOOT_SUCCESS;
}
