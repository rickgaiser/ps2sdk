//---------------------------------------------------------------------------
//File name:    main.c
//---------------------------------------------------------------------------
#include <loadcore.h>
#include <stdio.h>
#include <irx.h>
#include <bdm.h>

//#define DEBUG  //comment out this line when not debugging
#include "module_debug.h"


#define MAJOR_VER 1
#define MINOR_VER 0


IRX_ID("bdm", MAJOR_VER, MINOR_VER);


extern struct irx_export_table _exp_bdm;
extern void bdm_init();


int _start( int argc, char *argv[])
{
	printf("Block Device Manager (BDM) v%d.%d\n", MAJOR_VER, MINOR_VER);

	if (RegisterLibraryEntries(&_exp_bdm) != 0) {
		M_PRINTF("ERROR: Already registered!\n");
		return MODULE_NO_RESIDENT_END;
	}

	// initialize the block device manager
	bdm_init();

	// return resident
	return MODULE_RESIDENT_END;
}
