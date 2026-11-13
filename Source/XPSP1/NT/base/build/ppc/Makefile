# Integrated PowerPC (Broadway/G3) build target for the NT 5.1 port.
#
# Compiles HAL, NTDLL, and kernel arch sources into a single PowerPC ELF32
# kernel image (ntoskrnl.elf). Links with -Ttext 0x00000000 so the vector
# page sits at 0x000 (classic PowerPC exception vectors).
#
#   make            - build ntoskrnl.elf
#   make clean      - remove objects and the ELF
#
# Toolchain: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec

CC      := powerpc-linux-gnu-gcc
AS      := powerpc-linux-gnu-as
LD      := powerpc-linux-gnu-ld
OBJCOPY := powerpc-linux-gnu-objcopy

ARCH_ROOT := /root/winxp_source/Source/XPSP1/NT/base
HAL       := $(ARCH_ROOT)/hals/halppc/ppc
NTDLL     := $(ARCH_ROOT)/ntdll/ppc
NTOS      := $(ARCH_ROOT)/ntos/ppc
INC_PPC   := $(ARCH_ROOT)/ntos/inc/ppc
INC_NTOS  := $(ARCH_ROOT)/ntos/inc
INC_NTDLL := $(ARCH_ROOT)/ntdll/ppc
INC_CONFIG:= $(ARCH_ROOT)/ntos/config

CFLAGS  := -mcpu=750 -mno-altivec -Wall -Werror -O2 -ffreestanding \
           -DPOWERPC -D_G3_ -DBROADWAY -DNT_UP -DNT_KERNEL -D_NTSYSTEM_ \
           -I$(INC_PPC) -I$(INC_NTOS) -I$(INC_NTDLL) -I$(INC_CONFIG) \
           -I$(HAL)
ASFLAGS :=
LDFLAGS := -Ttext 0x00000000 -nostdlib -static

OBJDIR := obj
ELF    := ntoskrnl.elf

# ---- Explicit object lists (avoid pattern-rule conflicts between .c/.S) ----

HAL_OBJS := $(OBJDIR)/hal_halppc.o $(OBJDIR)/hal_atomic.o \
            $(OBJDIR)/hal_interrupt.o $(OBJDIR)/hal_context.o \
            $(OBJDIR)/hal_of.o $(OBJDIR)/hal_display.o $(OBJDIR)/hal_font.o \
            $(OBJDIR)/hal_pad.o $(OBJDIR)/hal_disk.o $(OBJDIR)/hal_fat.o

NTDLL_OBJS := $(OBJDIR)/ntdll_context.o $(OBJDIR)/ntdll_dispatch.o \
              $(OBJDIR)/ntdll_critsect.o $(OBJDIR)/ntdll_ldrctx.o \
              $(OBJDIR)/ntdll_ldrthunk.o $(OBJDIR)/ntdll_npxnp.o

NTOS_OBJS := $(OBJDIR)/ntos_ppcinit.o $(OBJDIR)/ntos_exceptn.o \
             $(OBJDIR)/ntos_pcr.o $(OBJDIR)/ntos_ppc.o \
             $(OBJDIR)/ntoskrnl.o

# New port modules: process/thread, object, LPC, shell.
NTOS_PS_OBJS := $(OBJDIR)/ntos_ps_pslist.o $(OBJDIR)/ntos_ps_pscreate.o \
                $(OBJDIR)/ntos_ps_psexit.o

NTOS_OB_OBJS := $(OBJDIR)/ntos_ob_obcreate.o $(OBJDIR)/ntos_ob_obname.o

NTOS_LPC_OBJS := $(OBJDIR)/ntos_lpc_lpc.o $(OBJDIR)/ntos_lpc_lpcport.o

NTOS_CMD_OBJS := $(OBJDIR)/ntos_cmd_cmdmain.o $(OBJDIR)/ntos_cmd_cmds.o \
                 $(OBJDIR)/ntos_cmd_console.o

