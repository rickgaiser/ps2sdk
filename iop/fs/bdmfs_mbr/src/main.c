//---------------------------------------------------------------------------
//File name:    main.c
//---------------------------------------------------------------------------
#include <loadcore.h>
#include <stdio.h>
#include <irx.h>


#define MAJOR_VER 1
#define MINOR_VER 0


IRX_ID("bdmmbr", MAJOR_VER, MINOR_VER);


extern void part_init();


int _start(int argc, char *argv[])
{
	printf("BDM MBR partition driver v%d.%d\n", MAJOR_VER, MINOR_VER);

	// initialize the partition driver
	part_init();

	// return resident
	return MODULE_RESIDENT_END;
}
