/*++
Copyright (c) 1992  Microsoft Corporation
Ported to Broadway/PowerPC G3 (NT 5.1) - stub

Module Name:

    initx86.c

Abstract:

    PowerPC stub equivalent of base/boot/bldr/i386/initx86.x
    Original: x86-specific initialization, then starts the common ARC osloader.
    This stub is self-contained and compiles as a PowerPC ELF object.

--*/

/* Self-contained constants for the PPC port. */
#define PPC_INITX86_STUB 1
#define PPC_BOOT_SUCCESS 0
#define PPC_SECTOR_SIZE 512

/* Entry point for the PowerPC boot loader stub. */
int main(void)
{
    return PPC_BOOT_SUCCESS;
}
