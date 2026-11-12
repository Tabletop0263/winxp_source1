# ARC Loader Integration — Windows XP (NT 5.1) Kernel on Wii/GameCube

This document describes how the XP NT 5.1 kernel ELF (`arcfw.elf`) integrates
with entii's `arcldr` loader (`boot.dol`) for the Broadway/G3 port, and how to
run it on real Wii hardware via the Homebrew Channel.

## Contents

1. [Entry convention](#entry-convention)
2. [PHW_DESCRIPTION field offsets used](#phw_description-field-offsets-used)
3. [SD card layout](#sd-card-layout)
4. [Build & assembly verification](#build--assembly-verification)
5. [How to test on real Wii](#how-to-test-on-real-wii)
6. [Expected behavior](#expected-behavior)

---

## Entry convention

The kernel's `_start` (in `Source/XPSP1/NT/base/build/ppc/ntos_entry.S`) is
entered via entii's `ModeSwitchEntry` (defined in
`entii/arcldr/source/modeswitch.S`).  `ModeSwitchEntry` is declared in
`entii/arcldr/source/main.c` as:

```c
typedef void (*ArcFirmEntry)(PHW_DESCRIPTION HwDesc);
extern void __attribute__((noreturn)) ModeSwitchEntry(ArcFirmEntry Start, PHW_DESCRIPTION HwDesc);
```

In `modeswitch.S` (`ModeSwitchEntry`):

- `r3` (the `Start` entrypoint argument) is saved to `srr0`.
- `r4` (the `HwDesc` argument) is saved to `r31`, then `oris r3, r31, 0x8000`
  builds the K0 virtual address of the `PHW_DESCRIPTION`.
- `rfi` jumps to the entrypoint with the new `r3`.

So on entry to the XP kernel `_start`:

| GPR | Meaning | Source |
|-----|---------|--------|
| **r3** | `PHW_DESCRIPTION *` loader block (K0 virtual address) | entii `ModeSwitchEntry` (r4 → r3) |
| **r2** | PCR address (`&HalpPcrStorage`) | loaded by `_start` itself |

`_start` performs the following sequence:

1. Saves the loader block pointer (r3) to `KeLoaderBlock` (a `.word` in `.data`).
2. Loads the PCR address (`&HalpPcrStorage`, defined in `ntos/ppc/pcr.c`) into GPR
   r2 (the PowerPC TOC/global-pointer convention used by this port).
3. Reloads the loader block pointer into r3 (first argument).
4. Calls `KeInitializeKernel(r3)` (arch-independent entry in `ntos/ppc/ntoskrnl.c`).
5. After `KeInitializeKernel` returns, enters an idle loop with external
   interrupts enabled (`MSR.EE`), waiting for the Decrementer tick
   (vector `0x900`), which is dispatched via `HalpVectorEntry` and returns
   via `rfi`.

`KeInitializeKernel` stores the loader block and delegates to
`KeInitializeKernelPpc(LoaderBlock)`, which:
- installs the PCR (`KeSetPcr` / `HalpSetPcrRegister` → r2),
- installs exception vectors (`HalpInstallVectors` → sets `MSR.EE`),
- reads `DecrementerFrequency` from the loader block and programs `SPR_DEC`,
- initializes the framebuffer display and Wii PAD controller.

---

## PHW_DESCRIPTION field offsets used

`PHW_DESCRIPTION` is `HW_DESCRIPTION` defined in `entii/inc/hwdesc.h`:

```c
typedef struct _HW_DESCRIPTION {
    ULONG MemoryLength[2];   // offset 0x00, 0x04
    ULONG DdrIpcBase;        // offset 0x08
    ULONG DdrIpcLength;      // offset 0x0c
    ULONG DecrementerFrequency;  // offset 0x10
    ULONG RtcBias;           // offset 0x14
    ULONG FpFlags;           // offset 0x18

    // Framebuffer details.
    ULONG FrameBufferBase;   // offset 0x1c  (verified = 28)
    ULONG FrameBufferLength; // offset 0x20
    ULONG FrameBufferWidth;  // offset 0x24  (verified = 36)
    ULONG FrameBufferHeight; // offset 0x28  (verified = 40)
    ULONG FrameBufferStride; // offset 0x2c  (verified = 44)

    ULONG GxFifoBase;        // offset 0x30
    ULONG Padding;           // offset 0x34
} HW_DESCRIPTION, *PHW_DESCRIPTION;
```

Verified field offsets (byte offsets from `PHW_DESCRIPTION` base):

| Field | Offset | Notes |
|-------|--------|-------|
| `MemoryLength[0]` | 0x00 | |
| `MemoryLength[1]` | 0x04 | |
| `DdrIpcBase` | 0x08 | |
| `DdrIpcLength` | 0x0c | |
| `DecrementerFrequency` | 0x10 | read by `KeInitializeKernelPpc` at +16 |
| `RtcBias` | 0x14 | |
| `FpFlags` | 0x18 | FPF_* bit flags |
| `FrameBufferBase` | 0x1c (= 28) | framebuffer base address |
| `FrameBufferLength` | 0x20 | |
| `FrameBufferWidth` | 0x24 (= 36) | display width |
| `FrameBufferHeight` | 0x28 (= 40) | display height |
| `FrameBufferStride` | 0x2c (= 44) | bytes per line |
| `GxFifoBase` | 0x30 | |
| `Padding` | 0x34 | |

`KeInitializeKernelPpc` reads `DecrementerFrequency` at `KeLoaderBlock + 16`
(0x10). `HalpDisplayInit` uses the framebuffer fields (base/width/height/stride)
at offsets 28/36/40/44.

---

## SD card layout

Produced by `.github/workflows/build-wii.yml` (step "Package Wii SD card
layout").  Copy the **contents** of `wii_sd/` to the root of a FAT32 SD card:

```
sd:/
  README.md
  nt/
    arcfw.elf     <- XP kernel ELF32 little-endian (built ntoskrnl_le.elf)
  apps/
    arcldr/
      boot.dol    <- entii arcldr DOL (built arcldr_rvl.dol)
```

| SD path | Source | Description |
|---------|--------|-------------|
| `sd:/apps/arcldr/boot.dol` | `entii/arcldr/arcldr_rvl.dol` | arcldr loader, launched from Homebrew Channel |
| `sd:/nt/arcfw.elf` | `Source/XPSP1/NT/base/build/ppc/ntoskrnl_le.elf` | XP kernel ELF32 little-endian |
| `sd:/README.md` | generated by CI | testing instructions |

The CI also uploads the kernel ELF as a separate artifact named
`arcfw-kernel-elf` (`ntoskrnl_le.elf`) for easier download.

---

## Build & assembly verification

The kernel must compile under:

```
powerpc64le-linux-gnu-gcc -m32 -mcpu=750 -mno-altivec -Wall -Werror -O2 \
    -ffreestanding -nostdinc
```

Local verification (run from `Source/XPSP1/NT/base/build/ppc/`):

```sh
# Assemble entry + vector stubs
powerpc64le-linux-gnu-gcc -m32 -c ntos_entry.S -o obj_le/ntos_entry.o
powerpc64le-linux-gnu-gcc -m32 -c ../../ntos/ppc/ppc.S -o obj_le/ntos_ppc.o

# Compile C sources (see build-wii.yml / Makefile for include paths)
powerpc64le-linux-gnu-gcc -m32 -mcpu=750 -mno-altivec -Wall -Werror -O2 \
    -ffreestanding -nostdinc \
    -DPOWERPC -D_G3_ -DBROADWAY -DNT_UP -DNT_KERNEL -D_NTSYSTEM_ \
    -I../../ntos/inc/ppc -I../../ntos/inc -I../../ntdll/ppc -I./inc_stub \
    -c ../../ntos/ppc/ntoskrnl.c -o obj_le/ntoskrnl.o
# ... repeat for ppcinit, pcr, exceptn, HAL, NTDLL ...

# Link ELF32 little-endian
powerpc64le-linux-gnu-ld -m elf32lppc -Ttext 0x00000000 -nostdlib -static \
    -o ntoskrnl_le.elf <objects...>

# Verify: 0 compile errors, 0 warnings (-Werror), 0 undefined symbols
powerpc64le-linux-gnu-readelf -h ntoskrnl_le.elf   # Class: ELF32, Data: LSB, Machine: PowerPC
powerpc64le-linux-gnu-nm ntoskrnl_le.elf \| grep " U "   # must be empty
```

Acceptance: `ntos_entry.S` assembles cleanly, `ppc.S` assembles cleanly,
the full kernel links with 0 undefined symbols, and `-Werror` produces no
warnings.

---

## How to test on real Wii

### Prerequisites

- A Nintendo Wii with the Homebrew Channel installed.
- A FAT32-formatted SD card (SD or SDHC, up to 32 GB recommended).
- A PC to format the SD card and copy files.
- The CI-built SD package (`wii_sd/`) produced by `build-wii.yml`.

### Steps

1. **Format the SD card as FAT32** (single partition).
   - Linux: `mkfs.vfat -F 32 /dev/sdX`
   - Windows: `format X: /FS:FAT32`
   - macOS: Disk Utility → Erase → MS-DOS (FAT)

2. **Copy `wii_sd/` contents to the SD card root** (not the `wii_sd/` folder
   itself):

   ```
   sd:/
     README.md
     nt/
       arcfw.elf
     apps/
       arcldr/
         boot.dol
   ```

3. **Insert the SD card into the Wii** and power on.

4. **Launch the Homebrew Channel** from the Wii System Menu. Select `arcldr`
   (from `sd:/apps/arcldr/boot.dol`) and press A.

5. **arcldr** loads `sd:/nt/arcfw.elf`, builds the `PHW_DESCRIPTION` from Wii
   hardware, enters little-endian mode (`MSR_ILE`), and jumps to `_start` via
   `ModeSwitchEntry` with r3 = `PHW_DESCRIPTION` pointer.

---

## Expected behavior

Once the kernel boots, text should appear on screen via the framebuffer driver:

```
Windows XP (NT 5.1) - PowerPC Broadway/G3 Port
Kernel initialized. Boot phase complete.
Controller input initialized.
```

Success criteria:
- Text appears on screen via `HalpDisplayPrintf`.
- "Kernel initialized. Boot phase complete." is printed.
- "Controller input initialized." is printed.
- The Wii does not immediately reboot or crash.
- Each Decrementer tick (vector `0x900`) dispatches through `HalpVectorEntry`
  → `HalpInterruptEntry` → `KiExceptionDispatch` and returns via `rfi`.

### Troubleshooting

**Screen stays black:** Confirm the SD card is FAT32, `boot.dol` and
`arcfw.elf` exist and are non-zero, and you launched `arcldr`. The framebuffer
defaults to 640×480 RGB565 via libogc; try a different display mode/component
cable if text renders off-screen.

**Wii reboots immediately:** Usually a fault during `_start` or
`KeInitializeKernelPpc` (bad vector page, bad PCR setup, or an exception with
no handler). Confirm `arcfw.elf` is ELF32 little-endian (`Class: ELF32`,
`Data: little endian`, `Machine: PowerPC`) linked with `-Ttext 0x00000000`.

**No text appears:** arcldr may have loaded the ELF but the kernel hung before
display init. Confirm the loader block is valid (arcldr build) and that
`PHW_DESCRIPTION` framebuffer base (offset 28) is correct.

---

## Notes

- This is experimental early boot code. The kernel may hang, crash, or reboot.
- arcldr requires a Wii with the Homebrew Channel installed.
- The kernel expects entii's `PHW_DESCRIPTION` loader block in GPR r3.
- There is no shell yet; controller input is initialized but not acted on.
