//---------------------------------------------------------------------------
//File name:    bdm.c
//---------------------------------------------------------------------------
#include <stdio.h>
#include <bdm.h>
#include "fat_driver.h"

//#define DEBUG  //comment out this line when not debugging
#include "module_debug.h"


int vfat_connect_bd(struct block_device* bd)
{
	M_DEBUG("vfat_connect_bd\n");

	return fat_mount(bd);
}

void vfat_disconnect_bd(struct block_device* bd)
{
	M_DEBUG("vfat_disconnect_bd\n");

	fat_forceUnmount(bd);
}


static struct file_system g_fs = {
	.priv = NULL,
	.name = "vfat",
	.connect_bd = vfat_connect_bd,
	.disconnect_bd = vfat_disconnect_bd,
};

void bdm_init()
{
	M_DEBUG("bdm_init\n");

	bdm_connect_fs(&g_fs);
}
