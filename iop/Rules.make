# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

# Comma definition for use in $(addprefix)
comma := ,

IOP_OBJS_DIR ?= obj/
IOP_SRC_DIR ?= src/
IOP_INC_DIR ?= include/

# include dir
IOP_INCS := $(IOP_INCS) -I$(IOP_SRC_DIR) -I$(IOP_SRC_DIR)include -I$(IOP_INC_DIR) -I$(PS2SDKSRC)/iop/kernel/include -I$(PS2SDKSRC)/common/include

ifdef IOP_IMPORT_INCS
IOP_INCS += $(addprefix -I$(PS2SDKSRC)/iop/, $(addsuffix /include, $(IOP_IMPORT_INCS)))
endif

# Optimization compiler flags
IOP_OPTFLAGS ?= -Os

# Warning compiler flags
IOP_WARNFLAGS ?= -Wall -Werror

ifeq ($(IOP_USE_LLVM),1)
# =====================
# LLVM/Clang settings
# =====================

# Target flags for IOP (MIPS R3000 - 32-bit little endian)
# -mabi=32: Use O32 ABI (standard 32-bit MIPS ABI)
# -mno-abicalls: Match GCC's default non-PIC code generation
IOP_TARGET_FLAGS := --target=mipsel-none-elf -march=mips1 -mcpu=mips1 -mabi=32 -mno-abicalls

# Debug information flags (LLVM compatible)
IOP_DBGINFOFLAGS ?= -gdwarf-4

# C compiler flags for LLVM/Clang
# -fno-builtin prevents built-in functions from being included
# -msoft-float ensures software floating point (IOP has no FPU)
# -G0 disables small data section optimization
# -mno-explicit-relocs ensures paired HI16/LO16 relocations (required for IRX1 format)
# -mllvm --mno-check-zero-division disables trap instructions for division by zero
#   (required because MIPS1 doesn't have trap instructions)
# -Qunused-arguments silences warnings about unused arguments when used with linker
# -U__mips -D__mips=1 fixes LLVM incorrectly setting __mips=32 for MIPS-I
IOP_CFLAGS := $(IOP_TARGET_FLAGS) -D_IOP -U__mips -D__mips=1 -ffreestanding -fno-builtin -fno-builtin-memset -fno-builtin-memcpy -fno-builtin-memmove -msoft-float -mno-explicit-relocs -G0 -fomit-frame-pointer -mllvm --mno-check-zero-division -Qunused-arguments $(IOP_OPTFLAGS) $(IOP_WARNFLAGS) $(IOP_DBGINFOFLAGS) $(IOP_INCS) $(IOP_CFLAGS)
ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG
endif

# Import/export table flags for LLVM
IOP_IETABLE_CFLAGS :=

# Linker flags for LLVM/Clang with LLD
# -nostdlib: don't link standard libraries
# -Wl,-r: create relocatable output
# -Wl,--no-gc-sections: keep all sections
IOP_LDFLAGS := -nostdlib -fuse-ld=lld -Wl,-r -Wl,--no-gc-sections $(IOP_LDFLAGS)

# Assembler flags for LLVM (using clang as assembler)
IOP_ASFLAGS := $(IOP_TARGET_FLAGS) -msoft-float $(IOP_ASFLAGS)

# Default link file for LLVM (LLD-compatible)
IOP_LINKFILE_DEFAULT := $(PS2SDKSRC)/iop/startup/src/linkfile.lld

else
# =====================
# GCC settings (default)
# =====================

IOP_CC_VERSION := $(shell $(IOP_CC) -dumpversion)

ifeq ($(IOP_CC_VERSION),3.2.2)
ASFLAGS_TARGET = -march=r3000
endif

ifeq ($(IOP_CC_VERSION),3.2.3)
ASFLAGS_TARGET = -march=r3000
endif

# Debug information flags
IOP_DBGINFOFLAGS ?= -gdwarf-2 -gz

