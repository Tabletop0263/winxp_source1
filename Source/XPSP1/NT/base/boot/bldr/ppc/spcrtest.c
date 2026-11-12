/*++
Ported to Broadway/PowerPC G3 (NT 5.1) - stub

Module Name:

    spcrtest.c

Abstract:

    PowerPC stub equivalent of base/boot/bldr/i386/spcrtest.c.
    This stub is self-contained and compiles as a PowerPC ELF object.

--*/

/* Self-contained constants for the PPC port. */
#define PPC_SPCRTEST_STUB 1
#define PPC_BOOT_SUCCESS 0

/* Entry point for the PowerPC boot loader stub. */
int main(void)
{
    return PPC_BOOT_SUCCESS;
}
