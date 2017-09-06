//---------------------------------------------------------------------------
//File name:    bdm.c
//---------------------------------------------------------------------------
#include <stdio.h>
#include <bdm.h>

//#define DEBUG  //comment out this line when not debugging
#include "module_debug.h"


struct bdm_mounts {
	struct block_device* bd;
	struct file_system* fs;
};

#define MAX_CONNECTIONS 10
static struct bdm_mounts g_mount[MAX_CONNECTIONS];
static struct file_system* g_fs[MAX_CONNECTIONS];


static void bdm_try_mount(struct bdm_mounts* mount)
{
	int i;

	M_DEBUG("bdm_try_mount\n");

	for (i = 0; i < MAX_CONNECTIONS; ++i) {
		if (g_fs[i] != NULL) {
			if (g_fs[i]->connect_bd(mount->bd) == 0) {
				M_PRINTF("%s%dp%d mounted to %s\n", mount->bd->name, mount->bd->devNr, mount->bd->parNr, g_fs[i]->name);
				mount->fs = g_fs[i];
				break;
			}
		}
	}
}

void bdm_connect_bd(struct block_device* bd)
{
	int i;

	M_PRINTF("connecting device %s%dp%d\n", bd->name, bd->devNr, bd->parNr);

	for (i = 0; i < MAX_CONNECTIONS; ++i) {
		if (g_mount[i].bd == NULL) {
			g_mount[i].bd = bd;
			bdm_try_mount(&g_mount[i]);
			break;
		}
	}
}

void bdm_disconnect_bd(struct block_device* bd)
{
	int i;

	M_PRINTF("disconnecting device %s%dp%d\n", bd->name, bd->devNr, bd->parNr);

	for (i = 0; i < MAX_CONNECTIONS; ++i) {
		if (g_mount[i].bd == bd) {
			g_mount[i].fs->disconnect_bd(bd);
			M_PRINTF("%s%dp%d unmounted from %s\n", bd->name, bd->devNr, bd->parNr, g_mount[i].fs->name);
			g_mount[i].bd = NULL;
			g_mount[i].fs = NULL;
		}
	}
}

void bdm_connect_fs(struct file_system* fs)
{
	int i;

	M_PRINTF("connecting fs %s\n", fs->name);

	for (i = 0; i < MAX_CONNECTIONS; ++i) {
		if (g_fs[i] == NULL) {
			g_fs[i] = fs;
			break;
		}
	}

	// Try to use FS for unmounted block devices
	for (i = 0; i < MAX_CONNECTIONS; ++i) {
		if ((g_mount[i].bd != NULL) && (g_mount[i].fs == NULL)) {
			if (fs->connect_bd(g_mount[i].bd) == 0) {
				M_PRINTF("%s%dp%d mounted to %s\n", g_mount[i].bd->name, g_mount[i].bd->devNr, g_mount[i].bd->parNr, fs->name);
				g_mount[i].fs = fs;
			}
		}
	}
}

void bdm_disconnect_fs(struct file_system* fs)
{
	int i;

	M_PRINTF("disconnecting fs %s\n", fs->name);

	for (i = 0; i < MAX_CONNECTIONS; ++i) {
		if (g_fs[i] == fs) {
			//
			// TODO: unmount
			//
			g_fs[i] = NULL;
		}
	}
}

void bdm_get_bd(struct block_device** pbd, unsigned int count)
{
	int i;

	// Fill pointer array with block device pointers
	for (i = 0; i < count && i < MAX_CONNECTIONS; i++)
		pbd[i] = g_mount[i].bd;
}

void bdm_init()
{
	int i;

	M_DEBUG("bdm_init\n");

	for (i = 0; i < MAX_CONNECTIONS; ++i) {
		g_mount[i].bd = NULL;
		g_mount[i].fs = NULL;
		g_fs[i] = NULL;
	}
}
