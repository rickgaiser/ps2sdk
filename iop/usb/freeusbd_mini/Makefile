# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

IOP_SRC_DIR = $(PS2SDKSRC)/iop/usb/freeusbd/src/

IOP_CFLAGS += -DMINI_DRIVER
IOP_IMPORT_INCS += usb/freeusbd/
IOP_BIN ?= usbd_mini.irx

include $(PS2SDKSRC)/iop/usb/freeusbd/Makefile