# C compiler flags
# -fno-builtin is required to prevent the GCC built-in functions from being included,
#   for finer-grained control over what goes into each IRX.
IOP_CFLAGS := -D_IOP -fno-builtin -G0 $(IOP_OPTFLAGS) $(IOP_WARNFLAGS) $(IOP_DBGINFOFLAGS) $(IOP_INCS) $(IOP_CFLAGS)
ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG
endif
# Linker flags
IOP_LDFLAGS := -nostdlib -dc -r $(IOP_LDFLAGS)

# Additional C compiler flags for GCC >=v5.3.0
# -msoft-float is to "remind" GCC/Binutils that the soft-float ABI is to be used. This is due to a bug, which
#   results in the ABI not being passed correctly to binutils and iop-as defaults to the hard-float ABI instead.
# -mno-explicit-relocs is required to work around the fact that GCC is now known to
#   output multiple LO relocs after one HI reloc (which the IOP kernel cannot deal with).
# -fno-toplevel-reorder (for IOP import and export tables only) disables toplevel reordering by GCC v4.2 and later.
#   Without it, the import and export tables can be broken apart by GCC's optimizations.
ifneq ($(IOP_CC_VERSION),3.2.2)
ifneq ($(IOP_CC_VERSION),3.2.3)
IOP_CFLAGS += -msoft-float -mno-explicit-relocs
IOP_IETABLE_CFLAGS := -fno-toplevel-reorder
endif
endif

# If gpopt is requested, use it if the GCC version is compatible
ifneq (x$(IOP_PREFER_GPOPT),x)
ifeq ($(IOP_CC_VERSION),3.2.2)
IOP_CFLAGS += -DUSE_GP_REGISTER=1 -mgpopt -G$(IOP_PREFER_GPOPT)
endif
ifeq ($(IOP_CC_VERSION),3.2.3)
IOP_CFLAGS += -DUSE_GP_REGISTER=1 -mgpopt -G$(IOP_PREFER_GPOPT)
endif
endif

# Assembler flags
IOP_ASFLAGS := $(ASFLAGS_TARGET) -EL -G0 $(IOP_ASFLAGS)

endif

# Default link file
ifeq ($(IOP_LINKFILE),)
ifeq ($(IOP_USE_LLVM),1)
IOP_LINKFILE := $(PS2SDKSRC)/iop/startup/src/linkfile.lld
else
IOP_LINKFILE := $(PS2SDKSRC)/iop/startup/src/linkfile
endif
endif

IOP_OBJS := $(IOP_OBJS:%=$(IOP_OBJS_DIR)%)

IOP_BIN_ELF := $(IOP_BIN:.irx=.notiopmod.elf)

IOP_BIN_STRIPPED_ELF := $(IOP_BIN:.irx=.notiopmod.stripped.elf)

# Externally defined variables: IOP_BIN, IOP_OBJS, IOP_LIB

# These macros can be used to simplify certain build rules.
IOP_C_COMPILE = $(IOP_CC) $(IOP_CFLAGS)

# Command for ensuring the output directory for the rule exists.
DIR_GUARD = @$(MKDIR) -p $(@D)

MAKE_CURPID := $(shell printf '%s' $$PPID)

$(IOP_OBJS_DIR)%.o: $(IOP_SRC_DIR)%.c
	$(DIR_GUARD)
	$(IOP_C_COMPILE) -c $< -o $@

$(IOP_OBJS_DIR)%.o: $(IOP_SRC_DIR)%.S
	$(DIR_GUARD)
	$(IOP_C_COMPILE) -c $< -o $@

ifeq ($(IOP_USE_LLVM),1)
# For LLVM, use clang as assembler (requires -c flag)
$(IOP_OBJS_DIR)%.o: $(IOP_SRC_DIR)%.s
	$(DIR_GUARD)
	$(IOP_AS) $(IOP_ASFLAGS) -c $< -o $@
else
# For GCC, use binutils as (no -c flag needed)
$(IOP_OBJS_DIR)%.o: $(IOP_SRC_DIR)%.s
	$(DIR_GUARD)
	$(IOP_AS) $(IOP_ASFLAGS) $< -o $@
endif

.INTERMEDIATE:: $(IOP_LIB)_tmp$(MAKE_CURPID) $(IOP_OBJS_DIR)build-imports.c $(IOP_OBJS_DIR)build-exports.c

