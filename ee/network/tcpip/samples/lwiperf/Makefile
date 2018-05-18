# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

# Change this to point to the version of LWIP this is being compiled with.
LWIP = $(PS2SDKSRC)/common/tcpip/lwip-2.0.0

EE_BIN = tcpip_lwiperf.elf
EE_OBJS = ps2ip.o lwiperf.o urxperf.o EEDEBUG_irx.o DEV9_irx.o NETMAN_irx.o SMAP_irx.o
EE_INCS = -I$(LWIP)/src/include -I$(LWIP)/src/include/ipv4 -I$(PS2SDKSRC)/ee/network/tcpip/src/include
EE_CFLAGS = -DLWIP_NOASSERT -DLWIP_COMPAT_MUTEX -DLWIP_COMPAT_MUTEX_ALLOWED
EE_LIBS = -lnetman -lps2ip -ldebug -lpatches

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) DEV9_irx.c NETMAN_irx.c SMAP_irx.c

EEDEBUG_irx.c: $(PS2SDK)/iop/irx/eedebug.irx
	bin2c $< EEDEBUG_irx.c EEDEBUG_irx

DEV9_irx.c: $(PS2SDK)/iop/irx/ps2dev9.irx
	bin2c $< DEV9_irx.c DEV9_irx

NETMAN_irx.c: $(PS2SDK)/iop/irx/netman.irx
	bin2c $< NETMAN_irx.c NETMAN_irx

SMAP_irx.c: $(PS2SDK)/iop/irx/smap.irx
	bin2c $< SMAP_irx.c SMAP_irx

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
