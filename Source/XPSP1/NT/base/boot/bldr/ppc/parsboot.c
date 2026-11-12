/*++
Copyright (c) 1991  Microsoft Corporation
Ported to Broadway/PowerPC G3 (NT 5.1) - stub

Module Name:

    parsboot.c

Abstract:

    PowerPC stub equivalent of base/boot/bldr/i386/parsboot.c
    Original: parses boot.ini, displays a menu, provides kernel path/name.
    This stub is self-contained and compiles as a PowerPC ELF object.

--*/

/* Self-contained constants for the PPC port. */
#define PPC_PARSBOOT_STUB 1
#define PPC_MAX_SELECTIONS 10
#define PPC_MAX_TITLE_LENGTH 71
#define PPC_BOOT_SUCCESS 0

/* Entry point for the PowerPC boot loader stub. */
int main(void)
{
    return PPC_BOOT_SUCCESS;
}
