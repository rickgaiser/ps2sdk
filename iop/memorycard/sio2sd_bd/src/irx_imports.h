/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$
# Defines all IRX imports.
*/

#ifndef IOP_IRX_IMPORTS_H
#define IOP_IRX_IMPORTS_H

#include "irx.h"

/* Please keep these in alphabetical order!  */
#include "intrman.h"
#include "loadcore.h"
#include "stdio.h"
#include "sysclib.h"
#include "sysmem.h"
#include "thbase.h"
#include "thevent.h"
#include "thmsgbx.h"
#include "thsemap.h"
#include "vblank.h"
#include "ioman.h"

#include "bdm.h"
//#include "sio2man.h"



// IRX Imports
#define sio2man_IMPORTS_start DECLARE_IMPORT_TABLE(sio2man, 1, 2)
#define sio2man_IMPORTS_end END_IMPORT_TABLE


#define I_sio2_pad_transfer_init DECLARE_IMPORT(23, sio2_pad_transfer_init)
#define I_sio2_transfer DECLARE_IMPORT(25, sio2_transfer)
#define I_sio2_transfer_reset DECLARE_IMPORT(26, sio2_transfer_reset)



//#include "../mips-iop_ppcComMain/emuModComIfIop.h"

#endif /* IOP_IRX_IMPORTS_H */
