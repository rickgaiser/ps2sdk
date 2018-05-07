/* Copyright (c) 2007 Mega Man */
#include <types.h>
#include <thbase.h>
#include <thsemap.h>
#include <sysclib.h>
#include <stdio.h>
#include <ioman.h>
#include <intrman.h>
#include <loadcore.h>
#include <sifman.h>
#include <sifcmd.h>

#define MODNAME "eedebug"

IRX_ID(MODNAME, 1, 1);

/** Maximum buffer size. */
#define BUFFER_SIZE 1024

static void eePrint(const char *text);
static int ttyMount(void);
void eedebug_thread(void *unused);

static volatile char buffer[BUFFER_SIZE];
static volatile int bufferReadPos = 0;
static volatile int bufferWritePos = 0;
static int tty_sema = -1;

typedef struct {
	struct t_SifCmdHeader sifcmd;
	char text[80];
} iop_text_data_t;

/** IOP Module entry point, install io driver redirecting every printf() to EE. */
int _start(int argc, char **argv)
{
	iop_thread_t thread;
	int res;

	if (!sceSifCheckInit())
		sceSifInit();
	sceSifInitRpc(0);

	printf(MODNAME ": Started eedebug!\n");

	thread.attr = TH_C;
	thread.option = 0;
	thread.thread = eedebug_thread;
	thread.stacksize = 0x800;
	thread.priority = 40;

	if ((res = CreateThread(&thread)) < 0) {
		printf(MODNAME ": cannot create thread (%d).\n", res);
		eePrint(MODNAME ": cannot create thread\n");
		return -1;
	}

	StartThread(res, NULL);

	eePrint(MODNAME ": EE debug start\n");

	ttyMount();
	return 0;
}

static void eePrint(const char *text)
{
	static iop_text_data_t text_data __attribute__((aligned(64)));

	strncpy(text_data.text, text, 80);
	text_data.text[79] = 0;

	if (QueryIntrContext() == 0)
        sceSifSendCmd(0x8000000E, &text_data, sizeof(iop_text_data_t), NULL, NULL, 0);
    else
        isceSifSendCmd(0x8000000E, &text_data, sizeof(iop_text_data_t), NULL, NULL, 0);
}

static void eePrintBuffer()
{
	static iop_text_data_t text_data __attribute__((aligned(64)));

	while (bufferReadPos != bufferWritePos) {
		int i, id;

		/* Copy text data */
		i = 0;
		while(bufferReadPos != bufferWritePos) {
			text_data.text[i] = buffer[bufferReadPos];
			bufferReadPos = (bufferReadPos + 1) % BUFFER_SIZE;
			i++;
			if (i >= (80-1))
				break;
		}
		text_data.text[i] = 0;

		/* Start transfer to EE */
		while ((id = sceSifSendCmd(0x8000000E, &text_data, sizeof(text_data), NULL, NULL, 0)) == 0) {
			/* Try in one millisecond again. */
			DelayThread(1000);
		}

		/* Wait for transfer to finish */
		while (sceSifDmaStat(id) >= 0)
		{
			/* Try in one millisecond again. */
			DelayThread(1000);
		}
	}
}

////////////////////////////////////////////////////////////////////////

static char ttyname[] = "tty";

////////////////////////////////////////////////////////////////////////
static int dummy()
{
	return -5;
}

////////////////////////////////////////////////////////////////////////
static int dummy0()
{
	return 0;
}

////////////////////////////////////////////////////////////////////////
static int ttyInit(iop_device_t *driver)
{
	iop_sema_t sema_info;

	sema_info.attr    = 0;
	sema_info.initial = 0;	/* Locked.  */
	sema_info.max     = 1;
	if ((tty_sema = CreateSema(&sema_info)) < 0)
		return -1;

	return 1;
}

////////////////////////////////////////////////////////////////////////
static int ttyOpen( int fd, char *name, int mode)
{
	return 1;
}

////////////////////////////////////////////////////////////////////////
static int ttyClose( int fd)
{
	return 1;
}

////////////////////////////////////////////////////////////////////////
static int ttyWrite(iop_file_t *file, char *buf, int size)
{
#if 1
    eePrint(buf);
    return size;
#else
	int i;

	for (i = 0; i < size; i++) {
		int pos;

		pos = (bufferWritePos + 1) % BUFFER_SIZE;
		if (pos == bufferReadPos) {
			eePrint("\n\n" MODNAME ": Buffer full\n\n");
			eePrintBuffer();
            pos = (bufferWritePos + 1) % BUFFER_SIZE;
		}

		buffer[bufferWritePos] = buf[i];
		bufferWritePos = pos;
	}

	SignalSema(tty_sema);
	return i;
#endif
}

iop_device_ops_t tty_functarray = { ttyInit, dummy0, (void *)dummy,
	(void *)ttyOpen, (void *)ttyClose, (void *)dummy,
	(void *)ttyWrite, (void *)dummy, (void *)dummy,
	(void *)dummy, (void *)dummy, (void *)dummy,
	(void *)dummy, (void *)dummy, (void *)dummy,
	(void *)dummy, (void *)dummy };

iop_device_t tty_driver = { ttyname, 3, 1, "TTY via EE", &tty_functarray };

////////////////////////////////////////////////////////////////////////
// Entry point for mounting the file system
static int ttyMount(void)
{
	close(0);
	close(1);
	DelDrv(ttyname);
	AddDrv(&tty_driver);
	if(open("tty00:", O_RDONLY) != 0) while(1);
	if(open("tty00:", O_WRONLY) != 1) while(1);

	return 0;
}

void eedebug_thread(void *unused)
{
	eePrint(MODNAME ": eedebug_thread running.\n");
	while (1) {
		WaitSema(tty_sema);
		eePrintBuffer();
	}
}
