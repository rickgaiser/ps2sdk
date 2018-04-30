//---------------------------------------------------------------------------
//File name:    fs_driver.c
//---------------------------------------------------------------------------
/*
 * fat_driver.c - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 * (C) 2004  Hermes (support for sector sizes from 512 to 4096 bytes)
 * (C) 2004  raipsu (fs_dopen, fs_dclose, fs_dread, fs_getstat implementation)
 *
 * EXT2 filesystem layer
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */
//---------------------------------------------------------------------------
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <ioman.h>

#ifdef WIN32
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#else
#include <sysclib.h>
#include <thsemap.h>
#include <thbase.h>
#include <sysmem.h>
#endif

#include <usbhdfsd-common.h>
#include "common.h"
#include "ext2fs.h"

//#define DEBUG  //comment out this line when not debugging
#include "module_debug.h"


/*************************************************************************************/
/*    File IO functions                                                              */
/*************************************************************************************/

//---------------------------------------------------------------------------
static int _lock_sema_id = -1;

//---------------------------------------------------------------------------
static int _fs_init_lock(void)
{
#ifndef WIN32
	iop_sema_t sp;

	sp.initial = 1;
	sp.max = 1;
	sp.option = 0;
	sp.attr = 0;
	if((_lock_sema_id = CreateSema(&sp)) < 0) { return(-1); }
#endif

	return(0);
}

//---------------------------------------------------------------------------
void _fs_lock(void)
{
#ifndef WIN32
	WaitSema(_lock_sema_id);
#endif
}

//---------------------------------------------------------------------------
void _fs_unlock(void)
{
#ifndef WIN32
	SignalSema(_lock_sema_id);
#endif
}

//---------------------------------------------------------------------------
static void fs_reset(void)
{
#ifndef WIN32
	if(_lock_sema_id >= 0) { DeleteSema(_lock_sema_id); }
#endif
	_fs_init_lock();
}

//---------------------------------------------------------------------------
static int fs_inited = 0;

//---------------------------------------------------------------------------
static int fs_dummy(void)
{
	return -5;
}

//---------------------------------------------------------------------------
static int fs_init(iop_device_t *driver)
{
	if(!fs_inited) {
		fs_reset();
		fs_inited = 1;
	}

	return 1;
}

#ifndef WIN32
static iop_device_ops_t fs_functarray={
	&fs_init,
	(void*)&fs_dummy,
	(void*)&fs_dummy,
	&ext2_fs_open,
	&ext2_fs_close,
	&ext2_fs_read,
	&ext2_fs_write,
	&ext2_fs_lseek,
	&ext2_fs_ioctl,
	&ext2_fs_remove,
	&ext2_fs_mkdir,
	&ext2_fs_rmdir,
	&ext2_fs_dopen,
	&ext2_fs_closedir,
	&ext2_fs_dread,
	&ext2_fs_getstat,
	(void*)&fs_dummy
};
static iop_device_t fs_driver={
	"mass",
	IOP_DT_FS,
	2,
	"EXT2 driver",
	&fs_functarray
};

/* init file system driver */
int InitFS(void)
{
	DelDrv("mass");
	return(AddDrv(&fs_driver) == 0 ? 0 : -1);
}
#endif

//---------------------------------------------------------------------------
//End of file:  fs_driver.c
//---------------------------------------------------------------------------
