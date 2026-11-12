#!/bin/sh
# PowerPC build harness for Broadway/G3 (PowerPC 750-class).
# Invokes powerpc-linux-gnu-gcc with -mcpu=750 -mno-altivec -static on
# hello.c, then inspects the ELF header with powerpc-linux-gnu-readelf -h.
set -eu

CC=powerpc-linux-gnu-gcc
READELF=powerpc-linux-gnu-readelf
SRC=hello.c
OUT=hello.ppc

echo "==> Compiling $SRC with $CC -mcpu=750 -mno-altivec -static"
"$CC" -mcpu=750 -mno-altivec -static -o "$OUT" "$SRC"

echo "==> Inspecting ELF header of $OUT"
"$READELF" -h "$OUT"