$(PS2SDKSRC)/tools/srxfixup/bin/srxfixup: $(PS2SDKSRC)/tools/srxfixup
	$(MAKEREC) $<

$(IOP_OBJS_DIR)template-imports.h:
	$(DIR_GUARD)
	$(PRINTF) '%s\n' "#include \"irx_imports.h\"" > $@

# Rules to build imports.lst.
$(IOP_OBJS_DIR)build-imports.c: $(IOP_OBJS_DIR)template-imports.h $(IOP_SRC_DIR)imports.lst
	$(DIR_GUARD)
	cat $^ > $@

$(IOP_OBJS_DIR)imports.o: $(IOP_OBJS_DIR)build-imports.c
	$(DIR_GUARD)
	$(IOP_C_COMPILE) $(IOP_IETABLE_CFLAGS) -c $< -o $@

$(IOP_OBJS_DIR)template-exports.h:
	$(DIR_GUARD)
	$(PRINTF) '%s\n' "#include \"irx.h\"" > $@

# Rules to build exports.tab.
$(IOP_OBJS_DIR)build-exports.c: $(IOP_OBJS_DIR)template-exports.h $(IOP_SRC_DIR)exports.tab
	$(DIR_GUARD)
	cat $^ > $@

$(IOP_OBJS_DIR)exports.o: $(IOP_OBJS_DIR)build-exports.c
	$(DIR_GUARD)
	$(IOP_C_COMPILE) $(IOP_IETABLE_CFLAGS) -c $< -o $@

ifeq ($(IOP_USE_LLVM),1)
# For LLVM, use clang as linker driver with lld
$(IOP_BIN_ELF): $(IOP_OBJS) $(IOP_LIB_ARCHIVES) $(IOP_ADDITIONAL_DEPS)
	$(DIR_GUARD)
	$(IOP_C_COMPILE) -Wl,-T,$(IOP_LINKFILE) $(IOP_OPTFLAGS) -o $@ $(IOP_OBJS) $(IOP_LDFLAGS) $(IOP_LIB_ARCHIVES) $(IOP_LIBS)
else
# For GCC, use gcc as linker driver
$(IOP_BIN_ELF): $(IOP_OBJS) $(IOP_LIB_ARCHIVES) $(IOP_ADDITIONAL_DEPS)
	$(DIR_GUARD)
	$(IOP_C_COMPILE) -T$(IOP_LINKFILE) $(IOP_OPTFLAGS) -o $@ $(IOP_OBJS) $(IOP_LDFLAGS) $(IOP_LIB_ARCHIVES) $(IOP_LIBS)
endif

ifeq ($(IOP_USE_LLVM),1)
# For LLVM, also remove LLVM-specific sections
$(IOP_BIN_STRIPPED_ELF): $(IOP_BIN_ELF)
	$(DIR_GUARD)
	$(IOP_STRIP) --strip-unneeded --remove-section=.pdr --remove-section=.comment --remove-section=.mdebug.abi32 --remove-section=.gnu.attributes --remove-section=.llvm_addrsig --remove-section=.note.GNU-stack -o $@ $<
else
$(IOP_BIN_STRIPPED_ELF): $(IOP_BIN_ELF)
	$(DIR_GUARD)
	$(IOP_STRIP) --strip-unneeded --remove-section=.pdr --remove-section=.comment --remove-section=.mdebug.abi32 --remove-section=.gnu.attributes -o $@ $<
endif

# Use IRX1 format for both GCC and LLVM (IRX2 not supported by PS2 loadcore)
$(IOP_BIN): $(IOP_BIN_STRIPPED_ELF) $(PS2SDKSRC)/tools/srxfixup/bin/srxfixup
	$(PS2SDKSRC)/tools/srxfixup/bin/srxfixup --irx1 -o $@ $<

$(IOP_LIB)_tmp$(MAKE_CURPID): $(IOP_OBJS)
	$(DIR_GUARD)
	$(IOP_AR) cru $@ $(IOP_OBJS)

$(IOP_LIB): $(IOP_LIB)_tmp$(MAKE_CURPID)
	$(DIR_GUARD)
	mv $< $@