CONFIG_OBJS := $(OBJDIR)/config_cmregistry.o $(OBJDIR)/config_cmkey.o \
               $(OBJDIR)/config_cmvalue.o $(OBJDIR)/config_cmcallback.o

# Kernel entry stub (hand-written asm, not a source file in the tree).
NTOS_ENTRY := $(OBJDIR)/ntos_entry.o

ALL_OBJS := $(HAL_OBJS) $(NTDLL_OBJS) $(NTOS_OBJS) $(CONFIG_OBJS) \
            $(NTOS_PS_OBJS) $(NTOS_OB_OBJS) $(NTOS_LPC_OBJS) $(NTOS_CMD_OBJS) \
            $(NTOS_ENTRY)

.PHONY: all clean
all: $(ELF)

# ---- Compile rules ---------------------------------------------------------

$(OBJDIR)/hal_halppc.o: $(HAL)/halppc.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/hal_atomic.o: $(HAL)/atomic.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/hal_interrupt.o: $(HAL)/interrupt.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/hal_context.o: $(HAL)/context.S | $(OBJDIR)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJDIR)/hal_of.o: $(HAL)/of.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/hal_display.o: $(HAL)/display.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/hal_font.o: $(HAL)/font8x8.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/hal_pad.o: $(HAL)/pad.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/hal_disk.o: $(HAL)/disk.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/hal_fat.o: $(HAL)/fat.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntdll_context.o: $(NTDLL)/context.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntdll_dispatch.o: $(NTDLL)/dispatch.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntdll_critsect.o: $(NTDLL)/critsect.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntdll_ldrctx.o: $(NTDLL)/ldrctx.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntdll_ldrthunk.o: $(NTDLL)/ldrthunk.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntdll_npxnp.o: $(NTDLL)/npxnp.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_ppcinit.o: $(NTOS)/ppcinit.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_exceptn.o: $(NTOS)/exceptn.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_pcr.o: $(NTOS)/pcr.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_ppc.o: $(NTOS)/ppc.S | $(OBJDIR)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJDIR)/ntoskrnl.o: $(NTOS)/ntoskrnl.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/config_cmregistry.o: $(ARCH_ROOT)/ntos/config/cmregistry.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/config_cmkey.o: $(ARCH_ROOT)/ntos/config/cmkey.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/config_cmvalue.o: $(ARCH_ROOT)/ntos/config/cmvalue.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/config_cmcallback.o: $(ARCH_ROOT)/ntos/config/cmcallback.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ---- New port module compile rules --------------------------------------

$(OBJDIR)/ntos_ps_pslist.o: $(ARCH_ROOT)/ntos/ps/pslist.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_ps_pscreate.o: $(ARCH_ROOT)/ntos/ps/pscreate.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_ps_psexit.o: $(ARCH_ROOT)/ntos/ps/psexit.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_ob_obcreate.o: $(ARCH_ROOT)/ntos/ob/obcreate.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_ob_obname.o: $(ARCH_ROOT)/ntos/ob/obname.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_lpc_lpc.o: $(ARCH_ROOT)/ntos/lpc/lpc.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_lpc_lpcport.o: $(ARCH_ROOT)/ntos/lpc/lpcport.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_cmd_cmdmain.o: $(ARCH_ROOT)/ntos/cmd/cmd/cmdmain.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_cmd_cmds.o: $(ARCH_ROOT)/ntos/cmd/cmd/cmds.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ntos_cmd_console.o: $(ARCH_ROOT)/ntos/cmd/cmd/console.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(NTOS_ENTRY): ntos_entry.S | $(OBJDIR)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

# ---- Link -----------------------------------------------------------------

$(ELF): $(ALL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)
	$(OBJCOPY) -O binary -S $@ ntoskrnl.bin

clean:
	rm -rf $(OBJDIR) $(ELF) ntoskrnl.bin
