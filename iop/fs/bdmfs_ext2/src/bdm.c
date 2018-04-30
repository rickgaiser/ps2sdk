//---------------------------------------------------------------------------
//File name:    bdm.c
//---------------------------------------------------------------------------
#include <stdio.h>
#include <bdm.h>
#include "ext2fs.h"

//#define DEBUG  //comment out this line when not debugging
#include "module_debug.h"


int ext2_connect_bd(struct block_device* bd)
{
	M_DEBUG("ext2_connect_bd\n");

	return ext2_mount(bd);
}

void ext2_disconnect_bd(struct block_device* bd)
{
	M_DEBUG("ext2_disconnect_bd\n");

	ext2_umount(bd);
}


static struct file_system g_fs = {
	.priv = NULL,
	.name = "ext2",
	.connect_bd = ext2_connect_bd,
	.disconnect_bd = ext2_disconnect_bd,
};

void bdm_init()
{
	M_DEBUG("bdm_init\n");

	bdm_connect_fs(&g_fs);
}
