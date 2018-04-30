//---------------------------------------------------------------------------
//File name:    main.c
//---------------------------------------------------------------------------
#include <loadcore.h>
#include <intrman.h>
#include <stdio.h>
#include <sysmem.h>
#include <irx.h>

//#define DEBUG  //comment out this line when not debugging
#include "module_debug.h"


#define MAJOR_VER 1
#define MINOR_VER 1


IRX_ID("bdmvfat", MAJOR_VER, MINOR_VER);


extern int InitFS();
extern void bdm_init();


int _start(int argc, char *argv[])
{
	printf("BDM EXT2 driver v%d.%d\n", MAJOR_VER, MINOR_VER);

	// initialize the file system driver
	if (InitFS() != 0) {
		M_DEBUG("Error initializing FS driver!\n");
		return MODULE_NO_RESIDENT_END;
	}

	// Connect to block device manager
	bdm_init();

	// return resident
	return MODULE_RESIDENT_END;
}

#ifndef WIN32
void *malloc(int size){
	void *result;
	int OldState;

	CpuSuspendIntr(&OldState);
	result = AllocSysMemory(ALLOC_FIRST, size, NULL);
	CpuResumeIntr(OldState);

	return result;
}

void free(void *ptr){
	int OldState;

	CpuSuspendIntr(&OldState);
	FreeSysMemory(ptr);
	CpuResumeIntr(OldState);
}
#endif
