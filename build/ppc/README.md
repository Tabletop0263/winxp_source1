# PowerPC Build Harness

This directory holds a minimal PowerPC build harness for the Broadway/PowerPC G3 port of Windows XP (NT 5.1). It is built against the generic `powerpc-linux-gnu` cross toolchain (Ubuntu 15.2.0 `powerpc-linux-gnu-gcc`), not a certified Broadway/G3 cross-compiler. The `-mcpu=750` flag targets the G3-class (PowerPC 750) core, and `-mno-altivec` disables AltiVec since the G3 does not support it.

## Commands run

```sh
cd /root/winxp_source/build/ppc
chmod +x toolchain.sh
./toolchain.sh
```

`toolchain.sh` compiles `hello.c` with:

```sh
powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -static -o hello.ppc hello.c
```

then runs:

```sh
powerpc-linux-gnu-readelf -h hello.ppc
```

## readelf output

```
ELF Header:
  Magic:   7f 45 4c 46 01 02 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF32
  Data:                              2's complement, big endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              EXEC (Executable file)
  Machine:                           PowerPC
  Version:                           0x1
  Entry point address:               0x10000488
  Start of program headers:          52 (bytes into file)
  Start of section headers:          714756 (bytes into file)
  Flags:                             0x0
  Size of this header:               52 (bytes)
  Size of program headers:           32 (bytes)
  Number of program headers:         7
  Size of section headers:           40 (bytes)
  Number of section headers:         25
  Section header string table index: 24
```

Result: `Class: ELF32` and `Machine: PowerPC`, confirming the cross toolchain emits PowerPC ELF32 binaries (big endian, as expected for PowerPC).
