# Broadway/PowerPC G3 Boot Loader Porting Notes

**Target:** Windows XP (NT 5.1) source port to Broadway/PowerPC G3  
**Lane:** `boot`  
**Compiler:** `powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec`  
**Scope:** `base/boot/*/ppc/*` only (no other lanes touched).

## Surveyed i386 directories

### base/boot/bldr/i386/ (3 files)

| i386 file | PPC stub counterpart | Status |
|-----------|---------------------|--------|
| initx86.c | base/boot/bldr/ppc/initx86.c | C stub created, compiles to PowerPC ELF |
| parsboot.c | base/boot/bldr/ppc/parsboot.c | C stub created, compiles to PowerPC ELF |
| spcrtest.c | base/boot/bldr/ppc/spcrtest.c | C stub created, compiles to PowerPC ELF |

### base/boot/startup/i386/ (43 files)

| i386 file | PPC counterpart | Status |
|-----------|----------------|--------|
| a20.asm | base/boot/startup/ppc/a20.asm | Assembly — no C stub (kept as reference) |
| backend.asm | base/boot/startup/ppc/backend.asm | Assembly — no C stub (kept as reference) |
| br/strings.h | base/boot/startup/ppc/br/strings.h | Header — no C stub |
| constant.h | base/boot/startup/ppc/constant.h | Header — no C stub |
| cs/strings.h | base/boot/startup/ppc/cs/strings.h | Header — no C stub |
| da/strings.h | base/boot/startup/ppc/da/strings.h | Header — no C stub |
| display.c | base/boot/startup/ppc/display.c | C stub created, compiles to PowerPC ELF |
| eisa.h | base/boot/startup/ppc/eisa.h | Header — no C stub |
| eisa.inc | base/boot/startup/ppc/eisa.inc | Include — no C stub |
| eisaa.asm | base/boot/startup/ppc/eisaa.asm | Assembly — no C stub (kept as reference) |
| eisac.c | base/boot/startup/ppc/eisac.c | C stub created, compiles to PowerPC ELF |
| el/strings.h | base/boot/startup/ppc/el/strings.h | Header — no C stub |
| es/strings.h | base/boot/startup/ppc/es/strings.h | Header — no C stub |
| exp.asm | base/boot/startup/ppc/exp.asm | Assembly — no C stub (kept as reference) |
| fi/strings.h | base/boot/startup/ppc/fi/strings.h | Header — no C stub |
| fr/strings.h | base/boot/startup/ppc/fr/strings.h | Header — no C stub |
| ger/strings.h | base/boot/startup/ppc/ger/strings.h | Header — no C stub |
| global.h | base/boot/startup/ppc/global.h | Header — no C stub |
| hu/strings.h | base/boot/startup/ppc/hu/strings.h | Header — no C stub |
| it/strings.h | base/boot/startup/ppc/it/strings.h | Header — no C stub |
| macro.inc | base/boot/startup/ppc/macro.inc | Include — no C stub |
| main.c | base/boot/startup/ppc/main.c | C stub created, compiles to PowerPC ELF |
| memmap.h | base/boot/startup/ppc/memmap.h | Header — no C stub |
| memmap.inc | base/boot/startup/ppc/memmap.inc | Include — no C stub |
| nl/strings.h | base/boot/startup/ppc/nl/strings.h | Header — no C stub |
| no/strings.h | base/boot/startup/ppc/no/strings.h | Header — no C stub |
| ntmisc.h | base/boot/startup/ppc/ntmisc.h | Header — no C stub |
| pl/strings.h | base/boot/startup/ppc/pl/strings.h | Header — no C stub |
| pt/strings.h | base/boot/startup/ppc/pt/strings.h | Header — no C stub |
| ru/strings.h | base/boot/startup/ppc/ru/strings.h | Header — no C stub |
| su.asm | base/boot/startup/ppc/su.asm | Assembly — no C stub (kept as reference) |
| su.h | base/boot/startup/ppc/su.h | Header — no C stub |
| su.inc | base/boot/startup/ppc/su.inc | Include — no C stub |
| sudata.asm | base/boot/startup/ppc/sudata.asm | Assembly — no C stub (kept as reference) |
| sv/strings.h | base/boot/startup/ppc/sv/strings.h | Header — no C stub |
| tr/strings.h | base/boot/startup/ppc/tr/strings.h | Header — no C stub |
| trap.asm | base/boot/startup/ppc/trap.asm | Assembly — no C stub (kept as reference) |
| trapdump.c | base/boot/startup/ppc/trapdump.c | C stub created, compiles to PowerPC ELF |
| types.h | base/boot/startup/ppc/types.h | Header — no C stub |
| usa/strings.h | base/boot/startup/ppc/usa/strings.h | Header — no C stub |

## C stubs created (7 total)

### base/boot/bldr/ppc/
- `initx86.c` — stub for `base/boot/bldr/i386/initx86.c`
- `parsboot.c` — stub for `base/boot/bldr/i386/parsboot.c`
- `spcrtest.c` — stub for `base/boot/bldr/i386/spcrtest.c`

### base/boot/startup/ppc/
- `display.c` — stub for `base/boot/startup/i386/display.c`
- `eisac.c` — stub for `base/boot/startup/i386/eisac.c`
- `main.c` — stub for `base/boot/startup/i386/main.c`
- `trapdump.c` — stub for `base/boot/startup/i386/trapdump.c`

## Stub contract

Every `.c` stub is:
- Self-contained (defines its own constants via `#define`).
- Provides a `int main(void)` entry point returning `0`.
- Compiles under `powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c`.

## Compile commands (run from each ppc directory)

```bash
cd base/boot/bldr/ppc
powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c initx86.c -o initx86.o
powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c parsboot.c -o parsboot.o
powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c spcrtest.c -o spcrtest.o

cd base/boot/startup/ppc
powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c display.c -o display.o
powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c eisac.c -o eisac.o
powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c main.c -o main.o
powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c trapdump.c -o trapdump.o
```

## readelf confirmation

Each `.o` produced above reports:

```
  Machine:                           PowerPC
```

Verified with `readelf -h <file>.o | grep -i machine` for all 7 stubs.

## Notes on non-C i386 files

The startup/i386 directory also contains `.asm` (assembly), `.h` (headers),
`.inc` (assembler includes), and `*/strings.h` (localized string headers).
These are not C sources and fall outside the "minimal compilable C stub"
requirement. They are listed above for mapping completeness and are left as
reference counterparts under `startup/ppc/` (no files written for them).
