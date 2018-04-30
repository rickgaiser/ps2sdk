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
# SD Card over SIO2 driver by Wisi.
*/

#include "stdio.h"
#include "sysclib.h"
#include "thsemap.h"
#include "loadcore.h"

#include <thbase.h>
#include <timrman.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <ioman.h>
#include <sysmem.h>
#include <intrman.h>

//#include <sio2man.h>


#define BDM_EN 1


struct _sio2_dma_arg {
	void	*addr;
	int	size;
	int	count;
}; // = 0xC


typedef struct {
	u32	stat6c;

	u32	port_ctrl1[4];
	u32	port_ctrl2[4];

	u32	stat70;

	u32	regdata[16];

	u32	stat74;

	u32	in_size;
	u32	out_size;
	u8	*inp;
	u8	*out;

	struct _sio2_dma_arg in_dma;
	struct _sio2_dma_arg out_dma;
} sio2_transfer_data_t;

int sio2_transfer(sio2_transfer_data_t *td);
void sio2_pad_transfer_init();
void sio2_transfer_reset();



IRX_ID("sdCard", 1, 1);

#define WELCOME_STR "sdCard v1.1\n"

/*
extern struct irx_export_table _exp_sdCard;

int sdCardSema;

int sdCardLock(void) {
	return WaitSema(sdCardSema);
}

int sdCardUnlock(void) {
	return SignalSema(sdCardSema);
}
*/





#if 0

// PPC I/O functions

#include "../mips-iop_ppcComMain/emuModComIfIop.h"

extern u8 ppcSdCard_prx[];
extern int size_ppcSdCard_prx;
struct t_regPpcCpMod cpMiData = {0};
struct t_regPpcCpMod *cpMi = &cpMiData;

int cmnVl = 0;
u32 lastDataFromPpcToIopCmd = 0;
u32 prxCmdHndlr(u32 cmdCode, struct t_regPpcCpMod *mi, void *data, u32 flags) {
//	printf("\n iop -side handler called cmd %04X  mi %08X  data %08X flags %08X ", cmdCode, (u32)mi, (u32)data, flags);
cmnVl++;
lastDataFromPpcToIopCmd = (u32)data;
// *(vu32*)0xBF80145C = 0x8877FC12;


	return 0xB1FFC0DE;
}

/*
void * memcpy(void *dest, const void *src, size_t size) {
	CALL_CMD_PPC(0x30, 0, cpMi); return NULL;
}

	u32 args[1];
	args[0] = (u32)ep;
	sendCmdToPpc(0x11, args, CMD_FLAG_USE_DATA, cpMi);
*/

int ppcModInit(void) {
	cpMiData.ppcPrxBuffAddr = (void*)ppcSdCard_prx;
	cpMiData.ppcPrxBuffSize = size_ppcSdCard_prx;
	cpMiData.iopHandler = &prxCmdHndlr;
	//cpMiData.ppcHandler = NULL; //set by ppc-code
	cpMiData.sentArgAddr = cpMiData.ppcPrxBuffAddr; // Do not use this before the PRX is loaded, or it will get overwritten
	cpMiData.recvArgAddr = cpMiData.ppcPrxBuffAddr;
	//cpMiData.ppcPrxBuffAddr can be used as common buffer


	int ret = regPpcCpMod(&cpMiData);
	//printf("\n reg mi ret %08X ", ret);


//funcCallTest
/*
u32 rt;
//rt  = vaTstFunc(0x10BAC, 0x20BAC, 0x50BAC, 0xBAC30, 0xACB80, 0xBCC60);
//printf("\n ret %08X ", rt);
printf("\n %08X %08X ", (u32)&cpMiData, (u32)cpMi);
rt = vaTstFunc2(0x01020304, 0x01020314, 0x01020324, 0x01020334, 0x01020344, 0x01020354, 0x01020364, 0x01020374, 0x01020384);
printf("\n ret %08X ", rt);
*/
//while(1) {}

	u32 dataStct = 0xBAFF1234; //test with data on stack
	ret = sendCmdToPpc(7, &dataStct, CMD_FLAG_USE_DATA, &cpMiData);
	printf("\n sendCmd 7 ret %08X ", ret);

	dataStct = 0xC0DE1324; //test with data on stack
	*(u32*)cpMiData.sentArgAddr = 0xCAFFE342;
	ret = sendCmdToPpc(9, &dataStct, 0, &cpMiData);
	printf("\n sendCmd 9 ret %08X ", ret);


	dataStct = 0x1024DE; //test with command back to the IOP
	*(u32*)cpMiData.sentArgAddr = 0x12FFFE;
	ret = sendCmdToPpc(3, &dataStct, 0, &cpMiData);
	printf("\n sendCmd 3 ret %08X  and PPC->IOP dat %08X  trig: %08X ", ret, lastDataFromPpcToIopCmd, cmnVl);

	dataStct = 0x1024D1; //test with command back to the IOP
	*(u32*)cpMiData.sentArgAddr = 0x12FFF1;
	ret = sendCmdToPpc(3, &dataStct, CMD_FLAG_USE_DATA, &cpMiData);
	printf("\n sendCmd 3 ret %08X  and PPC->IOP dat %08X  trig: %08X ", ret, lastDataFromPpcToIopCmd, cmnVl);



	//ret = sendCmdToPpc(0x10, &memPool, CMD_FLAG_USE_DATA, &cpMiData);
//	printf("\n sendCmd 0x10 ret %08X ", ret);

//	ret = sendCmdToPpc(0x1B, &usbConfig, CMD_FLAG_USE_DATA, &cpMiData);


	return 0;
}
#endif











//SIO2 definitions and functions:


//Registers:

#define SIO2_REL_BASE		0x1F808000
#define SIO2_SEG				0xA0000000 //Uncached
#define SIO2_BASE 			(SIO2_REL_BASE + SIO2_SEG)

#define SIO2_REG_BASE		(SIO2_BASE + 0x200)
#define SIO2_REG_PORT0_CTRL1	(SIO2_BASE + 0x240)
#define SIO2_REG_PORT0_CTRL2	(SIO2_BASE + 0x244)
#define SIO2_REG_DATA_OUT	(SIO2_BASE + 0x260)
#define SIO2_REG_DATA_IN	(SIO2_BASE + 0x264)
#define SIO2_REG_CTRL		(SIO2_BASE + 0x268)
#define SIO2_REG_STAT6C		(SIO2_BASE + 0x26C)
#define SIO2_REG_STAT70		(SIO2_BASE + 0x270)
#define SIO2_REG_STAT74		(SIO2_BASE + 0x274)
#define SIO2_REG_UNKN78		(SIO2_BASE + 0x278)
#define SIO2_REG_UNKN7C		(SIO2_BASE + 0x27C)
#define SIO2_REG_STAT		(SIO2_BASE + 0x280)


#define SIO2_A_QUEUE		(SIO2_BASE + 0x200)
#define SIO2_A_PORT0_CTRL1	(SIO2_BASE + 0x240)
#define SIO2_A_PORT0_CTRL2	(SIO2_BASE + 0x244)
#define SIO2_A_DATA_OUT	(SIO2_BASE + 0x260)
#define SIO2_A_DATA_IN	(SIO2_BASE + 0x264)
#define SIO2_A_CTRL		(SIO2_BASE + 0x268)
#define SIO2_A_STAT6C		(SIO2_BASE + 0x26C)
#define SIO2_A_CONN_STAT70		(SIO2_BASE + 0x270)
#define SIO2_A_FIFO_STAT74		(SIO2_BASE + 0x274)
#define SIO2_A_TX_FIFO_PT		(SIO2_BASE + 0x278)
#define SIO2_A_RX_FIFO_PT		(SIO2_BASE + 0x27C)
#define SIO2_A_INTR_STAT		(SIO2_BASE + 0x280)


//Bit-field definitions:

#define PCTRL0_ATT_LOW_PER(x) (((x)<<0) & 0xFF)
#define PCTRL0_ATT_MIN_HIGH_PER(x) (((x)<<8) & 0xFF00)
#define PCTRL0_BAUD0_DIV(x) (((x)<<16) & 0xFF0000)
#define PCTRL0_BAUD1_DIV(x) (((x)<<24) & 0xFF000000)

#define PCTRL1_ACK_TIMEOUT_PER(x) (((x)<<0) & 0xFFFF)
#define PCTRL1_INTER_BYTE_PER(x) (((x)<<16) & 0xFF0000)
#define PCTRL1_UNK24(x) (((x)&1)<<24)
#define PCTRL1_IF_MODE_SPI_DIFF(x) (((x)&1)<<25)


#define TR_CTRL_PORT_NR(x) (((x)&0x3)<<0)
#define TR_CTRL_PAUSE(x) (((x)&1)<<2)
#define TR_CTRL_UNK03(x) (((x)&1)<<3)
//Each of the folowing is 0 for PIO transfer in the given direction. to select DMA, set to 1.
#define TR_CTRL_TX_MODE_PIO_DMA(x) (((x)&1)<<4)
#define TR_CTRL_RX_MODE_PIO_DMA(x) (((x)&1)<<5)
/*
normal	special
0			0				no transfer done(???)
1			0				normal transfer - usually used
0			1				"special" transfer (not known to have been ever used by anything, no known difference from normal).
1			1				no transfer takes place
*/
#define TR_CTRL_NORMAL_TR(x) (((x)&1)<<6)
#define TR_CTRL_SPECIAL_TR(x) (((x)&1)<<7)
//In bytes 0 - 0x100:
#define TR_CTRL_TX_DATA_SZ(x) (((x)&0x1FF)<<8)
#define TR_CTRL_UNK17(x) (((x)&1)<<17)
#define TR_CTRL_RX_DATA_SZ(x) (((x)&0x1FF)<<18)
#define TR_CTRL_UNK27(x) (((x)&1)<<27)
//28 and 29 can't be set
#define TR_CTRL_UNK28(x) (((x)&1)<<28)
#define TR_CTRL_UNK29(x) (((x)&1)<<29)
//selects between baud rate divisors 0 and 1
#define TR_CTRL_BAUD_DIV(x) (((x)&1)<<30)
#define TR_CTRL_WAIT_ACK_FOREVER(x) (((x)&1)<<31)



//8268 SIO2 (main) CTRL register.
#define CTRL_TR_START(x)			(((x)&1)<<0)
#define CTRL_TR_RESUME_PAUSED(x)		(((x)&1)<<1)
#define CTRL_RESET_STATE(x)			(((x)&1)<<2)
#define CTRL_RESET_FIFOS(x)			(((x)&1)<<3)
#define CTRL_USE_ACK_WAIT_TIMEOUT(x)	(((x)&1)<<4)
#define CTRL_NO_MISSING_ACK_ERR(x)	(((x)&1)<<5)
//bit6 unknonw r/o, 0
#define CTRL_UNK06(x)					(((x)&1)<<6)
//bit7 unknonw r/w, usually set to 1 by software
#define CTRL_UNK07(x)					(((x)&1)<<7)
#define CTRL_ERROR_INTR_EN(x)			(((x)&1)<<8)
#define CTRL_TR_COMP_INTR_EN(x)		(((x)&1)<<9)
//bits 10, 11 unknonw r/w, usually set to 0 by software
#define CTRL_UNK10(x)					(((x)&1)<<10)
#define CTRL_UNK11(x)					(((x)&1)<<11)
//Unknown r/o, 0:
#define CTRL_UNK2912(x)					(((x)&0x3FFFF)<<12)
#define CTRL_PS1_MODE_EN(x)			(((x)&1)<<30)
#define CTRL_SLAVE_MODE_EN(x)			(((x)&1)<<31)


//826C SIO2 (main) STAT register. (read-only)
#define STAT_UNK0300(x)					(((x)>>0)&0xF)
//Needs more data through DMA to complete transfer:
#define STAT_NEED_DMA_DATA_TX(x)		(((x)>>4)&1)
#define STAT_NEED_DMA_DATA_RX(x)		(((x)>>5)&1)
#define STAT_UNK0706(x)					(((x)>>6)&3)
//Queue slot number that is going to be processed next (0-15):
#define STAT_QUEUE_SLOT_PROC(x)		(((x)>>8)&0xF)
//Kept clear only while tranfer is running, and set while idle. 1 can also signify transfer completion:
#define STAT_TR_IDLE(x)					(((x)>>12)&1)
//FIFO underflow / overflow errors: (I am not sure if they really correspond to the TX and RX FIFOs)
#define STAT_ERR_TX_FIFO_UOFLOW(x)	(((x)>>13)&1)
#define STAT_ERR_RX_FIFO_UOFLOW(x)	(((x)>>14)&1)
#define STAT_ERR_ACK_MISSING(x)		(((x)>>15)&1)
//Flags used to determine the transfers of which queue slots triggered erroirs. Slot = 0 - 15:
#define STAT_ERR_QUEUE_SLOT(x, slot)	(((x)>>(slot+16))&1)


//8270 SIO2 Device connected detection status register. (read-only)
//For ports 0-3. Only SAS0 and SAS1 are actually connected to the physical port and only on early PS2 models (~ SCPH-30000).
#define CONN_STAT_CDC0(x)	(((x)>>0)&1)
#define CONN_STAT_CDC1(x)	(((x)>>1)&1)
#define CONN_STAT_CDC2(x)	(((x)>>2)&1)
#define CONN_STAT_CDC3(x)	(((x)>>3)&1)
#define CONN_STAT_SAS0(x)	(((x)>>4)&1)
#define CONN_STAT_SAS1(x)	(((x)>>5)&1)
#define CONN_STAT_SAS2(x)	(((x)>>6)&1)
#define CONN_STAT_SAS3(x)	(((x)>>7)&1)


//8274 SIO2 FIFO status register. (read-only)
//values 0 - 0x100 bytes:
#define FSTAT_TX_SZ(x)			(((x)>>0)&0x1FF)
#define FSTAT_TX_UNK09(x)		(((x)>>9)&1)
#define FSTAT_TX_OVERFLOW(x)	(((x)>>10)&1)
#define FSTAT_TX_UNDERFLOW(x)	(((x)>>11)&1)
#define FSTAT_UNK1512(x)		(((x)>>12)&0xF)
#define FSTAT_RX_SZ(x)			(((x)>>16)&0x1FF)
#define FSTAT_RX_UNK09(x)		(((x)>>25)&1)
#define FSTAT_RX_OVERFLOW(x)	(((x)>>26)&1)
#define FSTAT_RX_UNDERFLOW(x)	(((x)>>27)&1)
#define FSTAT_UNK3128(x)		(((x)>>28)&0xF)


//8278 SIO2_REG_UNKN78	TX FIFO pointers. (read/write)
//#define GET_TXFPNTS_HEAD(x)	(((x)>>0)&0xFF)
//#define SET_TXFPNTS_HEAD(x)	(((x)>>0)&0xFF)
//...
//Better not add the FIFO pointers, as to change only one of them would require more complex macros.
//827C SIO2_REG_UNKN7C	RX FIFO pointers.


//8280	SIO2_REG_STAT	u32 Interrupt status. Writing 1 to an interrupt flag clears it.
#define GET_ISTAT_TR_COMP(x)	(((x)>>0)&1)
//Any error:
#define GET_ISTAT_TR_ERR(x)	(((x)>>1)&1)


//Maybe all definitions should have "SIO2_" added before them, and some of their names should be changed.





//TODO: if the simple funcs here are to be used, make them inline, so that local functions would waste less time calling them!

/* 04 */ void sio2_ctrl_set(u32 val) { _sw(val, SIO2_REG_CTRL); }
/* 05 */ u32  sio2_ctrl_get() { return _lw(SIO2_REG_CTRL); }
/* 06 */ u32  sio2_stat6c_get() { return _lw(SIO2_REG_STAT6C); }
/* 07 */ void sio2_portN_ctrl1_set(int N, u32 val) { _sw(val, SIO2_REG_PORT0_CTRL1 + (N * 8)); }
/* 08 */ u32  sio2_portN_ctrl1_get(int N) { return _lw(SIO2_REG_PORT0_CTRL1 + (N * 8)); }
/* 09 */ void sio2_portN_ctrl2_set(int N, u32 val) { _sw(val, SIO2_REG_PORT0_CTRL2 + (N * 8)); }
/* 10 */ u32  sio2_portN_ctrl2_get(int N) { return _lw(SIO2_REG_PORT0_CTRL2 + (N * 8)); }
/* 11 */ u32  sio2_stat70_get() { return _lw(SIO2_REG_STAT70); }
/* 12 */ void sio2_regN_set(int N, u32 val) { _sw(val, SIO2_REG_BASE + (N * 4)); }
/* 13 */ u32  sio2_regN_get(int N) { return _lw(SIO2_REG_BASE + (N * 4)); }
/* 14 */ u32  sio2_stat74_get() { return _lw(SIO2_REG_STAT74); }
/* 15 */ void sio2_unkn78_set(u32 val) { _sw(val, SIO2_REG_UNKN78); }
/* 16 */ u32  sio2_unkn78_get() { return _lw(SIO2_REG_UNKN78); }
/* 17 */ void sio2_unkn7c_set(u32 val) { _sw(val, SIO2_REG_UNKN7C); }
/* 18 */ u32  sio2_unkn7c_get() { return _lw(SIO2_REG_UNKN7C); }
/* 19 */ void sio2_data_out(u8 val) { _sb(val, SIO2_REG_DATA_OUT); }
/* 20 */ u8   sio2_data_in() { return _lb(SIO2_REG_DATA_IN); }
/* 21 */ void sio2_stat_set(u32 val) { _sw(val, SIO2_REG_STAT); }
/* 22 */ u32  sio2_stat_get() { return _lw(SIO2_REG_STAT); }


inline void send_td(sio2_transfer_data_t *td)
{
	int i;

//sio2_portN_ctrl1_set(1, td->port_ctrl1[1]);
//sio2_portN_ctrl2_set(1, td->port_ctrl2[1]);

	for (i = 0; i < 4; i++) {
		sio2_portN_ctrl1_set(i, td->port_ctrl1[i]);
		sio2_portN_ctrl2_set(i, td->port_ctrl2[i]);
	}

	for (i = 0; i < 16; i++) {
		sio2_regN_set(i, td->regdata[i]);
		if (td->regdata[i] == 0) break; //make it faster
	}
//sio2_regN_set(0, td->regdata[0]);
//sio2_regN_set(1, 0);

//printf("\n sndSz %08X " , td->in_size);
	//if (td->in_size) {
	for (i = 0; i < td->in_size; i++)
		*(vu8*)0xBF808260 = td->inp[i];
		//sio2_data_out(td->inp[i]);
	//}

	if (td->in_dma.addr) {
		// *(vu32*)0xBF801548 = 0;
		*(vu32*)0xBF801540 = (u32)td->in_dma.addr & 0x00FFFFFF;
		*(vu32*)0xBF801544 = (td->in_dma.count <<16) | (td->in_dma.size &0xFFFF);
		*(vu32*)0xBF801548 = 0x01000201;
		*(vu32*)0xBF801548 |= 0x01000000;
		//dmac_request(IOP_DMAC_SIO2in, td->in_dma.addr, td->in_dma.size,
		//		td->in_dma.count, DMAC_FROM_MEM);
		//dmac_transfer(IOP_DMAC_SIO2in);
	}

	if (td->out_dma.addr) {
		//card -> IOP
		// *(vu32*)0xBF801558 = 0;
		*(vu32*)0xBF801550 = (u32)td->out_dma.addr & 0x00FFFFFF;
		*(vu32*)0xBF801554 = (td->out_dma.count <<16) | (td->out_dma.size &0xFFFF);
		*(vu32*)0xBF801558 = 0x40000200;
		*(vu32*)0xBF801558 |= 0x01000000;
		//dmac_request(IOP_DMAC_SIO2out, td->out_dma.addr, td->out_dma.size,
		//		td->out_dma.count, DMAC_TO_MEM);
		//dmac_transfer(IOP_DMAC_SIO2out);
	}

}

inline void recv_td(sio2_transfer_data_t *td)
{
	int i;

	td->stat6c = *(vu32*)0xBF80826C; //sio2_stat6c_get();
	td->stat70 = *(vu32*)0xBF808270; //sio2_stat70_get();
	td->stat74 = *(vu32*)0xBF808274; //sio2_stat74_get();
//printf("\n rcvdSz %08X " , td->out_size);
	//if (td->out_size) {
	for (i = 0; i < td->out_size; i++)
		td->out[i] = *(vu8*)0xBF808264; //sio2_data_in();
	//}
}



void Lsio2_transfer(sio2_transfer_data_t *td)
{

	*(vu32*)0xBF801074 &= ~(1<<17);
	// *(vu32*)0xBF801070 = ~(1<<17);

		//u32  oldCtrlVal = sio2_ctrl_get() & (~0x3);
	//	sio2_ctrl_set(0x0BC); //no intr
		//sio2_ctrl_set(0x3BC);
	*(vu32*)0xBF808268 = 0x0BC;
	//	sio2_ctrl_set(sio2_ctrl_get() | 0xc); //clear fifo & other stuff
	send_td(td);
	//sio2_ctrl_set(sio2_ctrl_get() | 1); //start queue exec
	*(vu32*)0xBF808268 |= 1;

#if 1
	//while (sio2_ctrl_get() & 1) {}//int t; for(t=0;t<0x4;t++) {}} //wait for it to clear
//wait for transf compl on intr

	//while ((*(vu32*)0xBF801070 & (1<<17)) == 0) {}
#endif
	//Wait for transfer to complete
	//int k=0;
	while ((*(vu32*)0xBF80826C & (1<<12)) == 0) { }// k++; if (k>0xFFF) { printf("\n sio2 busy Timeout "); return 0;/*break;*/} }


// *(vu32*)0xBF801070 = ~(1<<17);
		recv_td(td);


//printf("\n 8268  %08X  8268 %08X  8280 %08X ", *(vu32*)0xBF808268, *(vu32*)0xBF80826C, *(vu32*)0xBF808280);


	//sio2_ctrl_set(oldCtrlVal);
	*(vu32*)0xBF808268 = 0x3BC;
	*(vu32*)0xBF801070 = ~(1<<17);
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	*(vu32*)0xBF801074 |= (1<<17);

	return;
}












//Transfer rate measurment functions:

//TODO: switch from global variables to global structure or local structure.
u32 measureTransferRateSize = 0;
iop_sys_clock_t measureTransferRateClock;

void measureTransferRateStart(u32 transferSz) {
	measureTransferRateSize = transferSz;
	GetSystemTime(&measureTransferRateClock);
	return;
}

int measureTransferRateFinish(int doPrt) {
	u32 testRdSz = measureTransferRateSize;
	iop_sys_clock_t clock;
	u32 sec, usec, secOld, usecOld;

	SysClock2USec(&measureTransferRateClock, &secOld, &usecOld);
	GetSystemTime(&clock);
	SysClock2USec(&clock, &sec, &usec);

	sec -= secOld;
	if (usec < usecOld) { usec += 1000000; sec--; }
	usec -= usecOld;
	u32 trRt = (testRdSz * (1000000/1024)) / (usec + (sec * 1000000));
//	trRt /= 1024;
//74 -> 3378
	if (doPrt) printf("\n Tr: sz %08X took %d sec and %d usec  trRate %d kB/s ", testRdSz, sec, usec, trRt);

	return trRt;
}





















//Reversing the order of the bits in a byte:


static unsigned char rbLut[16] = {
0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf, };

inline u8 reverseByte(u8 n) {
   return (rbLut[n & 0xF] << 4) | rbLut[(n>>4) & 0xF];
}


//slower:
//#define reverseByteTest(b) reverseByte(b)

#define reverseByteTest(b) (BitReverseTable[b & 0xFF])

unsigned char BitReverseTable[256] =
{
0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};


//This might be faster than the 32-bit variant because the instructions for loading 16-bit values are 1-instr, while for 32-bit values 2 instructuons are used.
inline u16 reverseByteInHalf(u16 n) {
/*	n = ((n & 0xF0F0) >>4) | ((n & 0x0F0F) <<4);
	n = ((n & 0xCCCC) >>2) | ((n & 0x3333) <<2);
	n = ((n & 0xAAAA) >>1) | ((n & 0x5555) <<1);
*/
#if 1
//u32 vala, valb;
	//15 instr
	__asm__ volatile (
	".set\tnoreorder\n\t"		\
	"sll\t$a2, %1, 4\n\t"	\
	"srl\t$a3, %1, 4\n\t"	\
	"andi\t$a2, $a2, 0xF0F0\n\t" \
	"andi\t$a3, $a3, 0x0F0F\n\t"	\
	"or\t%1, $a2, $a3\n\t"	\
	\
	"sll\t$a2, %1, 2\n\t"	\
	"srl\t$a3, %1, 2\n\t"	\
	"andi\t$a2, $a2, 0xCCCC\n\t" \
	"andi\t$a3, $a3, 0x3333\n\t"	\
	"or\t%1, $a2, $a3\n\t"	\
	\
	"sll\t$a2, %1, 1\n\t"	\
	"srl\t$a3, %1, 1\n\t"	\
	"andi\t$a2, $a2, 0xAAAA\n\t" \
	"andi\t$a3, $a3, 0x5555\n\t"	\
	"or\t%0, $a2, $a3\n\t"	\
	: "=r" (n) : "r" (n) : "a2", "a3" );

#else
n = reverseByteTest(n) | (reverseByteTest(n >>8)<<8);
#endif

   return n;
}


inline u32 reverseByteInHalfNmsk(u32 n) {
/*	n = ((n & 0xF0F0) >>4) | ((n & 0x0F0F) <<4);
	n = ((n & 0xCCCC) >>2) | ((n & 0x3333) <<2);
	n = ((n & 0xAAAA) >>1) | ((n & 0x5555) <<1);
*/
#if 1
//u32 vala, valb;
	//15 instr
	__asm__ volatile (
	".set\tnoreorder\n\t"		\
	"sll\t$a2, %1, 4\n\t"	\
	"srl\t$a3, %1, 4\n\t"	\
	"andi\t$a2, $a2, 0xF0F0\n\t" \
	"andi\t$a3, $a3, 0x0F0F\n\t"	\
	"or\t%1, $a2, $a3\n\t"	\
	\
	"sll\t$a2, %1, 2\n\t"	\
	"srl\t$a3, %1, 2\n\t"	\
	"andi\t$a2, $a2, 0xCCCC\n\t" \
	"andi\t$a3, $a3, 0x3333\n\t"	\
	"or\t%1, $a2, $a3\n\t"	\
	\
	"sll\t$a2, %1, 1\n\t"	\
	"srl\t$a3, %1, 1\n\t"	\
	"andi\t$a2, $a2, 0xAAAA\n\t" \
	"andi\t$a3, $a3, 0x5555\n\t"	\
	"or\t%0, $a2, $a3\n\t"	\
	: "=r" (n) : "r" (n) : "a2", "a3" );
#else
n = reverseByteTest(n) | (reverseByteTest(n >>8)<<8);
#endif
   return n;
}

inline u32 reverseByteInWord(u32 n) {
	//By transforming the masks, the two lui/addiu instructions are substituted by a sll
/*	u32 maskF = 0x0F0F0F0F; //Compiler constant-optimization makes the shifts-optimization below not work.
	u32 mask3 = 0x33333333;
	u32 mask5 = 0x55555555;
	n = ((n & (maskF <<4)) >>4) | ((n & maskF) <<4);
	n = ((n & (mask3 <<2)) >>2) | ((n & mask3) <<2);
	n = ((n & (mask5 <<1)) >>1) | ((n & mask5) <<1);
*/
#if 1
	//24 instr.
	__asm__ volatile (
	".set\tnoreorder\n\t"		\
	"sll\t$a2, %1, 4\n\t"	\
	"srl\t$a3, %1, 4\n\t"	\
	"lui\t$a1, 0x0F0F\n\t" \
	"addiu\t$a1, $a1, 0x0F0F\n\t" \
	"and\t$a3, $a3, $a1\n\t"	\
	"sll\t$a1, $a1, 4\n\t"	\
	"and\t$a2, $a2, $a1\n\t" \
	"or\t%1, $a2, $a3\n\t"	\
	\
	"sll\t$a2, %1, 2\n\t"	\
	"srl\t$a3, %1, 2\n\t"	\
	"lui\t$a1, 0x3333\n\t" \
	"addiu\t$a1, $a1, 0x3333\n\t" \
	"and\t$a3, $a3, $a1\n\t"	\
	"sll\t$a1, $a1, 2\n\t"	\
	"and\t$a2, $a2, $a1\n\t" \
	"or\t%1, $a2, $a3\n\t"	\
	\
	"sll\t$a2, %1, 1\n\t"	\
	"srl\t$a3, %1, 1\n\t"	\
	"lui\t$a1, 0x5555\n\t" \
	"addiu\t$a1, $a1, 0x5555\n\t" \
	"and\t$a3, $a3, $a1\n\t"	\
	"sll\t$a1, $a1, 1\n\t"	\
	"and\t$a2, $a2, $a1\n\t" \
	"or\t%0, $a2, $a3\n\t"	\
	\
	: "=r" (n) : "r" (n) : "a1", "a2", "a3" );
#else
n = reverseByteTest(n) | (reverseByteTest(n >>8)<<8) | (reverseByteTest(n >>16)<<16) | (reverseByteTest(n >>24)<<24);
#endif
	//the variant with 24 instr is (almost) exactly as fast at the direct 256-byte LUT (1265kB/s -> 1259kB/s).
	//The indirect-LUT (16-byte) variant is much slower (1259kB/s -> 998kB/s).
   return n;
}


unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}








//Debugging functions:

void prtData(void *dat, int len) {
	int i;
	u8 *d = dat;
	printf("\n data: ");
	for (i=0; i<len;) {
		printf("%02X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X ", i, d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++]);
		int j; for (j=0; j<0xFFFFF; j++) {}
	}
	return;
}

void prtDataArr(void *dat, int len) {
	int i;
	u8 *d = dat;
	printf("\n data: ");
	for (i=0; i<len;) {
		printf("\n 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, ", d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++], d[i++]);
		int j; for (j=0; j<0x8FFFF; j++) {}
	}
	return;
}







//CRC calculation functions:

u8 calcCrc7(void *buf, int len) {
	u8 *bf = buf;
	u32 poly = 0;
	int i;

	for (i=0; i<len; i++) {
		u32 di = bf[i];
		u32 xpd = poly ^ (di>>1);
		u32 polyIn = xpd ^ ((xpd >>4) & 0x07); //if both poly and di are masked to 0x7F this masking won't be necessary
		poly = polyIn ^ (polyIn << 3);

		u32 bt7 = (((poly >>6) ^ di) & 1);
		poly  = (bt7 ^ (bt7<<3)) ^ ((poly<<1));// & 0xFE);
		//printf("\n di %02X  poly %08X ", di,  poly);
	}
	u8 crc = poly & 0xFF;
	return crc;
}



u16 calcCrc16(void *buf, int len) {
	u16 *bf = buf;
	u32 poly = 0, polyIn, di; //Initializing poly to 0 AND using it as initializer in the loop for polyIn results in the compiler forcing it always to 0 in the loop (so don't use variables as initializers - i.e. u32 polyIn = poly; ).
	int i;
	len /= 2; //to u16
	for(i=0; i<len; i++) {
		di = (((bf[i]) <<8) & 0xFF00) | (((bf[i]) >>8) & 0xFF);
		polyIn = poly;
		//PPC has an instruction for shifting and masking (continuous mask) at the same time.
		//This sequence must be folowed because some of the bits altered in the first operations are used in the folowing.
		polyIn ^= ((polyIn ^ di) >>4 ) & 0x0F00;
		polyIn ^= ((polyIn ^ di) >>4 ) & 0x00F0;
		polyIn ^= ((polyIn ^ di) >>11) & 0x001F;
		polyIn ^= ((polyIn ^ di) >>4 ) & 0x000F;
		polyIn ^= di;
		poly = polyIn ^ (polyIn <<5) ^ (polyIn <<12);
	}

	u16 crc = poly;
	return crc;
}


inline u16 addCrc16(u16 oldCrc, u16 val) {
	//20 isntr. looks farely optimal, so rewriting in ASM can't make it much better...
	//u16 *bf = buf;
	u32 poly = 0, polyIn, di; //Initializing poly to 0 AND using it as initializer in the loop for polyIn results in the compiler forcing it always to 0 in the loop (so don't use variables as initializers - i.e. u32 polyIn = poly; ).
	//int i;
	//len /= 2; //to u16
	//for(i=0; i<len; i++) {
		di = val; //(((bf[i]) <<8) & 0xFF00) | (((bf[i]) >>8) & 0xFF);
		polyIn = oldCrc; //poly;
		//PPC has an instruction for shifting and masking (continuous mask) at the same time.
		//This sequence must be folowed because some of the bits altered in the first operations are used in the folowing.
		polyIn ^= ((polyIn ^ di) >>4 ) & 0x0F00;
		polyIn ^= ((polyIn ^ di) >>4 ) & 0x00F0;
		polyIn ^= ((polyIn ^ di) >>11) & 0x001F;
		polyIn ^= ((polyIn ^ di) >>4 ) & 0x000F;
		polyIn ^= di;
		poly = polyIn ^ (polyIn <<5) ^ (polyIn <<12);
	//}

	u16 crc = poly;
	return crc;
}

inline u32 addNmskCrc16(u32 oldCrc, u32 val) { //not masked to 16 bits - saves instr.
	u32 poly = 0, polyIn, di;
	di = val;
	polyIn = oldCrc;
	polyIn ^= ((polyIn ^ di) >>4 ) & 0x0F00;
	polyIn ^= ((polyIn ^ di) >>4 ) & 0x00F0;
	polyIn ^= ((polyIn ^ di) >>11) & 0x001F;
	polyIn ^= ((polyIn ^ di) >>4 ) & 0x000F;
	polyIn ^= di;
	poly = polyIn ^ (polyIn <<5) ^ (polyIn <<12);
	u32 crc = poly;
	return crc;
}




//SD Card protocol definitions:

//Responses:

//R1 1byte
//bit-fields:
//#define R1_F_IDLE (1<<0)
#define R1_IDLE(x) (((x)>>0)&1)
#define R1_ERASE_RST(x) (((x)>>1)&1)
#define R1_ILLEGAL_CMD(x) (((x)>>2)&1)
#define R1_CRC_ERR(x) (((x)>>3)&1)
#define R1_ERR_ERASE_SEQ(x) (((x)>>4)&1)
#define R1_ADDR_ERR(x) (((x)>>5)&1)
#define R1_PARAM_ERR(x) (((x)>>6)&1)
#define R1_CONST0(x) (((x)>>7)&1)

//R1b - same as R1, but with any nimber of bytes afer it, signalling the bysy condition: 0=busy / 1 (0xFF) = ready for next cmd.


//R2 response to SEND_STATUS
#define R2_LOCKED(x) (((x)>>0)&1)
#define R2_WP_ERASE_SKIP_LOCK_UNLOCK_FAIL(x) (((x)>>1)&1)
#define R2_ERR(x) (((x)>>2)&1)
#define R2_CC_ERR(x) (((x)>>3)&1)
#define R2_ECC_FAIL(x) (((x)>>4)&1)
#define R2_WP_VIOLATION(x) (((x)>>5)&1)
#define R2_ERASE_PARAM(x) (((x)>>6)&1)
#define R2_OUT_OF_RANGE(x) (((x)>>7)&1)

#define R2_1_IDLE(x) (((x)>>0)&1)
#define R2_1_ERASE_RST(x) (((x)>>1)&1)
#define R2_1_ILLEGAL_CMD(x) (((x)>>2)&1)
#define R2_1_COM_CRC_ERR(x) (((x)>>3)&1)
#define R2_1_ERASE_ERR(x) (((x)>>4)&1)
#define R2_1_ADDR_ERR(x) (((x)>>5)&1)
#define R2_1_PARAM_ERR(x) (((x)>>6)&1)
#define R2_1_CONST0(x) (((x)>>7)&1)

////


//Data tokens
//Sent by the card just before the data
// 1 byte token, data[1-2048bytes] CRC[2bytes]

//Data response tokens
//unshifted - directly compare with masked reg
#define DATA_RESP_CHK(x) ((((x) & 0x11) == 1) ? 1 : 0)
#define DATA_RESP_STAT_MSK 0x1F
#define DATA_RESP_STAT_ACCEPT 0x5
#define DATA_RESP_STAT_CRC_REJECT 0xB
#define DATA_RESP_STAT_WR_ERR_REJECT 0xD

//Data tokens:
#define C171824_DATA_TOKEN	0xFE
#define C25_DATA_TOKEN 0xFC
#define STOP_DATA_TOKEN 0xFD

//Read error token
#define ERR_TOKEN_CHK(x) (((((x) & 0xE0) == 0) && (((x) & 0x1F) != 0)) ? 1 : 0)
#define ERR_ERROR (1<<0)
#define ERR_CC (1<<1)
#define ERR_ECC_FAIL (1<<2)
#define ERR_OUT_OF_RANGE (1<<3)
#define ERR_LOCKED (1<<4)




int prtReadDataToken(u8 token) {

	if (token == C171824_DATA_TOKEN) {
	//	printf("\n Read Data Token OK ");
		return 0; //OK
	}

	if (ERR_TOKEN_CHK(token) == 0) {
		printf("\n Invalid Err Token %02X ", token);
	}

		char EnoString[] = "";
		char Eerr[] = "Error";
		char *EerrS = Eerr;
		if ((token & ERR_ERROR) == 0) EerrS = EnoString;

		char Ecc[] = "CC err";
		char *EccS = Ecc;
		if ((token & ERR_CC) == 0) EccS = EnoString;

		char Eecc[] = "ECC failed";
		char *EeccS = Eecc;
		if ((token & ERR_ECC_FAIL) == 0) EeccS = EnoString;

		char Eour[] = "Out of range";
		char *EourS = Eour;
		if ((token & ERR_OUT_OF_RANGE) == 0) EourS = EnoString;

		char Elock[] = "Locked";
		char *ElockS = Elock;
		if ((token & ERR_LOCKED) == 0) ElockS = EnoString;

		printf("\n token %02X    %s  %s  %s  %s  %s  ", token,  EerrS, EccS, EeccS, EourS, ElockS);

	return -1;
}


int chkPrtWriteDataRespToken(u8 tokenIn) {
	u8 token = tokenIn;
	if (DATA_RESP_CHK(token) == 0) {
		printf("\n Invalid Write Data Response Token %02X ", token);
	}

	token &= DATA_RESP_STAT_MSK;

	if (token == DATA_RESP_STAT_ACCEPT) {
	//	printf("\n Read Data Token OK ");
		return 0; //OK
	}

		char EnoString[] = "";
		char Ecrc[] = "CRC Error";
		char *EcrcS = EnoString;
		char EwrErr[] = "Write Error";
		char *EwrErrS = EnoString;

		if (token == DATA_RESP_STAT_CRC_REJECT) EcrcS = Ecrc;
		else if (token != DATA_RESP_STAT_WR_ERR_REJECT) EwrErrS = EwrErr;
		else printf("\n Unrecognized value in Write Data Response Token %02X ", token);
		printf("\n token %02X    %s  %s  ", tokenIn,  EcrcS, EwrErrS);

	return -1;
}







//The some of the comments below are old and are not valid for the current state of the code:


	//WARNING: When using DMA, make sure that transfer sizes are multiple of 4 bytes, otherwise transfer can hang in incompleted state when DMA doesn't transfer enough or transfers too much data!
	//Send and receive DMA sizes can be different, but each should match the size set in the command-queue. This means that lengths not multiple of 4 bytes are invalid.



// 80ns base of period ^ = 80ns *3  - so maybe it is in cycles of the interface... - as 83ns is 12MHz = 48MHz/4
//83 * 3 = 249, but per = 450 => 200ms added.
//2 -> 370ns
//1 -> 370ns
//0 -> 370 ns

//on the fat ps2 tgis is +1 and only 0->2, while 1->1:
//baud = 6 = 8MHz
//3	4cy * 125 = 500
//2	3cy = 375ns = 3*125ns
//1	2cy * 125 = 500ns
//0	1 * 125 = 125n

//baud = 4 = 12MHz
//0	~120ns

//it sems like the inter-byte period and the synchronization of the suurounding bytes is not fixed to the bit-clock - for short inter-byte pauses, the clock for the consecutive bytes may be "reversed" - it isn't the "same" clock - so the divisor may be getting reset, when a ne byte is read/written from/top fifo.

//baud = 4, inter-byte = 0 => spd= 1004kb/s
//9cy/byte 12MHz => should be: 1272kb/s
//1179kB/s / 1190kB/s (on first run) the bare 0x100 byte tr. - acual measurement- the delay is because memory /RAS maybe.
//setting-up the transfer takes 19.6 us / 13.6(short one )
// 0x100 byte tr takes 230us



//Divisors:
//keep PCTRL1_INTER_BYTE_PER at 0xC and above when using faster clocks(0x6), because otherwise errors begin to appear.
//	0x06	8MHz 		maybe safe
// 0x05  9.6MHz	mostly safe, correct data (but not always?).
// 0x04  12MHz		clock is mostly correct and parts of the data are correct as well, incorrect data gets trsnferred. unusable.
// 0x03	16MHz		clock pulses seems to get skipped, incorrect data gets trsnferred. unusable.

//But in some cases baud div of 0x4 and inter-byte period of 0x00 stil result in correct data. Maximum speed reached in those cases 134kB/s with CRC and 138kB/s without CRC.
//baud div = 0x05 and inter-byte per = 0x2 => 129kB/s


//TODO: /ACK has to be kept constantly low, otherwise the transfer ends after the first byte.
//Slim PS2 doesn't hang if it is kept low on boot-up, but maybe the fat one did hang in early tests..? - wrong - they both hang.
//Could use a delay of some sort for it, from the last /ATT to generate /ACK. Connecting /ACK to /ATT doesn't work.




//u32 rdBuf[0x100];
//u32 wrBuf[0x100];
//u8 *rdb = (void*)rdBuf;
//u8 *wrb = (void*)wrBuf;

u32 SdDataInternBuf[0x200]; //0x400 bytes



//SD card I/O:



/*

/CS should be held low over the whole CMD - RESP - DATA(if any) transaction, which requres that they are done in the same transfer. This was found not to be always necessary for the cards tested with, so hopefully there aren't cards that really require this.
//The response can come anywhere within 0 to 8 bytes after the command (1-8 for SD cards), so the MSB should be polled for 0 to detect it.
//Basically transfers should always be rather long:
 - command 6 bytes
 - wait (max) 8 bytes
 - response 6 (max) bytes

There is also a requirement that the response-reading stops if there is an err flag in the first byte (R1)but maybe it would stil work fine if all are read.


*/


//TODO: the SD card command-sending functions shouldn't really read more than the response bytes. This doesn't cause problems in most cases, so it is left this way for now.

int fastDiv = 0;

int sendCmd(void *rdBufA, void *wrBufA, int rcvSz, int sndSz, int portNr) {

	u8 *rdbA = (void*)rdBufA;
	u8 *wrbA = (void*)wrBufA;
	int i;

	int fastDivVal = 2;//6;
	int interBytePer = 0;//2;

	int sndUseDma = 0;
	int rcvUseDma = 0;

#if 0
	printf("\n Sent: ");
	for (i=0; i<sndSz; i++) {
		printf(" %08X ", wrbA[i]);
	}
#endif
	for (i=0; i<sndSz; i++) wrbA[i] = reverseByte(wrbA[i]);

	sio2_transfer_data_t trDat;
	sio2_transfer_data_t *td = &trDat;

	//td->stat6c = 0;//only read


	td->port_ctrl1[0] = 0; //ports 0-3
	td->port_ctrl2[0] = 0;
	td->port_ctrl1[1] = 0; //ports 0-3
	td->port_ctrl2[1] = 0;
	td->port_ctrl1[2] = 0;
	td->port_ctrl2[2] = 0;
	td->port_ctrl1[3] = 0;
	td->port_ctrl2[3] = 0;

	td->port_ctrl1[portNr] = PCTRL0_ATT_LOW_PER(0x5) | PCTRL0_ATT_MIN_HIGH_PER(0x5) | PCTRL0_BAUD0_DIV(0xC0) | PCTRL0_BAUD1_DIV(fastDivVal);
	td->port_ctrl2[portNr] = PCTRL1_ACK_TIMEOUT_PER(0x12C) | PCTRL1_INTER_BYTE_PER(interBytePer) | PCTRL1_UNK24(0) | PCTRL1_IF_MODE_SPI_DIFF(0);
//0x78 //0x78 = 400kHz = 48MHz / 120(=0x78)
	//td->stat70 = 0;//only read

	td->regdata[0] = TR_CTRL_PORT_NR(portNr) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(sndUseDma) | TR_CTRL_RX_MODE_PIO_DMA(rcvUseDma) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(sndSz) |
		TR_CTRL_RX_DATA_SZ(rcvSz) | TR_CTRL_BAUD_DIV(fastDiv) | TR_CTRL_WAIT_ACK_FOREVER(0);

	//0-15 transfer queue
	td->regdata[1] = 0;

	//td->stat74 = 0;//only read

	if (sndUseDma == 0) td->in_size = sndSz; //PIO tr size TO SdCard
	else td->in_size = 0;

	if (rcvUseDma == 0) td->out_size = rcvSz; //PIO tr size FROM SdCard
	else td->out_size = 0;

	td->inp = (void*)wrBufA; //PIO trBuf 0x100 max
	td->out = (void*)rdBufA; //PIO trBuf

	if (sndUseDma) td->in_dma.addr = (void*)wrBufA;
	else td->in_dma.addr = NULL; //buff pt

	td->in_dma.size = sndSz>>2;
	td->in_dma.count = 1;//in words

	if (rcvUseDma) td->out_dma.addr = (void*)rdBufA;
	else td->out_dma.addr = NULL; //buff pt

	td->out_dma.size = rcvSz>>2;
	td->out_dma.count = 1; //in words


#if 0
#if 1
//	sio2_transfer_reset();
	sio2_pad_transfer_init();
	sio2_transfer(td);
	sio2_transfer_reset();

#else
	xsio2_transfer_reset();
	xsio2_pad_transfer_init();
	xsio2_transfer(td);
	xsio2_transfer_reset();
#endif
#else
	//Lsio2_pad_transfer_init();
	Lsio2_transfer(td);
	//Lsio2_transfer_reset();
#endif

for (i=0; i<rcvSz; i++) rdbA[i] = reverseByte(rdbA[i]);


	#if 0
		printf("\n Received: ");
		for (i=0; i<rcvSz; i++) {
			printf(" %08X ", rdbA[i]);
		}
	#endif
	//printf("\n TrQueue6C  %08X  lines_70  %08X   FIFO_state_74  %08X ", td->stat6c, td->stat70, td->stat74);

	return 0;
}



int sendDCmd(void *rdBufA, void *wrBufA, int rcvSz, int sndSz, int portNr, int rcvUseDma, int sndUseDma, int doRev) {

	u8 *rdbA = (void*)rdBufA;
	u8 *wrbA = (void*)wrBufA;
	int i;
	//	int sndUseDma = 0;
	//	int rcvUseDma = 1;

	//	if ((sndUseDma) && (sndSz < rcvSz)  //otherwise hangs at transfer.
	//		sndSz = rcvSz;

	#if 0
		printf("\n Sent: ");
		for (i=0; i<sndSz; i++) {
			printf(" %08X ", wrbA[i]);
		}
	#endif

	int fastDataBaud = 6;
	int interByteDelay = 0;//0x20;

	if (doRev) for (i=0; i<sndSz; i++) wrbA[i] = reverseByte(wrbA[i]);

	sio2_transfer_data_t trDat;
	sio2_transfer_data_t *td = &trDat;

	//td->stat6c = 0;//only read

	td->port_ctrl1[portNr] = PCTRL0_ATT_LOW_PER(0x5) | PCTRL0_ATT_MIN_HIGH_PER(0x5) | PCTRL0_BAUD0_DIV(fastDataBaud) | PCTRL0_BAUD1_DIV(fastDataBaud);

	td->port_ctrl2[portNr] = PCTRL1_ACK_TIMEOUT_PER(0x12C) | PCTRL1_INTER_BYTE_PER(interByteDelay) | PCTRL1_UNK24(0) | PCTRL1_IF_MODE_SPI_DIFF(0);

	td->regdata[0] = TR_CTRL_PORT_NR(portNr) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(sndUseDma) | TR_CTRL_RX_MODE_PIO_DMA(rcvUseDma) |  TR_CTRL_NORMAL_TR(0) | TR_CTRL_SPECIAL_TR(1) |
		TR_CTRL_TX_DATA_SZ(sndSz) |
		TR_CTRL_RX_DATA_SZ(rcvSz) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);

	td->regdata[1] = 0;

	//td->stat74 = 0;//only read

	if (sndUseDma == 0) td->in_size = sndSz; //PIO tr size TO SdCard
	else td->in_size = 0;

	if (rcvUseDma == 0) td->out_size = rcvSz; //PIO tr size FROM SdCard
	else td->out_size = 0;

	td->inp = (void*)wrBufA; //PIO trBuf 0x100 max
	td->out = (void*)rdBufA; //PIO trBuf

	if (sndUseDma) td->in_dma.addr = (void*)wrBufA;
	else td->in_dma.addr = NULL; //buff pt
	td->in_dma.size = sndSz>>2; //in words
	td->in_dma.count = 1; //in blocks

	if (rcvUseDma) td->out_dma.addr = (void*)rdBufA;
	else td->out_dma.addr = NULL; //buff pt

	td->out_dma.size = rcvSz>>2; //in words
	td->out_dma.count = 1; //in blocks

	Lsio2_transfer(td);

	if (doRev) for (i=0; i<rcvSz; i++) rdbA[i] = reverseByte(rdbA[i]);

	#if 0
		printf("\n Received: ");
		for (i=0; i<rcvSz; i++) {
			printf(" %08X ", rdbA[i]);
		}
	#endif
	//printf("\n TrQueue6C  %08X  lines_70  %08X   FIFO_state_74  %08X ", td->stat6c, td->stat70, td->stat74);

	return 0;
}





//with baudDiv = 1 => SCK=48MHz (no kidding) but period betwen bytes = ~182ns >> (much larger than) 20.8ns cycle-rate.
//SD cards doesn't work witis clock (even though it is well-formed).

//Using divisor = 0 causes the transfer to never complete - /CS (/ATT) keeps low, no interrupt, no clock, etc.
//It is possible that 48MHz can be used by some cards or maybe when connected directly - no resistors(because there are already resistors in the ps2) (tested with direct SCKonnect - no effect)- stil bad data -crc gets 0xffff, so card sees more clock-pulses most likely.
//same when tested with SD HC 4GB (4) SD4 card.

/*

Note that for high freqiuencies the period betwen bytes doesn't get reduced below a certain value, so the speed can increase too much).

These are raw speeds of the actual transfer (and parameter preparation). Additional SD-card processing isn't measured.

freq 		trSpd

48 MHz	1900 kB/s (transfered data is damaged)
24			1640 kB/s (fastes usable with SD cards in SPI mode)
16			1293 kB/s
12			1082 kB/s (fastes attainable on the controler port, but unsafe)
9.6		922 kB/s



24MHz - inter-byte = 198ns (178ns - whewn subtractinbg half a pulse)
16MHz - 168ns - from the same SCK transitions(as above 178) /(\)->/
12MHz - 168ns

this divisor is most likely loaded in a counter which counts up to it. This explains why it doesn't works wit a value of 0.

*/



//Data I/O:



int readSdData(int port, u8 *tokenOut, void *buf, u16 *crcOut) {
	int i, j;
	u8 *bo = (void*)buf;
	u8 *bf = (void*)SdDataInternBuf;



	//Clear buffer to default value:
	SdDataInternBuf[0] = 0xFFFFFFFF; //u32

	for (i=0; i<512; i++) bf[i] = 0xff; //0;

	bf[0] = 0xFF;

	i = 0;
	do {
		bf[0xFF] = 0xFF;
		sendDCmd(&bf[0xFF], &bf[0xFF], 0x1, 0x1, port, 0, 0, 1);
		i++;
		if (i> 0x200) {printf("\n ERR: Timeout on waiting for data. ");  break; }
	} while (bf[0xFF] == 0xFF);
//printf("\n on %08X ", i);

	//sendDCmd(bf, bf, 0x100, 0x4, port, 1, 0);

int l;
for (l=0; l<0x400; l++) bf[l] = reverseByte(bf[l]);


	iop_sys_clock_t clock;
	u32 secOld, usecOld, sec, usec;

printf("\n wait ");
int h; for (h=0; h<0x2FFFFF; h++) {}

for (h=0; h<0x8FFFFF; h++) {}
printf("\n waitEnd ");



		GetSystemTime(&clock);
		SysClock2USec(&clock, &secOld, &usecOld);

#if 0
	sendDCmd(&bf[0x100], &bf[0x100], 0x100, 0x4, port, 1, 0, 0);
//	sendDCmd(&bf[0x200], &bf[0x200], 0x3, 1, port);
	sendDCmd(&bf[0x200], &bf[0x200], 0x100, 0x4, port, 1, 0, 0);


	sendDCmd(&bf[0x300], &bf[0x300], 0x40, 0x4, port, 1, 0, 0); //CRC //reading a bit more to add more delay, otherwise the card isn't ready for the next access

#else


int fastDataBaud = 2;//6
int interByteDelay = 0;//0x20;//0xC;


	sio2_transfer_data_t trDat;
	sio2_transfer_data_t *td = &trDat;

int sndSz = 3; //0x100; //4*3; //each tranfers 4.. clould make it only 1.
//int rcvSz =0x100 + 0x200; // 0x100 +

	td->port_ctrl1[port] = PCTRL0_ATT_LOW_PER(0x5) | PCTRL0_ATT_MIN_HIGH_PER(0x5) | PCTRL0_BAUD0_DIV(fastDataBaud) | PCTRL0_BAUD1_DIV(fastDataBaud);

	td->port_ctrl2[port] = PCTRL1_ACK_TIMEOUT_PER(0x12C) | PCTRL1_INTER_BYTE_PER(interByteDelay) | PCTRL1_UNK24(0) | PCTRL1_IF_MODE_SPI_DIFF(0);

	td->regdata[0] = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(1) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(1) |
		TR_CTRL_RX_DATA_SZ(0x100) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);

	td->regdata[1] = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(1) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(1) |
		TR_CTRL_RX_DATA_SZ(0x100) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);

	td->regdata[2] = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(1) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(1) |
		TR_CTRL_RX_DATA_SZ(0x100) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);

	td->regdata[3] = 0;



	td->inp = (void*)&bf[0x100]; //PIO trBuf 0x100 max
	//td->out = (void*)bf; //PIO trBuf

	td->in_size = sndSz;
	td->out_size = 0;
	//td->inp = (void*)wrBufA; //PIO trBuf 0x100 max
	//td->out = (void*)rdBufA; //PIO trBuf

	td->in_dma.addr = NULL; //bf;
//	td->in_dma.size = sndSz>>2; //in words
//	td->in_dma.count = 1; //in blocks
	td->out_dma.addr = &bf[0x100];
	td->out_dma.size = 0x100 >>2; //rcvSz>>2; //in words
	td->out_dma.count = 3;//2;//3; //1; //in blocks

	Lsio2_transfer(td);

#endif

u32 testRdSz = 0x300;//0x200;//0x40;//100;//0x200;

		GetSystemTime(&clock);
		SysClock2USec(&clock, &sec, &usec);


printf("\n 8268  %08X  8268 %08X  8280 %08X ", *(vu32*)0xBF808268, *(vu32*)0xBF80826C, *(vu32*)0xBF808280);

		sec -= secOld;
		if (usec < usecOld) { usec += 1000000; sec--; }
		usec -= usecOld;
		u32 trRt = (testRdSz * 1000000) / (usec + (sec * 1000000));
		trRt /= 1024;
//74 -> 3378
		printf("\n >>> read sz %08X took %d sec and %d usec  trRate %d kB/s ", testRdSz, sec, usec, trRt);

/*

So do it like this:

- poll with single-byte reads for when the card is ready with the data.
- set-up a DMA transfer of 0x100-byte blocks, each corresponding to a transfer queue element, transfer the size of the tranferred data.
- After the actual data has been transferred (maybe to the buffer passed by the device) read the two byte CRC bvy PIO read.

- reverse the data bytes, calculate CRC at the same time and if that makes it faster - can do the transfer in PIO here, rather than through DMA. Can also copy from/to source buffer here.



TODO: check how long the actual DMA takes - most likely it only transfers to the buffer... maybe there is a second buffer for DMA?? - check what buffer contains and the memory above it.
To determine if it gets delayed for byte-transfers - cluld simply measiure the time around DMA with a fast and slow frequency transf.

*/


for (l=0; l<0x400; l++) bf[l] = reverseByte(bf[l]);


//With the card tested, reads can be done even with "disrupted" /CS.
//note: there might be a function to combine the /CS (/ATT) of consecutive commands in the queue, so this and dma could be used maybe to do long transfrs.
//a hack would be to fill the pio fifo less than the transfer size and use that to keep /CS active once the fifo runs-out (if it stays active).

//	for (i=0; bf[i] == 0xFF; i++) { if (i >= 0x100) { printf("\n ERR: Timeout on waiting for data. "); break; } }
//printf("\n on %08X ", i);


i = 0xFF;
	u8 token = bf[i];

	*tokenOut = token;
	i++;

i = 0x100;

	for (j=0; j<512; j++) bo[j] = bf[i+j]; //could also do CRC in the same loop...
	i += j;
	u16 crc = (bf[i]<<8) | (bf[i+1]<<0);
	*crcOut = crc;

	u16 localCrc = calcCrc16(bo, 512);
	//printf("\n token %02X ", token);
	prtReadDataToken(token);
	//printf("\n CRC %04X   local %04X ", crc, localCrc);
	if (crc != localCrc) printf("\n ERR: CRC mismatch! CRC %04X   local %04X ", crc, localCrc);

	//prtData(bo, 512);
	//prtData(&bf[i-(513+15)], 512+0x100);

	return 0;
}

















//better keep at 0 if it works.
#define USE_SIO2_INTR_POLLING 0


//it actually checks for idle-state:
inline int sio2PollTransferCompletion(void) {
	#if USE_SIO2_INTR_POLLING == 1
	if ((*(vu32*)0xBF801070 & (1<<17)) == 0) return 0; else return 1;
	#else
	if ((*(vu32*)0xBF80826C & (1<<12)) == 0) return 0; else return 1;
	//return (*(vu32*)0xBF80826C & (1<<12));
	#endif
}
#if 0
inline void sio2WaitTransferCompletion(void) {
	#if USE_SIO2_INTR_POLLING == 1
	while ((*(vu32*)0xBF801070 & (1<<17)) == 0) {}
	#else
	while ((*(vu32*)0xBF80826C & (1<<12)) == 0) {}
	//return (*(vu32*)0xBF80826C & (1<<12));
	#endif
	return;
}
#else

#define sio2WaitTransferCompletion() sio2WaitTransferCompletionWithTimeout()
#endif

//WARNING: if polled too often (without the timeout code in the loop), the waiting loop will take a very long time, because accessing the register seems to prevent/halt SIO2 trasnfer, so accessing it often prevents the transfer from runniung (at normal speed).
//So polling the interrupt status flag may actually be a better idea...
//same goes for accessing the FIFO state register

inline int sio2WaitTransferCompletionWithTimeout(void) {
	int cnt = 0;
	#if USE_SIO2_INTR_POLLING == 1
	while ((*(vu32*)0xBF801070 & (1<<17)) == 0) { cnt++; if (cnt>0xFFFF) return -1; }
	*(vu32*)0xBF801070 = ~(1<<17); //clear status
	#else
	while ((*(vu32*)0xBF80826C & (1<<12)) == 0) { cnt++; if (cnt>0xFFFF) return -1; }
	//return (*(vu32*)0xBF80826C & (1<<12));
//while ((*(vu32*)0xBF808274 & 0x1FF) != 0) {cnt++; if (cnt>0xFFFF) return -1; }
	#endif
	return 0;
}



int rdPollBytePrepare(int port) { //prepare a command, so that a byte can be polled faster
	//int i;
	int fastDataBaud = 2;//6; //This is not critical to be fast, so it will probably not influence overal speed if slow. But it does have some effect, so going with higher speed setting.
	int interByteDelay = 0;//2;
	*(vu32*)0xBF801074 &= ~(1<<17); //This is necessary if the pause bit it used, as it always causes transfer-completion interrupt regardless of the local interupt masks.
	#if USE_SIO2_INTR_POLLING == 1
	//Disable interrupts, so that set handler won't trigger.
	*(vu32*)0xBF801074 &= ~(1<<17);
	*(vu32*)0xBF801070 = ~(1<<17);
	//sio2_stat_set(sio2_stat_get()); //clear local intr (just in case)
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	sio2_ctrl_set(0x3BC); //clear old state
	#else
	//disable SIO2 interrupts internally in SIO2
	sio2_ctrl_set(0x0BC); //clear old state and disble interrupts (but the un-disableable pause-interrupt).
	#endif

	//sio2_transfer_data_t trDat;
	//sio2_transfer_data_t *td = &trDat;

	struct t_pCtrlRegs {
		u32 pCtrl0;
		u32 pCtrl1;
	};
	struct t_pCtrlRegs *pCtrl = (void*)0xBF808240;

	//td->port_ctrl1[port] =
	pCtrl[port].pCtrl0 = PCTRL0_ATT_LOW_PER(0x5) | PCTRL0_ATT_MIN_HIGH_PER(0x5) | PCTRL0_BAUD0_DIV(fastDataBaud) | PCTRL0_BAUD1_DIV(fastDataBaud);

	//td->port_ctrl2[port] =
	pCtrl[port].pCtrl1 = PCTRL1_ACK_TIMEOUT_PER(0x12C) | PCTRL1_INTER_BYTE_PER(interByteDelay) | PCTRL1_UNK24(0) | PCTRL1_IF_MODE_SPI_DIFF(0);

	// 1 byte read and 1 byte write
	//pause bit MUST be set on the last element, otherwise re-starting the transfer from the start won't work.
	//sio2_regN_set(0,
	vu32 *queueRegs = (void*)0xBF808200;

	queueRegs[0] = ( TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(1) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(0) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(1) |
		TR_CTRL_RX_DATA_SZ(1) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0) );

	queueRegs[1] = 0; //terminator
	//sio2_regN_set(1, 0);
	//td->regdata[1] = 0;

	//sio2_portN_ctrl1_set(port, td->port_ctrl1[port]);
	//sio2_portN_ctrl2_set(port, td->port_ctrl2[port]);

/*	for (i = 0; i < 4; i++) {
		sio2_portN_ctrl1_set(i, td->port_ctrl1[i]);
		sio2_portN_ctrl2_set(i, td->port_ctrl2[i]);
	} */

	//for (i = 0; i < 16; i++) {
	//	sio2_regN_set(i, td->regdata[i]);
	//	if (td->regdata[i] == 0) break; //make it faster
	//}
	//sio2_regN_set(0, td->regdata[0]);
	//sio2_regN_set(1, 0);

	/*if (td->in_size) {
		for (i = 0; i < td->in_size; i++)
			sio2_data_out(td->inp[i]);
	}*/

	//set-up regs and queue for a single-byte read, according to speed, port, etc.

	return 0;
}

//This won't work - if necessary - fix it:
/*int rdPollBytePoll(u8 fillByte) { //get a byte, with transfer queue already prepared above
	int ret = -1; //0xFFFFFFFF
	//fill a single byte in TX FIFO
	//trigger transfer - don't clear queue, but only FIFO - re-execute last queue.
	//wait completion
	//read byte from FIFO and return it
	sio2_data_out(fillByte);

	sio2_ctrl_set(sio2_ctrl_get() | 1); //start queue exec

	//while (sio2_ctrl_get() & 1) {}//int t; for(t=0;t<0x4;t++) {}} //wait for it to clear
	//wait for transf compl on intr
	//while ((*(vu32*)0xBF801070 & (1<<17)) == 0) {}
	u32 intrState = *(vu32*)0xBF801070;
	if ((intrState & (1<<17)) != 0) { //intr triggered:
		ret = sio2_data_in(); //get returned byte
		*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
		sio2_stat_set(sio2_stat_get()); //clear local intr
	}
	return ret;
} */

inline int rdPollByteWait(u8 fillByte, int port) { //get a byte, with transfer queue already prepared above
	int ret = -1; //0xFFFFFFFF
	//fill a single byte in TX FIFO
	//trigger transfer - don't clear queue, but only FIFO - re-execute last queue.
	//wait completion
	//read byte from FIFO and return it

	//printf("\n 8268 %08X  826C %08X  8274 %08X  8278 %08X  827C %08X  8280 %08X ", *(vu32*)0xBF808268, *(vu32*)0xBF80826C, *(vu32*)0xBF808274, *(vu32*)0xBF808278, *(vu32*)0xBF80827C, *(vu32*)0xBF808280 );

#if 0
	//a way to use non-pause-re-started transfer, so that intr doesn't trigger
	sio2_ctrl_set(0x0BC); //clear and disable intr
	sio2_regN_set(0, TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(0) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(1) |
		TR_CTRL_RX_DATA_SZ(1) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0) );

	sio2_regN_set(1, 0);
#endif

	//sio2_data_out(fillByte); //dummy byte to send
	*(vu8*)0xBF808260 = fillByte; //dummy byte to send
	//sio2_ctrl_set(sio2_ctrl_get() | 1); //(re-)start queue exec
	*(vu32*)0xBF808268 |= 1; //(re-)start queue exec

	//Removing the timeout loop below saves ~ 1kB/s

	//	while (sio2_ctrl_get() & 1) {}//int t; for(t=0;t<0x4;t++) {}} //wait for tr to complete - using this bit is unsafe (sometimes clears early).
	//wait for transf compl on intr
	int k = 0;
	#if USE_SIO2_INTR_POLLING == 1
	//Using interrupt should be safest, but requires disabling the SIO2 interrupt.
	while ((*(vu32*)0xBF801070 & (1<<17)) == 0) {k++; if (k>0xFFF) { printf("\n sio2 busy Timeout "); return 0;/*break;*/} }
	*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	#else
//0=busy
	//Using this idle/busy status bit should be much safer than using the tarnfer-start bit.
	while ((*(vu32*)0xBF80826C & (1<<12)) == 0) { k++; if (k>0xFFF) { printf("\n sio2 busy Timeout "); return 0;/*break;*/} }
	#endif
	ret = *(vu8*)0xBF808264; //sio2_data_in(); //get returned byte
	//printf("\n %08X %08X %08X %08X ", *(vu32*)0xBF801070,*(vu32*)0xBF80826C, *(vu32*)0xBF808280, sio2_ctrl_get() );
	return ret;
}


int rdPollByteComplete(int port) {

	//printf("\n %08X %08X %08X %08X ", *(vu32*)0xBF801070,*(vu32*)0xBF80826C, *(vu32*)0xBF808280, sio2_ctrl_get() );
	sio2_ctrl_set(0x3BC);//oldCtrlVal);
	#if USE_SIO2_INTR_POLLING == 1
	*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	*(vu32*)0xBF801074 |= (1<<17); //Re-enable intr
	#else
	//this is always be necessary because of the pasue bit always trigerring intr.
	*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	*(vu32*)0xBF801074 |= (1<<17); //Re-enable intr
	#endif
	return 0;
}





//u32 fifoStAr[0x40];



//The main limitation to reaching the actual potential of 24MHz is the pause between bits of length ~170ns (mostly equal for both 24 and 48MHz modes). Because of this pause, the 24MHz mode actually has the speed of 16MHz (= (((1/24) *8) +170ns) / 8)) and for 48Mhz - 18.7MHz - so it isn't much faster than the 24MHz(=>16MHz) mode.







/*
DMA read speeds:
4GB card:

0x200 bytes                    379KB/s      	  460kB/s
0x800                          695 	 			  931
0x1000 bytes (8 sectors): CRC: 746kB/s  noCRC: 1027kB/s

for comparison PIO speeds at 0x1000 are: 1100 / 1350

for DMA the byte-transfer period (inter-byte period included) is still 500 - 520ns, so the increased inter-byte delay is not due to SIO2 registers access in the PIO-variant of the tarsnfer function.

*/


//This DMA-read variant of the data transfer function is slower than the PIO-variant, so it isn't used.
//TODO: fix DMA that is >0x100 bytes and not 0x100-byte dividable
int readSdDMAUData(int port, u8 *tokenOut, void *buf, u16 *crcOut, int dataLen) { //The buffer MUST be 4-byte-aligned!
	int i;//, j;
	u8 *bf = (void*)buf;
//	printf("\n bf %08X ",(u32)buf );
	int localDataLen, dmaBlkCnt, dmaBlkSz=0;
	int dataToken;
	u16 cardCrc, localCrc = 0; //initial value

	int doCrc = (*crcOut == 0); //do only if variable pointed to == 0
	//do always if crcOut == NULL; //unimplemented

//measureTransferRateStart(0x200);

	//This stage can take some time, so no need to over-optimize it
	//Do something useful here, as this stage - while the SD Card is fetching the data can take some time. Make sure SIO2 doesn't get accessed by other functions, though.
	//This is mainly period-dependant and not (?) dependant on the number of clock-pulses fed.

	i = 0;
	rdPollBytePrepare(port);
	do {
		dataToken = rdPollByteWait(0xFF, port);
		i++;
		if (i> 0x800) {printf("\n ERR: Timeout on waiting for data (token). ");  break; }
		//int h; for( h=0; h<0xFFFFF; h++) {} //test whether the number of clock pulses matters or only the time duration matters.
	} while (dataToken == 0xFF);
	rdPollByteComplete(port);
	//printf("\n Data Token wait loop iterations  %08X ", i);
	dataToken = reverseByte(dataToken);

	int fastDataBaud = 2;//10; //With baud div = 1, the SCK pulses are 12 rather than 8!
	int interByteDelay = 0;//2;


	#if USE_SIO2_INTR_POLLING == 1
	*(vu32*)0xBF801074 &= ~(1<<17);
	*(vu32*)0xBF801070 = ~(1<<17);
	sio2_ctrl_set(0x3BC); //clear old state
	#else
	*(vu32*)0xBF801074 &= ~(1<<17); //stupid pause-bit always triggerring interrupts
	sio2_ctrl_set(0x0BC); //clear old state and disable interrupts
	#endif


	struct t_pCtrlRegs {
		u32 pCtrl0;
		u32 pCtrl1;
	};
	struct t_pCtrlRegs *pCtrl = (void*)0xBF808240;

	pCtrl[port].pCtrl0 = PCTRL0_ATT_LOW_PER(0x5) | PCTRL0_ATT_MIN_HIGH_PER(0x5) | PCTRL0_BAUD0_DIV(fastDataBaud) | PCTRL0_BAUD1_DIV(fastDataBaud);
	pCtrl[port].pCtrl1 = PCTRL1_ACK_TIMEOUT_PER(0x12C) | PCTRL1_INTER_BYTE_PER(interByteDelay) | PCTRL1_UNK24(0) | PCTRL1_IF_MODE_SPI_DIFF(0);

	u32 sio2BlkTrSetting = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(1) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(0) |
		TR_CTRL_RX_DATA_SZ(0x100) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);



	vu32 *queueRegs = (void*)0xBF808200;
	localDataLen = 0;
	for (i=0; i<14; i++) {
		// *(vu32*)(0xBF808260) = 0xFFFFFFFF; //output TX data - 4 bytes - 1 byte per queue element and two more, to save time from another access for writing the byte over getting CRC.
		localDataLen += 0x100;
		if (localDataLen >= dataLen) {
			localDataLen -= 0x100;
			int rem = dataLen - localDataLen;
			dmaBlkSz = rem >>2; // <= 0x100 bytes
			queueRegs[i] = sio2BlkTrSetting;// | TR_CTRL_RX_DATA_SZ(rem); //
			localDataLen = dataLen;
			i++; //next entry
			break;
		}
		queueRegs[i] = sio2BlkTrSetting;// | TR_CTRL_RX_DATA_SZ(0x100);
	}
//	queueRegs[i-1] |= TR_CTRL_PAUSE(1);
	dmaBlkCnt = i;

	//The loop above should exit with i = element after the last of the block
	//set-up CRC transfer. In practice this can be done as a separate transfer.

	queueRegs[i++] = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(0) | TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(0) |
		TR_CTRL_RX_DATA_SZ(0x2) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);

	queueRegs[i-1] = 0; //terminator
	//queueRegs[i-1] = 0; //terminator


	//u16 *rdhBuf = (void*)bf;
	u32 *rdwBuf = (void*)bf; //make sure it is 4-byte aligned
	//printf("\n %08X %08X %08X %08X ", i, localDataLen, dataToken, doCrc);

//printf("\n %08X %08X ", dmaBlkCnt, dmaBlkSz);
	//32-bit variant:

/*
	if (td->in_dma.addr) {
		// *(vu32*)0xBF801548 = 0;
		*(vu32*)0xBF801540 = (u32)td->in_dma.addr & 0x00FFFFFF;
		*(vu32*)0xBF801544 = (td->in_dma.count <<16) | (td->in_dma.size &0xFFFF);
		*(vu32*)0xBF801548 = 0x01000201;
		*(vu32*)0xBF801548 |= 0x01000000;
		//dmac_request(IOP_DMAC_SIO2in, td->in_dma.addr, td->in_dma.size,
		//		td->in_dma.count, DMAC_FROM_MEM);
		//dmac_transfer(IOP_DMAC_SIO2in);
	}
*/
//int f;
// for (f=0;f<0xFFFFF; f++) {}



	//card -> IOP
	// *(vu32*)0xBF801558 = 0;
	*(vu32*)0xBF801550 = (u32)buf & 0x00FFFFFF;
	*(vu32*)0xBF801554 = (dmaBlkCnt <<16) | (dmaBlkSz &0xFFFF);
//printf("\n BCR %08X ", 	*(vu32*)0xBF801554);
	*(vu32*)0xBF801558 = 0x40000200;
//for (f=0;f<0xFFF; f++) {}
	*(vu32*)0xBF801558 |= 0x01000000; //start transfer
// for (f=0;f<0xFFF; f++) {}
	*(vu32*)0xBF808268 |= 1; //start queue exec
//for (f=0;f<0xFFF; f++) {}
	//Tests showed that polling the SIO2 registers can seriously delay SIO2 hardware processing, so polling the DMA CHCR register instead (first).
	while (*(vu32*)0xBF801558 & 0x01000000) {} //wait for transfer to complete... although the DMAC would be free for much of the time, so the IOP should have some "free" time now to do whatever it wants. Could release execution to the other threas untill DMA completes.
	if (sio2WaitTransferCompletionWithTimeout() < 0) {
		printf("\n ERR: Read transfer completion timed-out. ");
	}
//for (f=0;f<0xFFF; f++) {}
 //for (h=0; h<0x1FFFFFF; h++) {}
		//	printf("\n FIFO State %08X  %08X  %08X ", *(vu32*)0xBF808274, *(vu32*)0xBF80826C, *(vu32*)0xBF808268);

	*(vu32*)0xBF808268 = 0x0B8;
//for (f=0;f<0xFFF; f++) {}
	queueRegs[0] = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(0) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(0) |
		TR_CTRL_RX_DATA_SZ(0x2) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);
	queueRegs[1] = 0;
//for (f=0;f<0xFFF; f++) {}
//	*(vu32*)0xBF808260 = 0xFFFFFFFF;
//for (f=0;f<0xFFF; f++) {}
	*(vu32*)0xBF808268 |= 1; // get CRC
	// *(vu32*)0xBF808268 |= 2; ///continue paused - get CRC
	while (!((*(vu32*)0xBF808274) >= 0x20000)) {}//int a; for (a=0; a<0x2;a++){}} //wait for the CRC bytes
	cardCrc = *(vu16*)0xBF808264;
	cardCrc = (cardCrc>>8) | (cardCrc<<8);
	cardCrc = reverseByteInHalf(cardCrc);

	if (sio2WaitTransferCompletionWithTimeout() < 0) {
		printf("\n ERR: CRC Read transfer completion timed-out. ");
	}

//for (f=0;f<0xFFFFF; f++) {}

#if 1
u32 val;
	if (doCrc) {

		int trLenWords = localDataLen >>2; //convert to words
//printf("\n len %08X %08X ", trLenWords, (u32)rdwBuf );
		for (i=0; i<trLenWords; i++) {
			u32 *wPt = &rdwBuf[i];
			val = *wPt;
			val = reverseByteInWord(val); //reverse bits in each byte
			*wPt = val;
//printf("\n len %08X %08X ", trLenWords, (u32)wPt );
			#if 1
			u8 *rdValPt = (void*)wPt;
			localCrc = addCrc16(localCrc, (rdValPt[1]) | (rdValPt[0] <<8)); //calculate CRC
			localCrc = addCrc16(localCrc, (rdValPt[3]) | (rdValPt[2] <<8)); 	//addNmskCrc - no increase in speed.
			#endif
		}

	} else {

		int trLenWords = localDataLen >>2; //convert to words
//printf("\n len %08X ", trLenWords);
		for (i=0; i<trLenWords; i++) {
			u32 *wPt = &rdwBuf[i];
			val = *wPt;
//printf("\n len %08X %08X ", trLenWords, (u32)wPt );
			val = reverseByteInWord(val); //reverse bits in each byte
			*wPt = val;
			#if 0
			u8 *rdValPt = (void*)wPt;
			localCrc = addCrc16(localCrc, (rdValPt[1]) | (rdValPt[0] <<8)); //calculate CRC
			localCrc = addCrc16(localCrc, (rdValPt[3]) | (rdValPt[2] <<8)); 	//addNmskCrc - no increase in speed.
			#endif
		}

	}
#endif


	sio2_ctrl_set(0x3BC);//oldCtrlVal);
	#if USE_SIO2_INTR_POLLING == 1
	*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	*(vu32*)0xBF801074 |= (1<<17); //Re-enable intr
	#else
	//pause-bit - what can one do...
	*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	*(vu32*)0xBF801074 |= (1<<17); //Re-enable intr
	#endif

	*tokenOut = dataToken;
	//printf("\n token %02X ", dataToken);
	prtReadDataToken(dataToken);

	*crcOut = cardCrc;
	u16 crcChk = 0; //calcCrc16(bf, 0x200);
	if (doCrc) {
		if (cardCrc != localCrc) printf("\n ERR: CRC mismatch! CRC %04X   local %04X  %04X ", cardCrc, localCrc, crcChk);
	}
	//prtData(bf, 512);

	return 0;
}


























//TODO: the ususal command sender transfers a fixed ammount of data that is more than the command+response.
//This was done so that the response could be found in this data and separated, but it may create problems for data transfer commands or CMD 6 (with data-read after it). So it may be necessary to either limit the read to only the response bytes or make it faster, so that the card doesn't respond too early with data and the start of the data getting "eaten" by the command response read.




//Universal command to read data:  PIO read - access pio port while doing CRC, bit-rev, etc.
//It can be used for data reads and reading data after CMD 6 for example.
//CRC is calculated if the value in crcOut is 0, otherwise it isn't checked nor calculated.
//dataLen must be less than 0xE00 bytes in the current implementation
//dataLen must be multiple of 4 (CRC and data token excluded)
int readSdUData(int port, u8 *tokenOut, void *buf, u16 *crcOut, int dataLen) { //The buffer MUST be 4-byte-aligned!
	int i, j;
	u8 *bf = (void*)buf;

	int localDataLen;
	int dataToken;
	u16 cardCrc, localCrc = 0; //initial value

	int doCrc = (*crcOut == 0); //do only if variable pointed to == 0
	//do always if crcOut == NULL; //unimplemented

//measureTransferRateStart(0x200);

	//This stage can take some time, so no need to over-optimize it
	//Do something useful here, as this stage - while the SD Card is fetching the data can take some time. Make sure SIO2 doesn't get accessed by other functions, though.
	//This is mainly period-dependant and not (?) dependant on the number of clock-pulses fed.

	i = 0;
	rdPollBytePrepare(port);
	do {
		dataToken = rdPollByteWait(0xFF, port);
		i++;
		if (i> 0x800) {printf("\n ERR: Timeout on waiting for data (token). ");  break; }
		//int h; for( h=0; h<0xFFFFF; h++) {} //test whether the number of clock pulses matters or only the time duration matters.
	} while (dataToken == 0xFF);
	rdPollByteComplete(port);
	//printf("\n Data Token wait loop iterations  %08X ", i);
	dataToken = reverseByte(dataToken);

	int fastDataBaud = 2;
	int interByteDelay = 0;

//TODO: remove this test-code:
if (dataLen & 0x3) fastDataBaud = dataLen & 0x3;
dataLen &= ~0x3;

	//sio2_transfer_data_t trDat;
	//sio2_transfer_data_t *td = &trDat;

	#if USE_SIO2_INTR_POLLING == 1
	*(vu32*)0xBF801074 &= ~(1<<17);
	*(vu32*)0xBF801070 = ~(1<<17);
	sio2_ctrl_set(0x3BC); //clear old state
	#else
	*(vu32*)0xBF801074 &= ~(1<<17); //stupid pause-bit always triggerring interrupts
	sio2_ctrl_set(0x0BC); //clear old state and disable interrupts
	#endif


	struct t_pCtrlRegs {
		u32 pCtrl0;
		u32 pCtrl1;
	};
	struct t_pCtrlRegs *pCtrl = (void*)0xBF808240;

	pCtrl[port].pCtrl0 = PCTRL0_ATT_LOW_PER(0x5) | PCTRL0_ATT_MIN_HIGH_PER(0x5) | PCTRL0_BAUD0_DIV(fastDataBaud) | PCTRL0_BAUD1_DIV(fastDataBaud);
	pCtrl[port].pCtrl1 = PCTRL1_ACK_TIMEOUT_PER(0x12C) | PCTRL1_INTER_BYTE_PER(interByteDelay) | PCTRL1_UNK24(0) | PCTRL1_IF_MODE_SPI_DIFF(0);

	u32 sio2BlkTrSetting = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(1) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(0) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(1) |
		TR_CTRL_RX_DATA_SZ(0) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);

	vu32 *queueRegs = (void*)0xBF808200;
	localDataLen = 0;
	for (i=0; i<14; i++) {
		*(vu32*)(0xBF808260) = 0xFFFFFFFF; //output TX data - 4 bytes - 1 byte per queue element and two more, to save time from another access for writing the byte over getting CRC.
		localDataLen += 0x100;
		if (localDataLen >= dataLen) {
			localDataLen -= 0x100;
			queueRegs[i] = sio2BlkTrSetting | TR_CTRL_RX_DATA_SZ(dataLen - localDataLen);
			localDataLen = dataLen;
			i++; //next entry
			break;
		}
		queueRegs[i] = sio2BlkTrSetting | TR_CTRL_RX_DATA_SZ(0x100);
	}

	//The loop above should exit with i = element after the last of the block
	//set-up CRC transfer. In practice this can be done as a separate transfer.
	//td->regdata[i++] =
	queueRegs[i++] = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(0) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(1) |
		TR_CTRL_RX_DATA_SZ(0x2) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);

	//td->regdata[i] =
	queueRegs[i] = 0; //terminator

	*(vu32*)0xBF808268 |= 1; //start queue exec
	//sio2_ctrl_set(sio2_ctrl_get() | 1); //start queue exec

	//u16 *rdhBuf = (void*)bf;
	u32 *rdwBuf = (void*)bf; //make sure it is 4-byte aligned
	//printf("\n %08X %08X %08X %08X ", i, localDataLen, dataToken, doCrc);

	#if 1
	//32-bit variant:

	if (doCrc) {

		int trLenWords = localDataLen >>2; //convert to words

		for (i=0; i<trLenWords;) {
			j = 0x40;
			if (j > (trLenWords - i)) j = (trLenWords - i);
			//The maximum block-size of the SIO2 is 0x100 bytes = 0x40 words
			//do {
			while (j>0) {
				while (!((*(vu32*)0xBF808274) >= 0x40000)) {} //wait for at least 4 bytes of data

				u32 val = *(vu32*)0xBF808264; //get them

				val = reverseByteInWord(val); //reverse bits in each byte
				u32 *wPt = &rdwBuf[i];
				*wPt = val;
			#if 1
				u8 *rdValPt = (void*)wPt;
				localCrc = addCrc16(localCrc, (rdValPt[1]) | (rdValPt[0] <<8)); //calculate CRC
				localCrc = addCrc16(localCrc, (rdValPt[3]) | (rdValPt[2] <<8)); 	//addNmskCrc - no increase in speed.
			#endif
				i++;
				j--;
			} //while ((i & 0x3F) != 0x00);

			//resume next (paused) transfer:
			#if USE_SIO2_INTR_POLLING == 1
			//intr-clearing isn't that necesary in this case...
			*(vu32*)0xBF801070 = ~(1<<17);
			//sio2_stat_set(sio2_stat_get()); //clear local intr
			*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
			#endif
			//sio2_ctrl_set(sio2_ctrl_get() | 0x8); //clear fifos
			// sio2_data_out(0xFF); sio2_data_out(0xFF);
			*(vu32*)0xBF808268 |= 2; ///continue paused
			//sio2_ctrl_set(sio2_ctrl_get() | 0x2); //continue paused
		}

	} else {

		int trLenWords = localDataLen >>2; //convert to words

		for (i=0; i<trLenWords;) {
			j = 0x40;
			if (j > (trLenWords - i)) j = (trLenWords - i);
			//The maximum block-size of the SIO2 is 0x100 bytes = 0x40 words
			//do {
			while (j>0) {
				while (!((*(vu32*)0xBF808274) >= 0x40000)) {} //wait for at least 4 bytes of data

				u32 val = *(vu32*)0xBF808264; //get them

				val = reverseByteInWord(val); //reverse bits in each byte
				u32 *wPt = &rdwBuf[i];
				*wPt = val;
			#if 0
				u8 *rdValPt = (void*)wPt;
				localCrc = addCrc16(localCrc, (rdValPt[1]) | (rdValPt[0] <<8)); //calculate CRC
				localCrc = addCrc16(localCrc, (rdValPt[3]) | (rdValPt[2] <<8)); 	//addNmskCrc - no increase in speed.
			#endif
				i++;
				j--;
			} //while ((i & 0x3F) != 0x00);

			//resume next (paused) transfer:
			#if USE_SIO2_INTR_POLLING == 1
			//intr-clearing isn't that necesary in this case...
			*(vu32*)0xBF801070 = ~(1<<17);
			//sio2_stat_set(sio2_stat_get()); //clear local intr
			*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
			#endif
			//sio2_ctrl_set(sio2_ctrl_get() | 0x8); //clear fifos
			// sio2_data_out(0xFF); sio2_data_out(0xFF);
			*(vu32*)0xBF808268 |= 2; ///continue paused
			//sio2_ctrl_set(sio2_ctrl_get() | 0x2); //continue paused
		}

	}

#else
//16-bit variant:
//XXX: Code NOT changed for this!
	int trLenHalfWords = nrBlks <<8;

	for (i=0; i<trLenHalfWords; i+=0x80) {

		//The maximum block-size of the SIO2 is 0x100 bytes = 0x40 words
		for (j=0; j<0x80; j++) {
			//This looks odd, but results in the shortest code:
			//having no bits set in the lower 16 bits saves one instruction.
			//a  < 0x20000  comparison results in comapring with 0x1FFFF, which again is longer.
			while (!((*(vu32*)0xBF808274) >= 0x20000)) {} //this doesn't take care of the case with FIFO flags set (overflow/underflow)
			u16 val = *(vu16*)0xBF808264;
			val = reverseByteInHalfNmsk(val);
			u16 *hPt = &rdhBuf[i+j];
			*hPt = val;
			#if 1
			u8 *rdValPt = (void*)wPt;
			localCrc = addNmskCrc16(localCrc, (rdValPt[1]) | (rdValPt[0] <<8));
			#endif
		}
		//resume next (paused) transfer:
		#if USE_SIO2_INTR_POLLING == 1
		//intr-clearing isn't that necesary in this case...
		*(vu32*)0xBF801070 = ~(1<<17);
		//sio2_stat_set(sio2_stat_get()); //clear local intr
		*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
		#endif
		//sio2_ctrl_set(sio2_ctrl_get() | 0x8); //clear fifos
		// sio2_data_out(0xFF); sio2_data_out(0xFF);
		*(vu32*)0xBF808268 |= 2; ///continue paused
		//sio2_ctrl_set(sio2_ctrl_get() | 0x2); //continue paused
	}

#endif


//printf ("\n crc");

	//Start CRC tr. should already be started by above tr.
	// *(vu32*)0xBF801070 = ~(1<<17); //clear intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	//sio2_ctrl_set(sio2_ctrl_get() | 2); //continue paused
	while (!((*(vu32*)0xBF808274) >= 0x20000)) {} //wait for the CRC bytes
	cardCrc = *(vu16*)0xBF808264;
	cardCrc = (cardCrc>>8) | (cardCrc<<8);
	cardCrc = reverseByteInHalf(cardCrc);

	//while (sio2_ctrl_get() & 1) {} //wait for it to clear
	//wait for transf complete
	if (sio2WaitTransferCompletionWithTimeout() < 0) {
		printf("\n ERR: Transfer completion timed-out. ");
	}

	sio2_ctrl_set(0x3BC);//oldCtrlVal);
	#if USE_SIO2_INTR_POLLING == 1
	*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	*(vu32*)0xBF801074 |= (1<<17); //Re-enable intr
	#else
	//pause-bit - what can one do...
	*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	*(vu32*)0xBF801074 |= (1<<17); //Re-enable intr
	#endif

	*tokenOut = dataToken;
	//printf("\n token %02X ", dataToken);
	prtReadDataToken(dataToken);

	*crcOut = cardCrc;
	u16 crcChk = 0; //calcCrc16(bf, 0x200);
	if (doCrc) {
		if (cardCrc != localCrc) printf("\n ERR: CRC mismatch! CRC %04X   local %04X  %04X ", cardCrc, localCrc, crcChk);
	}
//	prtData(bf, 512);

	return 0;
}












//Universal command to read data:  PIO read - access pio port while doing CRC, bit-rev, etc.
//It can be used for data reads and reading data after CMD 6 for example.
//CRC is calculated if the value in crcOut is 0, otherwise it isn't checked nor calculated.
//dataLen must be less than 0xE00 bytes in the current implementation
//dataLen must be multiple of 4 (CRC and data token excluded)
//If the pointer for CRC is NULL, then the writeSdPData() calculates the CRC itself, othwerwise it uses(sends) the CRC pointed-to by it.
int writeSdUData(int port, u8 *tokenIo, void *buf, u16 *crcIn, int dataLen) { //The buffer MUST be 4-byte-aligned!
	int i;//, j;
	u8 *bf = (void*)buf;

	int localDataLen;
	int dataToken = *tokenIo;
	u16 cardCrc, localCrc = 0; //initial value

	int doCrc = (crcIn == NULL); //do only if pointer == NULL, otherwise use CRC pointed-to by it.

//measureTransferRateStart(0x200);


//kTODO: the folowing is not needed, but maybe a single byte of delay (dummy byte) is necessary, so send it at least...
//The actual trsnsfer will start when the write-start token is encountered.

//int h; for (h=0; h<0x1FFFFFF; h++) {}

//measureTransferRateStart(0x200);

	int fastDataBaud = 2;
	int interByteDelay = 0;


	#if USE_SIO2_INTR_POLLING == 1
	*(vu32*)0xBF801074 &= ~(1<<17);
	*(vu32*)0xBF801070 = ~(1<<17);
	sio2_ctrl_set(0x3BC); //clear old state
	#else
	*(vu32*)0xBF801074 &= ~(1<<17); //stupid pause-bit always triggerring interrupts
	sio2_ctrl_set(0x0BC); //clear old state and disable interrupts
	#endif


	struct t_pCtrlRegs {
		u32 pCtrl0;
		u32 pCtrl1;
	};
	struct t_pCtrlRegs *pCtrl = (void*)0xBF808240;

	pCtrl[port].pCtrl0 = PCTRL0_ATT_LOW_PER(0x5) | PCTRL0_ATT_MIN_HIGH_PER(0x5) | PCTRL0_BAUD0_DIV(fastDataBaud) | PCTRL0_BAUD1_DIV(fastDataBaud);
	pCtrl[port].pCtrl1 = PCTRL1_ACK_TIMEOUT_PER(0x12C) | PCTRL1_INTER_BYTE_PER(interByteDelay) | PCTRL1_UNK24(0) | PCTRL1_IF_MODE_SPI_DIFF(0);

	vu32 *queueRegs = (void*)0xBF808200;


	//Send the data token (for write or multiple-write):
	queueRegs[0] = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(0) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(1) |
		TR_CTRL_RX_DATA_SZ(0) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);
	queueRegs[1] = 0;
	*(vu8*)0xBF808260 = reverseByte(dataToken); //send byte
	*(vu32*)0xBF808268 |= 1;
	if (sio2WaitTransferCompletionWithTimeout() < 0) { //Wait for prev. tr to compl.
		printf("\n ERR: Data token sending timed-out. ");
	}


	u32 sio2BlkTrSetting = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(1) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(0) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(0) |
		TR_CTRL_RX_DATA_SZ(0) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);

	localDataLen = dataLen;

	queueRegs[0] = sio2BlkTrSetting | TR_CTRL_TX_DATA_SZ(4);
	queueRegs[1] = 0; //terminator
	//The CRC transfer is set-up separately.


	//u16 *rdhBuf = (void*)bf;
	u32 *rdwBuf = (void*)bf; //make sure it is 4-byte aligned


	//32-bit variant:

	if (doCrc) {

		int trLenWords = localDataLen >>2; //convert to words

		for (i=0; i<trLenWords; i++) {

			u32 *wPt = &rdwBuf[i];
			u32 val = *wPt;
			#if 1
			u8 *rdValPt = (void*)wPt;
			localCrc = addCrc16(localCrc, (rdValPt[1]) | (rdValPt[0] <<8)); //calculate CRC
			localCrc = addCrc16(localCrc, (rdValPt[3]) | (rdValPt[2] <<8)); 	//addNmskCrc - no increase in speed.
			#endif
			val = reverseByteInWord(val); //reverse bits in each byte

			if (sio2WaitTransferCompletionWithTimeout() < 0) { //Wait for prev. tr to compl.
				printf("\n ERR: WR Transfer completion timed-out. ");
			}

			//start next transf.
			*(vu32*)0xBF808260 = val; //send word
			*(vu32*)0xBF808268 |= 1; //re-start paused

		}

	} else {

		int trLenWords = localDataLen >>2; //convert to words

		for (i=0; i<trLenWords; i++) {

			u32 *wPt = &rdwBuf[i];
			u32 val = *wPt;
			#if 0
			u8 *rdValPt = (void*)wPt;
			localCrc = addCrc16(localCrc, (rdValPt[1]) | (rdValPt[0] <<8)); //calculate CRC
			localCrc = addCrc16(localCrc, (rdValPt[3]) | (rdValPt[2] <<8)); 	//addNmskCrc - no increase in speed.
			#endif
			val = reverseByteInWord(val); //reverse bits in each byte

			if (sio2WaitTransferCompletionWithTimeout() < 0) { //Wait for prev. tr to compl.
				printf("\n ERR: WR Transfer completion timed-out. ");
			}

			//start next transf.
			*(vu32*)0xBF808260 = val; //send word
			*(vu32*)0xBF808268 |= 1; //re-start paused

		}

	}




//printf ("\n crc");

	//Start CRC tr. should already be started by above tr.
	// *(vu32*)0xBF801070 = ~(1<<17); //clear intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	//sio2_ctrl_set(sio2_ctrl_get() | 2); //continue paused
	//while (!((*(vu32*)0xBF808274) >= 0x20000)) {} //wait for the CRC bytes


	if (sio2WaitTransferCompletionWithTimeout() < 0) { //Wait for prev. tr to compl.
		printf("\n ERR: CRC WR Transfer completion timed-out. ");
	}

	queueRegs[0] = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(0) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(0x2) |
		TR_CTRL_RX_DATA_SZ(0) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);
	queueRegs[1] = 0; //terminator

	if (!doCrc) localCrc = *crcIn; //If the CRC from the caller is used rather than doing it locally.
	cardCrc = (localCrc>>8) | (localCrc<<8);
	cardCrc = reverseByteInHalf(cardCrc);
	*(vu16*)0xBF808260 = cardCrc; //send CRC
	*(vu32*)0xBF808268 |= 1; ///re-start paused
	*crcIn = localCrc;


	//wait for transf complete
	if (sio2WaitTransferCompletionWithTimeout() < 0) {
		printf("\n ERR: Transfer completion timed-out. ");
	}

//measureTransferRateFinish(1);

		//	printf("\n FIFO State %08X  %08X  %08X ", *(vu32*)0xBF808274, *(vu32*)0xBF80826C, *(vu32*)0xBF808268);

 //for (h=0; h<0x1FFFFFF; h++) {}
	//		printf("\n FIFO State %08X  %08X  %08X ", *(vu32*)0xBF808274, *(vu32*)0xBF80826C, *(vu32*)0xBF808268);



	sio2_ctrl_set(0x3BC);//oldCtrlVal);
	#if USE_SIO2_INTR_POLLING == 1
	*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	*(vu32*)0xBF801074 |= (1<<17); //Re-enable intr
	#else
	//pause-bit - what can one do...
	*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	*(vu32*)0xBF801074 |= (1<<17); //Re-enable intr
	#endif






	return 0;
}













// 4-byte word:  CRC: 1262kB/s  noCRC: 1587kB/s, with the SDcard data-fetching time the transfer speed becomes: from 1262kB/s to 782kB/s

//#define readSdPData(a,b,c,d) readSdDMAUData(a,b,c,d,512)

//PIO read - access pio port while doing CRC, bit-rev, etc.
int readSdPData(int port, u8 *tokenOut, void *buf, u16 *crcOut) { //The buffer MUST be 4-byte-aligned!
	int i, j;
	u8 *bf = (void*)buf;

	int dataToken;
	u16 cardCrc, localCrc = 0; //initial value

	int doCrc = (*crcOut == 0); //do only if variable pointed to == 0
	//do always if crcOut == NULL; //unimplemented

//measureTransferRateStart(0x200);

	//This stage can take some time, so no need to over-optimize it
	//Do something useful here, as this stage - while the SD Card is fetching the data can take some time. Make sure SIO2 doesn't get accessed by other functions, though.
	//This is mainly period-dependant and not (?) dependant on the number of clock-pulses fed.
	i = 0;
	rdPollBytePrepare(port);
	do {
		dataToken = rdPollByteWait(0xFF, port);
		i++;
		if (i> 0x800) {printf("\n ERR: Timeout on waiting for data (token). ");  break; }
		//int h; for( h=0; h<0xFFFFF; h++) {} //test whether the number of clock pulses matters or only the time duration matters.
	} while (dataToken == 0xFF);
	rdPollByteComplete(port);
	//printf("\n Data Token wait loop iterations  %08X ", i);
	dataToken = reverseByte(dataToken);



//DONE_TODO: Check if byte-reversing is faster with LUT - just as fast (maybe a bit slower?) as the 4-byte word-variant. The test was done with the 256-byte direct LUT - the 16byte LUT is slower.





#if 0
printf("\n wait ");
int h; for (h=0; h<0x2FFFFF; h++) {}
for (h=0; h<0x8FFFFF; h++) {}
printf("\n waitEnd ");
#endif


//TODO: add setting for tr speed/freq - according to what the card supports and what works - do tests with higher speeds and chosse a lower than the one at which it works (before the one it doesn't work at).

//measureTransferRateStart(0x200);


	int fastDataBaud = 2;
	int interByteDelay = 0;


	//sio2_transfer_data_t trDat;
	//sio2_transfer_data_t *td = &trDat;


	//The block-system can be made more universal, but will there ever be a need for different-size blocks?

	//Maximum transfer size should be 7 * 2 * 0x100 = 0xE00 bytes, but it might be less.
	// each block has size of 0x200 bytes (1 "sector")
	int nrBlks = 1; //This is the block-size the card is set to. Using 512-byte blocks for maximum compatiblity. //Only block-size of 1 supported.

	#if USE_SIO2_INTR_POLLING == 1
	*(vu32*)0xBF801074 &= ~(1<<17);
	*(vu32*)0xBF801070 = ~(1<<17);
	sio2_ctrl_set(0x3BC); //clear old state
	#else
	*(vu32*)0xBF801074 &= ~(1<<17); //stupid pause-bit always triggerring interrupts
	sio2_ctrl_set(0x0BC); //clear old state and disable interrupts (but the pause-interrupt whoch can't(?) be masked).
	#endif


	struct t_pCtrlRegs {
		u32 pCtrl0;
		u32 pCtrl1;
	};
	struct t_pCtrlRegs *pCtrl = (void*)0xBF808240;

	//td->port_ctrl1[port]
	pCtrl[port].pCtrl0 = PCTRL0_ATT_LOW_PER(0x5) | PCTRL0_ATT_MIN_HIGH_PER(0x5) | PCTRL0_BAUD0_DIV(fastDataBaud) | PCTRL0_BAUD1_DIV(fastDataBaud);

	//td->port_ctrl2[port]
	pCtrl[port].pCtrl1 = PCTRL1_ACK_TIMEOUT_PER(0x12C) | PCTRL1_INTER_BYTE_PER(interByteDelay) | PCTRL1_UNK24(0) | PCTRL1_IF_MODE_SPI_DIFF(0);

	//sio2_portN_ctrl1_set(port, td->port_ctrl1[port]);
	//sio2_portN_ctrl2_set(port, td->port_ctrl2[port]);


	u32 sio2BlkTrSetting = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(1) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(0) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(1) |
		TR_CTRL_RX_DATA_SZ(0x100) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);

	vu32 *queueRegs = (void*)0xBF808200;
	i=0;
	//for (; i<(nrBlks<<1);) {
		//td->regdata[i++] = sio2BlkTrSetting;
		//td->regdata[i++] = sio2BlkTrSetting;
		queueRegs[i++] = sio2BlkTrSetting;
		queueRegs[i++] = sio2BlkTrSetting;
		*(vu32*)(0xBF808260) = 0xFFFFFFFF; //output TX data - 4 bytes - 1 byte per queue element and two more, to save time from another access for writing the byte over getting CRC.
	//}

	//The loop above should exit with i = element after the last of the block
	//set-up CRC transfer. In practice this can be done as a separate transfer.
	//td->regdata[i++] =
	queueRegs[i++] = TR_CTRL_PORT_NR(port) | TR_CTRL_PAUSE(0) |
		TR_CTRL_TX_MODE_PIO_DMA(0) | TR_CTRL_RX_MODE_PIO_DMA(0) |  TR_CTRL_NORMAL_TR(1) | TR_CTRL_SPECIAL_TR(0) |
		TR_CTRL_TX_DATA_SZ(1) |
		TR_CTRL_RX_DATA_SZ(0x2) | TR_CTRL_BAUD_DIV(0) | TR_CTRL_WAIT_ACK_FOREVER(0);

	//td->regdata[i] =
	queueRegs[i] = 0; //terminator




/*	for (i = 0; i < 4; i++) {
		sio2_portN_ctrl1_set(i, td->port_ctrl1[i]);
		sio2_portN_ctrl2_set(i, td->port_ctrl2[i]);
	} */
	#if 0
	for (i = 0; i < 16; i++) {
		//printf("\n queue elem %08X:  %08X ", i, td->regdata[i]);
		sio2_data_out(0xFF); //a byte for each transf.
		sio2_regN_set(i, td->regdata[i]);
		if (td->regdata[i] == 0) break; //make it faster
	}
	#endif
	//sio2_regN_set(0, td->regdata[0]);
	//sio2_regN_set(1, 0);

	/*if (td->in_size) {
		for (i = 0; i < td->in_size; i++)
			sio2_data_out(td->inp[i]);
	}*/






	*(vu32*)0xBF808268 |= 1; //start queue exec
	//sio2_ctrl_set(sio2_ctrl_get() | 1); //start queue exec


	//u16 *rdhBuf = (void*)bf; //make sure it is 4-byte aligned
	u32 *rdwBuf = (void*)bf;





#if 1
//32-bit variant:

if (doCrc) {

	int trLenWords = nrBlks <<7; // * 0x80 [words] = 0x200 bytes (SD card block size / standard sector-size).

	for (i=0; i<trLenWords; i+=0x40) {

		//The maximum block-size of the SIO2 is 0x100 bytes = 0x40 words
		for (j=0; j<0x40; j++) {

			while (!((*(vu32*)0xBF808274) >= 0x40000)) {} //wait for at least 4 bytes of data

			u32 val = *(vu32*)0xBF808264; //get them

			val = reverseByteInWord(val); //reverse bits in each byte
			u32 *wPt = &rdwBuf[i+j];
			*wPt = val;
		#if 1
			u8 *rdValPt = (void*)wPt;
			localCrc = addCrc16(localCrc, (rdValPt[1]) | (rdValPt[0] <<8)); //calculate CRC
			localCrc = addCrc16(localCrc, (rdValPt[3]) | (rdValPt[2] <<8)); 	//addNmskCrc - no increase in speed.
		#endif
		}

		//resume next (paused) transfer:
		#if USE_SIO2_INTR_POLLING == 1
		//intr-clearing isn't that necesary in this case...
		*(vu32*)0xBF801070 = ~(1<<17);
		//sio2_stat_set(sio2_stat_get()); //clear local intr
		*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
		#endif
		//sio2_ctrl_set(sio2_ctrl_get() | 0x8); //clear fifos
		// sio2_data_out(0xFF); sio2_data_out(0xFF);
		*(vu32*)0xBF808268 |= 2; ///continue paused
		//sio2_ctrl_set(sio2_ctrl_get() | 0x2); //continue paused
	}

} else {

	int trLenWords = nrBlks <<7; // * 0x80 [words] = 0x200 bytes (SD card block size / standard sector-size).

	for (i=0; i<trLenWords; i+=0x40) {

		//The maximum block-size of the SIO2 is 0x100 bytes = 0x40 words
		for (j=0; j<0x40; j++) {

			while (!((*(vu32*)0xBF808274) >= 0x40000)) {} //wait for at least 4 bytes of data

			u32 val = *(vu32*)0xBF808264; //get them
		#if 1
			val = reverseByteInWord(val); //reverse bits in each byte
			u32 *wPt = &rdwBuf[i+j];
			*wPt = val;
		#endif
		#if 0
			u8 *rdValPt = (void*)wPt;
			localCrc = addCrc16(localCrc, (rdValPt[1]) | (rdValPt[0] <<8)); //calculate CRC
			localCrc = addCrc16(localCrc, (rdValPt[3]) | (rdValPt[2] <<8)); 	//addNmskCrc - no increase in speed.
		#endif
		}

		//resume next (paused) transfer:
		#if USE_SIO2_INTR_POLLING == 1
		//intr-clearing isn't that necesary in this case...
		*(vu32*)0xBF801070 = ~(1<<17);
		//sio2_stat_set(sio2_stat_get()); //clear local intr
		*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
		#endif
		//sio2_ctrl_set(sio2_ctrl_get() | 0x8); //clear fifos
		// sio2_data_out(0xFF); sio2_data_out(0xFF);
		*(vu32*)0xBF808268 |= 2; ///continue paused
		//sio2_ctrl_set(sio2_ctrl_get() | 0x2); //continue paused
	}


}

#else
//16-bit variant:

	int trLenHalfWords = nrBlks <<8;

	for (i=0; i<trLenHalfWords; i+=0x80) {

		//The maximum block-size of the SIO2 is 0x100 bytes = 0x40 words
		for (j=0; j<0x80; j++) {
			//This looks odd, but results in the shortest code:
			//having no bits set in the lower 16 bits saves one instruction.
			//a  < 0x20000  comparison results in comapring with 0x1FFFF, which again is longer.
			while (!((*(vu32*)0xBF808274) >= 0x20000)) {} //this doesn't take care of the case with FIFO flags set (overflow/underflow)
			u16 val = *(vu16*)0xBF808264;
			val = reverseByteInHalfNmsk(val);
			u16 *hPt = &rdhBuf[i+j];
			*hPt = val;
			#if 1
			u8 *rdValPt = (void*)wPt;
			localCrc = addNmskCrc16(localCrc, (rdValPt[1]) | (rdValPt[0] <<8));
			#endif
		}
		//resume next (paused) transfer:
		#if USE_SIO2_INTR_POLLING == 1
		//intr-clearing isn't that necesary in this case...
		*(vu32*)0xBF801070 = ~(1<<17);
		//sio2_stat_set(sio2_stat_get()); //clear local intr
		*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
		#endif
		//sio2_ctrl_set(sio2_ctrl_get() | 0x8); //clear fifos
		// sio2_data_out(0xFF); sio2_data_out(0xFF);
		*(vu32*)0xBF808268 |= 2; ///continue paused
		//sio2_ctrl_set(sio2_ctrl_get() | 0x2); //continue paused
	}

#endif


//printf ("\n crc");

	//Start CRC tr. should already be started by above tr.
	// *(vu32*)0xBF801070 = ~(1<<17); //clear intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	//sio2_ctrl_set(sio2_ctrl_get() | 2); //continue paused
	while (!((*(vu32*)0xBF808274) >= 0x20000)) {} //wait for the CRC bytes
	cardCrc = *(vu16*)0xBF808264;
	cardCrc = (cardCrc>>8) | (cardCrc<<8);
	cardCrc = reverseByteInHalf(cardCrc);


//	printf("\n post CRC ");
// for (d=0; d<0xFFFFF; d++) {}
/*
int u;
for (u=0; u<0x40; u++) {
printf(" fst %08X ", fifoStAr[u]);
int s; for (s=0;s<0x4FFFF; s++) {}
}
*/


	//while (sio2_ctrl_get() & 1) {} //wait for it to clear
	//wait for transf complete
	if (sio2WaitTransferCompletionWithTimeout() < 0) {
		printf("\n ERR: Transfer completion timed-out. ");
	}


	sio2_ctrl_set(0x3BC);//oldCtrlVal);
	#if USE_SIO2_INTR_POLLING == 1
	*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	*(vu32*)0xBF801074 |= (1<<17); //Re-enable intr
	#else
	//pause-bit - what can one do...
	*(vu32*)0xBF801070 = ~(1<<17); //clear SIO2 intr
	//sio2_stat_set(sio2_stat_get()); //clear local intr
	*(vu32*)0xBF808280 = *(vu32*)0xBF808280; //clear local intr
	*(vu32*)0xBF801074 |= (1<<17); //Re-enable intr
	#endif




//measureTransferRateFinish(1);



//printf("\n 8268  %08X  8268 %08X  8280 %08X ", *(vu32*)0xBF808268, *(vu32*)0xBF80826C, *(vu32*)0xBF808280);



	*tokenOut = dataToken;
	//printf("\n token %02X ", dataToken);
	prtReadDataToken(dataToken);

	*crcOut = cardCrc;
	u16 crcChk = 0; //calcCrc16(bf, 0x200);
	if (doCrc) {
		if (cardCrc != localCrc) printf("\n ERR: CRC mismatch! CRC %04X   local %04X  %04X ", cardCrc, localCrc, crcChk);
	}
//printf("\n CRC %04X %04X ", cardCrc, localCrc);
//	prtData(bf, 512);

	return 0;
}
















//with baudDiv = 1 => SCK=48MHz (no kidding) but period betwen bytes = ~182ns >> (much larger than) 20.8ns cycle-rate.
//SD cards doesn't work witis clock (even though it is well-formed).

//Using divisor = 0 causes the transfer to never complete - /CS (/ATT) keeps low, no interrupt, no clock, etc.
//It is possible that 48MHz can be used by some cards or maybe when connected directly - no resistors(because there are already resistors in the ps2) (tested with direct SCKonnect - no effect)- stil bad data -crc gets 0xffff, so card sees more clock-pulses most likely.
//same when tested with SD HC 4GB (4) SD4 card.

/*

Note that for high freqiuencies the period betwen bytes doesn't get reduced below a certain value, so the speed can increase too much).
SCK div = 2 = 24MHz, inter-byte per = 0 (minimal) (unmeasured - should be ~180ns).
transfer rate of bare data (sector - test was done for 0x300 bytes) 1640 kB/s
These are raw speeds of the actual transfer (and parameter preparation). Additional SD-card processing isn't measured.

freq 		trSpd

48 MHz	1900 kB/s (transfered data is damaged)
24			1640 kB/s (fastes usable with SD cards in SPI mode)
16			1293 kB/s
12			1082 kB/s (fastes attainable on the controler port, but unsafe)
9.6		922 kB/s



24MHz - inter-byte = 198ns (178ns - whewn subtractinbg half a pulse)
16MHz - 168ns - from the same SCK transitions(as above 178) /(\)->/
12MHz - 168ns

this divisor is most likely loaded in a counter which counts up to it. This explains why it doesn't works wit a value of 0.


*/

/*

So do it like this:

- poll with single-byte reads for when the card is ready with the data.
- set-up a DMA transfer of 0x100-byte blocks, each corresponding to a transfer queue element, transfer the size of the tranferred data.
- After the actual data has been transferred (maybe to the buffe passed by the device) read the two byte CRC bvy PIO read.

- reverse the data bytes, calculate CRC at the same time and if that makes it faster - can do the transfer in PIO here, rather than through DMA. Can also copy from/to source buffer here.



TODO: check how long the actual DMA takes - most likely it only transfers to the buffer... maybe there is a second buffer for DMA?? - check what buffer contains and the memory above it.
To determine if it gets delayed for byte-transfers - cluld simply measure the time around DMA with a fast and slow frequency transf.

*/





//With the card tested, reads can be done even with "disrupted" /CS.
//note: there might be a function to combine the /CS (/ATT) of consecutive commands in the queue, so this and dma could be used maybe to do long transfrs.
//a hack would be to fill the pio fifo less than the transfer size and use that to keep /CS active once the fifo runs-out (if it stays active).





u32 dataBfW[0x100];
u8 *dataBf = (void*)dataBfW;


int prtRsp(u8 r1, u32 rsp) {

	if ((r1 & 0xFE) == 0) return 0;

		char EnoString[] = "";
		char Eidle[] = "Idle state";
		char *EidleS = Eidle;
		if (R1_IDLE(r1) == 0) EidleS = EnoString;
		char EeraseRst[] = "Erase Reset";
		char *EeraseRstS = EeraseRst;
		if (R1_ERASE_RST(r1) == 0) EeraseRstS = EnoString;
		char EillegalC[] = "Illegal Cmd";
		char *EillegalCS = EillegalC;
		if (R1_ILLEGAL_CMD(r1) == 0) EillegalCS = EnoString;
		char EcrcErr[] = "CRC error";
		char *EcrcErrS = EcrcErr;
		if (R1_CRC_ERR(r1) == 0) EcrcErrS = EnoString;
		char EerSeq[] = "Erase Sequence error ";
		char *EerSeqS = EerSeq;
		if (R1_ERR_ERASE_SEQ(r1) == 0) EerSeqS = EnoString;
		char Eaddr[] = "Address error";
		char *EaddrS = Eaddr;
		if (R1_ADDR_ERR(r1) == 0) EaddrS = EnoString;
		char Eparam[] = "Parameter error";
		char *EparamS = Eparam;
		if (R1_PARAM_ERR(r1) == 0) EparamS = EnoString;
		char EnoResp[] = "No Response error";
		char *EnoRespS = EnoResp;
		if (R1_CONST0(r1) == 0) EnoRespS = EnoString;

		printf("\n r1 %02X  rsp %08X   %s  %s  %s  %s  %s  %s  %s  %s  ", r1, rsp,  EidleS, EeraseRstS, EillegalCS, EcrcErrS, EerSeqS, EaddrS, EparamS, EnoRespS);

	return 0;
}

//CRC is enabled/disabled internally... maybe or maybe add special CMDs for it - when set at fixed value
//When crc is 0, then it isn't used. When at least the lsb is set (the last in transfer) then it is used. The lsb must always be set in(after) the CRC of a CMD.
u32 SdCmdInternBuf[0x100];
int sendSdCmd(int port, u8 cmd, u32 argIn, u8 crc, u8 *r1out, u32 *respOut) {
	int i;
	u8 *bf = (void*)SdCmdInternBuf;
	cmd &= 0x3F;
	//Clear buffer to default value:
	SdCmdInternBuf[1] = 0xFFFFFFFF; //u32
	SdCmdInternBuf[2] = 0xFFFFFFFF;
	SdCmdInternBuf[3] = 0xFFFFFFFF;
	SdCmdInternBuf[4] = 0xFFFFFFFF;
	bf[0] = cmd | 0x40;
	bf[1] = (argIn >>24) & 0xFF;
	bf[2] = (argIn >>16) & 0xFF;
	bf[3] = (argIn >>8) & 0xFF;
	bf[4] = (argIn >>0) & 0xFF;
	if (crc & 1) bf[5] = crc;
	else bf[5] = (calcCrc7(bf, 5) <<1) | 1; //CRC
	//data sent while waiting for response
	//for (i=0; i<(8+5); i++) bf[6+i] = 0xFF; //clear buf (donea bove faster)
	//int ioLen = 6 + 8 + 5; //cmd + max wait 8 bytes + max resp 5 bytes

	//Some data (CSD-reg-read) commands respond with the data only a byte after the R1 response. To prevent this eating this data, read bytes individually.
	#if 0
	sendCmd(bf, bf, ioLen, ioLen, port);
 	for (i=6; i<(6+8); i++) { if ((bf[i] & 0x80) == 0) break; } //if (bf[i] != 0xFF) break; }
	#else
	sendCmd(bf, bf, 6, 6, port); //send cmd
	for (i=0; i<=8; i++) {
		bf[0] = 0xFF;
		sendCmd(bf, bf, 1, 1, port);
		if ((bf[0] & 0x80) == 0) break; //Wait for response first byte (R1).
	}
	int r1val = bf[0];
	int respLen = 0;
	if (respOut != NULL) {
		respLen = *respOut;
		//if (respLen >  4) //FIXME: make the calling code support this.
			respLen = 4;
		SdCmdInternBuf[0] = 0xFFFFFFFF;
		SdCmdInternBuf[1] = 0xFFFFFFFF;
		sendCmd(&bf[4], &bf[4], respLen, respLen, port); //alignment
	}
	bf[3] = r1val;
	i = 3;
	#endif

//TODO: optimize the code above! - maybe add the SIO2 I/O funcs in it directly?

	u8 r1 = bf[i];
	u32 rsp = (bf[i+1] <<24) | (bf[i+2] <<16) | (bf[i+3] <<8) | (bf[i+4] <<0);
	*r1out = r1;
	if (respOut != NULL) *respOut = rsp;
	prtRsp(r1, rsp);

	return 0;
}


//options is used by this code to specify special processing
//0	1=don't print errors
//1	1=wait for 0xFF to be returned - for R1b - busy responses. For some reson CMD12 returns R1b with all errors in response. Maybe this is normal as it is R1b.
int sendSdCmdB(int port, u8 cmd, u32 argIn, u8 crc, u8 *r1out, u32 *respOut, int options) {
	int i;
	u8 *bf = (void*)SdCmdInternBuf;
	cmd &= 0x3F;
	//Clear buffer to default value:
	SdCmdInternBuf[1] = 0xFFFFFFFF; //u32
	SdCmdInternBuf[2] = 0xFFFFFFFF;
	SdCmdInternBuf[3] = 0xFFFFFFFF;
	SdCmdInternBuf[4] = 0xFFFFFFFF;
	bf[0] = cmd | 0x40;
	bf[1] = (argIn >>24) & 0xFF;
	bf[2] = (argIn >>16) & 0xFF;
	bf[3] = (argIn >>8) & 0xFF;
	bf[4] = (argIn >>0) & 0xFF;
	if (crc & 1) bf[5] = crc;
	else bf[5] = (calcCrc7(bf, 5) <<1) | 1; //CRC
	//data sent while waiting for response
	//for (i=0; i<(8+5); i++) bf[6+i] = 0xFF; //clear buf (donea bove faster)
	int ioLen = 6 + 8 + 5; //cmd + max wait 8 bytes + max resp 5 bytes
	sendCmd(bf, bf, ioLen, ioLen, port);

 	for (i=6; i<(6+8); i++) { if ((bf[i] & 0x80) == 0) break; } //if (bf[i] != 0xFF) break; }
	u8 r1 = bf[i];
	u32 rsp = (bf[i+1] <<24) | (bf[i+2] <<16) | (bf[i+3] <<8) | (bf[i+4] <<0);
	*r1out = r1;
	*respOut = rsp;

	if ((options & 1) == 0) prtRsp(r1, rsp);

	if (options & 2) {
		bf[0] = bf[ioLen-1]; //test the last read byte.
		while (bf[0] != 0xFF) {
			bf[0] = 0xFF;
			sendCmd(bf, bf, 1, 1, port);
		}
	}

	return 0;
}





int getSdDataRespToken(int port) {
	int dataResp;
	u32 bfs;
	u8 *bf = (void*)&bfs;
	bf[0] = 0xFF;
	int ioLen = 1;
	sendCmd(bf, bf, ioLen, ioLen, port);
	dataResp = bf[0];
	return dataResp;
}

int waitSdBusy(int port, int timeout) {
	//int ret = 0;
	u32 bfs;
	u8 *bf = (void*)&bfs;
	int ioLen = 1;
	int delayShift = 4; //1<<4 = 16 us
	//int delay = 1<<delayShift; //timeout >>
	timeout = timeout >>delayShift;
	//do {
	while (1) {
		bf[0] = 0xFF;
		sendCmd(bf, bf, ioLen, ioLen, port);
		if (bf[0] == 0xFF) // != 0x00) //the comparison with 0xFF should be safer... maybe.
			break;
	//XXX	DelayThread(delay);
		timeout--;
		if (timeout <= 0) return -1;
	} //while (bf[0] != 0xFF); //0x00); //the comparison with 0xFF should be safer... maybe.
	return 0;
}


int sendSdWriteStopDataToken(int port) {
	u32 bfs;
	u8 *bf = (void*)&bfs;
	bf[0] = STOP_DATA_TOKEN;
	bf[1] = 0xFF; //another byte needs to be clocked, before the busy signal appears. This is usually returned as 0xFF, so not clocking it would make the busy loop exit early.
	int ioLen = 2;
	sendCmd(bf, bf, ioLen, ioLen, port);
	return bfs;
}





//TODO: add DMA-variants of the sector-I/O functions.




//The current implementation does bit-reversing and CRC for each 4 bytes before sending them at once in a separate transfer. This is necessary, becasue the SIO2 doesn't seem to have an option to wait for the data before sending it.
//A faster way for multiple blocks would be to do the bit-reversing and CRC for the next block while waiting for the current one to be written to Flash. However this method would make single (and maybe even 2, 3 ) -block transfers much slower.
//current raw transfer speed: 1133kB/s with or without CRC - because sending the 4 bytes takes longer than calculating the CRC.



int wrSect(int port, void *dataBf, u32 sectorAddr, u32 nrSect, int sameBuf, int addrMode) {
	int ret = 0, i;
	u32 cardAddr = sectorAddr;
	if (addrMode == 0) cardAddr *= 512;
	u8 r1 = 0; u32 rsp = 0;

	if (nrSect == 1) {

		//CMD 24 write single block; R1
		i = 0;
		do {
			sendSdCmd(port, 24, cardAddr, 0, &r1, &rsp); //the arg is LBA in 512-byte sectors
			i++;
			if (i> 0) break; //0x10) break;
		} while (r1 != 0x00);
		if (r1 != 0x00) { printf("\n CMD 24 response error %02X ", r1); return -1; }

		while (nrSect > 0) {
			u8 token = C171824_DATA_TOKEN; //0xFE= single block / 0xFC = multiple blocks.
			//u16 crc = 0; //If the pointer for CRC is NULL, then the writeSdPData() calculates the CRC itself, othwerwise it uses(sends) the CRC pointed-to by it.
			u16 *crcPt = NULL; //&crc; //NULL;
			writeSdUData(port, &token, dataBf, crcPt, 512);
			//On return the sent CRC value is placed in crcPt.
			//if (addrMode == 0) cardAddr += 512;
			//else cardAddr++;
			nrSect--;
			if (sameBuf == 0) ((u32)dataBf) += 512;

			int dataResp = getSdDataRespToken(port);
			//printf("\n resp token %02X   crc %04X ", dataResp, *crcPt);
			if (dataResp < 0) { ret = dataResp; /*break;*/ }
			ret = chkPrtWriteDataRespToken(dataResp);
			int timeout = 1000000; //us
			ret |= waitSdBusy(port, timeout);
			if (ret < 0) break;
		}

	} else {

		//CMD 25 write multiple blocks; R1
		i = 0;
		do {
			sendSdCmd(port, 25, cardAddr, 0, &r1, &rsp); //the arg is LBA in 512-byte sectors
			i++;
			if (i> 0) break; //0x10) break;
		} while (r1 != 0x00);
		if (r1 != 0x00) { printf("\n CMD 25 response error %02X ", r1); return -1; }

		while (nrSect > 0) {
			u8 token = C25_DATA_TOKEN; //0xFE= single block / 0xFC = multiple blocks.
			//u16 crc = 0; //If the pointer for CRC is NULL, then the writeSdPData() calculates the CRC itself, othwerwise it uses(sends) the CRC pointed-to by it.
			u16 *crcPt = NULL; //&crc; //NULL;
			writeSdUData(port, &token, dataBf, crcPt, 512);
			//On return the sent CRC value is placed in crcPt.
			//if (addrMode == 0) cardAddr += 512;
			//else cardAddr++;
			nrSect--;
			if (sameBuf == 0) ((u32)dataBf) += 512;

			int dataResp = getSdDataRespToken(port);
			//printf("\n resp token %02X   crc %04X ", dataResp, *crcPt);
			if (dataResp < 0) { ret = dataResp; /*break;*/ }
			ret = chkPrtWriteDataRespToken(dataResp);
			int timeout = 1000000; //us
			ret |= waitSdBusy(port, timeout);
			if (ret < 0) break;
		}

		sendSdWriteStopDataToken(port); //0xFD End write of multiple blocks
		ret |= waitSdBusy(port, 1000000);
		//int va = sendSdWriteStopDataToken(port);
		//printf("\n  %X ", va);

	}

	return ret;
}


/*
write speeds of multi-transfer + wait for data to be written to flash:
block	transfer rate kB/s
1		81
2		187 kB/s
3		220

*/



int rdSect(int port, void *dataBf, u32 sectorAddr, u32 nrSect, int sameBuf, int addrMode) {
	int i;
	u32 cardAddr = sectorAddr;
	if (addrMode == 0) cardAddr *= 512;
	u8 r1 = 0; u32 rsp = 0;
	#if 0
	while (nrSect > 0) {
		u8 r1 = 0; u32 rsp = 0;
		//CMD 17 read single block; R1
		i = 0;
	do {
		sendSdCmd(port, 17, cardAddr, 0, &r1, &rsp); //the arg is LBA in 512-byte sectors
	//	if (r1 != 0x00) {
	//		printf("\n ERR: CMD 17  RSP %02X  ", r1);
	//	}
		i++;
		if (i> 0x10) break;
	} while (r1 != 0x00);
	if (r1 != 0x00) { printf("\n CMD 17 response error %02X ", r1); return -1; }

		u8 token;
		u16 crc = 0; // 0= do CRC check / else - don't do - CRC value from card is returned in the variable crc.
		//readSdData(port, &token, dataBf, &crc);
		readSdPData(port, &token, dataBf, &crc);

		if (addrMode == 0) cardAddr += 512;
		else cardAddr++;
		nrSect--;
		if (sameBuf == 0) ((u32)dataBf) += 512;
	}

	#else

	//CMD 18 read multiple blocks; R1
	i = 0;
	do {
		sendSdCmd(port, 18, cardAddr, 0, &r1, &rsp); //the arg is LBA in 512-byte sectors
	//	if (r1 != 0x00) {
	//		printf("\n ERR: CMD 17  RSP %02X  ", r1);
	//	}
		i++;
		if (i> 0x10) break;
	} while (r1 != 0x00);
	if (r1 != 0x00) { printf("\n CMD 18 response error %02X ", r1); return -1; }

	while (nrSect > 0) {
		u8 token;
		u16 crc = 1; // 0= do CRC check / else - don't do
		//readSdData(port, &token, dataBf, &crc);
		readSdPData(port, &token, dataBf, &crc);
		//if (addrMode == 0) cardAddr += 512;
		//else cardAddr++;
		nrSect--;
		if (sameBuf == 0) ((u32)dataBf) += 512;
	}
	//For some reson CMD12 returns R1b with all errors in response. Maybe this is normal as it is R1b.
	sendSdCmdB(port, 12, 0x00, 0, &r1, &rsp, 0x3); //stop transmission of multiple sectors

	#endif

	return 0;//ret;
}














//This has to be made device and port-specific and support multiple devices (at least 2).
struct t_commData {
	int port;
	int addrMode;
};

struct t_commData comDat;
struct t_commData *cd = &comDat;


#include "bdm.h"

struct block_device blockDevCfg;
struct block_device *bdc = &blockDevCfg;
char devNm[] = "sdc"; // What happens when two devs with the same name are registerated - shouldn't numbers be set by the BDM?

//uLE enters infinite loop opening directory on SD card when that is attempted under the root directory. it probably isn't because of intrs here. This is partially fixed by waiting for (the previous) SIO2 transfer to complete. Still for some reason reading a JPEG with uLE fails with multiple retries with errors -2 / -21 / -19 (and one "successfull" opening with "JPEG datastream contains no image").

int readSector(struct block_device* bd, u32 sector, void* buffer, u16 count) {
	int OldState;
	int k;
//CpuResumeIntr(1);
//TODO: calling printf here causes code to hang. maybe because interrupts are disabled - why???
//printf("\n rd st ");
//return 0;
	FlushDcache();
//	while ((*(vu32*)0xBF80826C & (1<<12)) == 0) {
		k = 0;
		while ((*(vu32*)0xBF80826C & (1<<12)) == 0) {k++; if (k > 0xFFFFFF) { CpuResumeIntr(1); printf("\n SIO2 extr transf comp timeout "); break; } }
//	}
	CpuSuspendIntr(&OldState);
	int sameBuf = 0;
//void *bufferA = SdCmdInternBuf;
	rdSect(cd->port, buffer, sector, count, sameBuf, cd->addrMode);
//	*(vu32*)0xBF801070 &= ~(1<<17);
	CpuResumeIntr(OldState);
	FlushDcache();
//printf("\n rd en ");
	return count;
}

int writeSector(struct block_device* bd, u32 sector, const void *buffer, u16 count) {
	int OldState;
	int k;
//printf("\n wr st ");
//return 0;
	FlushDcache();
//	while ((*(vu32*)0xBF80826C & (1<<12)) == 0) {
		k = 0;
		while ((*(vu32*)0xBF80826C & (1<<12)) == 0) {k++; if (k > 0xFFFFFF) { CpuResumeIntr(1); printf("\n SIO2 extr transf comp timeout "); break; } }
//	}
	CpuSuspendIntr(&OldState);
	int sameBuf = 0;
	wrSect(cd->port, (void*)buffer, sector, count, sameBuf, cd->addrMode);
//	*(vu32*)0xBF801070 &= ~(1<<17);
	CpuResumeIntr(OldState);
	FlushDcache();
//printf("\n wr en ");
	return count;
}

void flush(struct block_device* bd) {
	//? there shouldn't be any need to do anything here, nor do any de-initialization before "power-off".
	return;
}

//TODO: slim PS2 has problems - maybe related to emulation - if card is left connected on boot-up it returns wrong data afterwards... so unlike the fat version, the slim does access MC port 2. The slim PS2 is also slower (because emulation and much reg-access).

//on the SCPH-30003: Accessing bad (or maybe containing a lot of files) FAT32 directories causes odd errors - black screen or incompletely drawn screen hanging - maybe unhandled interrupt or memory overwrite.



//Because the original is in big-endian, using this bad way to convert it -
//when there is a "Hi" and "Lo" value, do ( (Hi << x) | Lo), where x is the bit-length of Lo. This becomes more problematic for fields larger than a byte.

//= (c_sizeHi<<10) | (c_sizeMd<<2) | c_sizeLo;

struct t_csdVer1 {
	u8		reserved1 		:6;
	u8		csd_structure 	:2;
	u8		taac 				:8;
	u8		nsac 				:8;
	u8		tran_speed		:8;

	u16	cccHi				:8;
	u16	read_bl_len		:4;
	u16	cccLo				:4;

	u32	c_sizeHi			:2;
	u8		read_bl_partial		:1;
	u8		write_blk_misalign	:1;
	u8		read_blk_misalign		:1;
	u8		dsr_imp			:1;
	u8		reserved2		:2;

	u32	c_sizeMd			:8;

	u32	c_sizeLo			:2;
	u8		vdd_r_curr_max	:3;
	u8		vdd_r_curr_min	:3;

	u8		c_size_multHi	:2;
	u8		vdd_w_curr_max	:3;
	u8		vdd_w_curr_min	:3;

	u8		c_size_multLo	:1;
//XXX: The folowing are not fixed to correct endianness!
	u8		erase_blk_en	:1;
	u8		sector_size		:7;
	u8		wp_grp_size		:7;
 	u8		ep_grp_enable	:1;
	u8		reserved5		:2;
	u8		r2w_factor		:3;
	u8		write_bl_len	:4;
	u8		write_bl_partial	:1;
	u8		reserved6		:5;
	u8		file_format_group	:1;
	u8		copy				:1;
	u8		perm_write_protect	:1;
	u8		tmp_write_protect	:1;
	u8		file_format		:2;
	u8		reserved7		:2;
	u8		crc				:7;
	u8		fixedTo1			:1;
};

struct t_csdVer2 {
	u8		reserved1 		:6;
	u8		csd_structure 	:2;
	u8		taac 				:8;
	u8		nsac 				:8;
	u8		tran_speed		:8;

	u16	cccHi				:8;
	u16	read_bl_len		:4;
	u16	cccLo				:4;

//Not fixed!:
	u8		read_bl_partial		:1;
	u8		write_blk_misalign	:1;
	u8		read_blk_misalign		:1;
	u8		dsr_imp			:1;
	u8		reserved2		:4;

//fixed:
	u32	c_sizeHi			:6;
	u8		reserved3		:2;//I give up - this is far too unaligned...
	// 79:58
	u32	c_sizeMd			:8;
	u32	c_sizeLo			:8;// 16 22;

//Not fixed!:
	u32	reserved4		:1;
	u8		erase_blk_en	:1;
	u8		sector_size		:7;
	u8		wp_grp_size		:7;
 	u8		ep_grp_enable	:1;
	u8		reserved5		:2;
	u8		r2w_factor		:3;
	u8		write_bl_len	:4;
	u8		write_bl_partial	:1;
	u8		reserved6		:5;
	u8		file_format_group	:1;
	u8		copy				:1;
	u8		perm_write_protect	:1;
	u8		tmp_write_protect	:1;
	u8		file_format		:2;
	u8		reserved7		:2;
	u8		crc				:7;
	u8		fixedTo1			:1;
};






int initBlockDev(void) {
	bdc->priv = NULL; //for now
	bdc->name = devNm;
	bdc->devNr = 0; //shouldn't the BDM set this?
	bdc->parNr = 0; //why does a device need to set a partition for itself - shouldn't the partition be set by the partition driver?
	bdc->sectorSize = 0x200; //the usual

	u8 r1;
	u32 cdsRegData[16>>2]; u8 *cdsReg = (void*)cdsRegData;
int i;

	for (i=0;i<16;i++) cdsReg[i] = 0xFF;
	//Get size be reading the card's CSD reg:
	sendSdCmd(cd->port, 9, 0, 0, &r1, NULL); //R1 //Use rsp pointer of NULL, when it shouldn't be read from serial data - in case there is actually data there.

	if (r1 != 0x00) printf("\n ERR: CMD 9 resp %02X ", r1);
	u16 crc = 0;
	u8 token;
	readSdUData(cd->port, &token, cdsReg, &crc, 16); //get CMD 9 data = CSD
//sendCmd(cdsReg, cdsReg, 16, 16, cd->port);
	printf("\n CSD reg: ");
	prtData(cdsReg, 16);
	struct t_csdVer1 *csdv1 = (void*)cdsReg;
	struct t_csdVer2 *csdv2 = (void*)cdsReg;

	if (csdv1->csd_structure == 0) { //ver 1
		printf("\n CSD ver 1 ");
		//Calculate size
		u32 c_size_mult = (csdv1->c_size_multHi<<1) | csdv1->c_size_multLo;
		u32 c_size = (csdv1->c_sizeHi<<10) | (csdv1->c_sizeMd<<2) | csdv1->c_sizeLo;
		u32 read_bl_len = csdv1->read_bl_len;
		u32 blockNr, blockLen;
		blockLen = 1 << read_bl_len;
		blockNr = (c_size + 1) << (c_size_mult + 2);
		u32 memCapacity = blockNr * blockLen;
		printf("\n cSize %08X  mult %08X  blkNr %08X  blkLn^ %08X blkLen %08X ", c_size, c_size_mult, blockNr, read_bl_len, blockLen );
		bdc->sectorCount = memCapacity / 0x200; //to sectors
		printf("\n Capacity %08X = %d MB = %08X  512-byte sectors ", memCapacity, (memCapacity / (1024*1024)), bdc->sectorCount);

	} else if (csdv1->csd_structure == 1) { //ver 2
#if 0
		printf("\n CSD ver 2 ");
		u32 c_size = (csdv2->c_sizeHi <<16) | (csdv2->c_sizeMd <<8) | csdv2->c_sizeLo;
		u32 read_bl_len = csdv2->read_bl_len;
		u32 memCapacity = (c_size + 1) * 0x200 * 1024;
		printf("\n cSize %08X blkLn^ %08X ", c_size, read_bl_len);
		bdc->sectorCount = memCapacity / 0x200; //to sectors
		printf("\n Capacity %08X = %d MB = %08X  512-byte sectors ", memCapacity, (memCapacity / (1024*1024)), bdc->sectorCount);
#else
		printf("\n CSD ver 2 ");
		u32 c_size = (csdv2->c_sizeHi <<16) | (csdv2->c_sizeMd <<8) | csdv2->c_sizeLo;
		u32 read_bl_len = csdv2->read_bl_len;
		u32 memCapacityKsectors = (c_size + 1);// * 0x200 * 1024;
		printf("\n cSize %08X blkLn^ %08X ", c_size, read_bl_len);
		bdc->sectorCount = memCapacityKsectors * 1024; // memCapacity / 0x200; //to sectors //WARNING: can overflow!
		printf("\n Capacity %d MB = %08X  512-byte sectors ", (memCapacityKsectors / 2), bdc->sectorCount);
		if ((bdc->sectorCount / 1024) != memCapacityKsectors) printf("\n ERR: SD card with too high capacity used => sector count variable overflowing can result in incorrect filesystem operation! ");
#endif
	}


	bdc->read  = &readSector;
	bdc->write = &writeSector;
	bdc->flush = &flush;
#if BDM_EN == 1
	bdm_connect_bd(bdc);
#endif
	return 0;
}



/*

On PC (SDIO with 50MHz maybe) the 2GB card gets 12.5MB/s read and 6MB/s write.
This can be explained by:
50 MHz / 2 = 25MHz (my case 24MHz)
4-data line SDIO / 4 = 1 data line SPI
=> (12.5 *1024) = 12800kB/s / 8 = 1600kB/s - just what I got in raw transfer speed.


It seems like the the speed of the raw transfer highly affects the speed of the total transfer - including SDcard fetching data from flash.
What happens most likely is that, even though the PIO-processing-interleaved transfer is much faster than DMA (the processing - byte-reversing and CRC) for which takes more time, PIO ends-up slower, because the SD Card will only fetch more data once the previous block has been read. So even though the raw transfer is at the highest-possible speed, it is actually slowing the SD card down, because it cant start fetchng the next data earlier.
This implementation makes the data delay minimal, but the overall speed suffers.

To work-around this, use highest possible transfer speed and DMA to get all the data. Then the SD card will have more time to fetch the next data through which the byte-bit reversing and CRC can be done.


XXX: on the fat ps2 having /ACK connected low on boot-up actually does leave it boot fine - maybe just after a bit more time..??

*/





void *malloc(int size){
	void *result;
	int OldState;

	CpuSuspendIntr(&OldState);
	result = AllocSysMemory(ALLOC_FIRST, size, NULL);
	CpuResumeIntr(OldState);

	return result;
}



void testSio2SdCardIo(void) {
	//int i;

	int port = 3;
	cd->port = port;


	int addrMode = 0;
	u8 r1 = 0xFF;
	u32 rsp = 0xFFFFFFFF;

	sendSdCmd(port, 0, 0x00, 0x95, &r1, &rsp);

	if (r1 != 0x01) {
		printf("\n ERR: CMD 0  RSP %02X  -> CMD 0 doesn't receive response soon on some cards, so this error is normal, if there are no errors after this point. ", r1);

		int i; for (i=0; i< 0xFFFFFF; i++) {}
		r1 = 0xFF;
		rsp = 0xFFFFFFFF;
		sendSdCmd(port, 0, 0x00, 0x95, &r1, &rsp);
	}

	if (r1 != 0x01) {
		printf("\n ERR: CMD 0  RSP %02X  ", r1);
#if 1
printf("\n Skipping init ");
		goto skipInit;
#endif
		goto unknownCard;
	}


	sendSdCmd(port, 8, 0x01AA, 0, &r1, &rsp); //R7
	printf("\n CMD 8  RSP %02X %08X ", r1, rsp);

	if (r1 == 0x01) {
		//SD ver.2 check

		if ((rsp & 0x0FFF) != 0x01AA) {
			printf("\n ERR: Accepted voltage mismatch: CMD 8  RSP %02X %08X ", r1, rsp);
			goto unknownCard;
		}

		int pass = 0;
		do {
			sendSdCmd(port, 55, 0x00, 0, &r1, &rsp); //R1  Sigtnal that the next command is an A-Application-specific command.
			sendSdCmd(port, 41, 0x40000000, 0, &r1, &rsp); //R1
			pass++;
		} while (r1 == 0x01);
		//Add timeout?
		printf("\n done %d passes ", pass);

		if (r1 == 0x00) {
			printf("\n Detected SD ver.2+ card ");

			sendSdCmd(port, 58, 0x00, 0, &r1, &rsp); //R3
			if (r1 != 0x00) printf("\n r1 %02X ", r1);
			if (rsp & 0x40000000) { //CCS Card Capacity Status bit
				printf("\n SD ver.2+ Block address ");
				addrMode = 1;
			}
		} else {
			printf("\n ERR: failed on SD card v.2+ cmd %02X arg %08X  r1 %02X resp %08X ", 58, 0, r1, rsp);
			goto unknownCard;
		}

	} else { //other type of card:
		//SD ver.1 check
		int pass = 0;
		do {
			sendSdCmd(port, 55, 0x00, 0, &r1, &rsp); //R1  Sigtnal that the next command is an 'A'-Application-specific command.
			sendSdCmd(port, 41, 0x00000000, 0, &r1, &rsp); //R1
			pass++;
		} while (r1 == 0x01);
		//Add timeout?
		printf("\n done %d passes ", pass);

		if (r1 == 0x00) {
			printf("\n Detected SD ver.1 card ");
		} else { //err or no response => other card type
			//MMC ver.3 check
			int pass = 0;
			do {
				sendSdCmd(port, 1, 0x00000000, 0, &r1, &rsp); //R1
				pass++;
			} while (r1 == 0x01);
			//Add timeout?
			printf("\n done %d passes ", pass);

			if (r1 == 0x00) {
				printf("\n Detected MMC ver.3 card ");
			} else {
				goto unknownCard;
			}
		}
	}


	if (addrMode == 0) {
		printf("\n Set block size in byte addressing mode ");
		sendSdCmd(port, 16, 0x00000200, 0, &r1, &rsp); //R1
		if (r1 != 0x00) {
			printf("\n ERR: Block size in byte addressing mode seting failed %02X ", r1);
		}
	}

	cd->addrMode = addrMode;

	printf("\n Card successfully initialized. ");



fastDiv = 1;


skipInit:


	do{}while(0);


	u32 sectorAddr = 0x120;
	u32 cardAddr = sectorAddr;
	if (addrMode == 0) cardAddr *= 512;

	//CMD 17 read single block; R1
	sendSdCmd(port, 17, cardAddr, 0, &r1, &rsp); //the arg is LBA in 512-byte sectors
	if (r1 != 0x00) {
		printf("\n ERR: CMD 17  RSP %02X  ", r1);
	}

	u8 token;
	u16 crc;
	readSdPData(port, &token, dataBf, &crc);



	sendSdCmd(port, 59, 0x00000001, 0, &r1, &rsp); //Enable CRC: 0=off/1=on
	if (r1 != 0x00) {
		printf("\n ERR: CMD 59  RSP %02X  ", r1);
	}


//int tstNrSect = 16;


#if 0
int t;

int trRate[36];

t = 4;
measureTransferRateStart(0x200 * t);
rdSect(port, dataBf, 0x130, t, 1, addrMode);
trRate[33] = measureTransferRateFinish(0);

t = 8;
measureTransferRateStart(0x200 * t);
rdSect(port, dataBf, 0x138, t, 1, addrMode);
trRate[34] = measureTransferRateFinish(0);

for (t=1; t<=32; t++) {
	measureTransferRateStart(0x200 * t);
	rdSect(port, dataBf, 0x130, t, 1, addrMode);
	trRate[t-1] = measureTransferRateFinish(0);
}

for (t=0; t<35; t++) {
printf("\n sz %08X  %d kB/s  ", 0x200*(t+1), trRate[t]);
}
#endif

#if 0
printf("\n WRITING: ");
 for (i=0; i< 512; i++) ((u8*)dataBf)[i] = -i;

measureTransferRateStart(0x200 * nrSect);

wrSect(port, dataBf, 0x130, nrSect, 1, addrMode);

measureTransferRateFinish();

#endif

/*
	rdSect(port, dataBf, 0x130, 1, 1, addrMode);
prtData(dataBf, 512);
	rdSect(port, dataBf, 0x131, 1, 1, addrMode);
prtData(dataBf, 512);
	rdSect(port, dataBf, 0x132, 1, 1, addrMode);
prtData(dataBf, 512);
	rdSect(port, dataBf, 0x133, 1, 1, addrMode);
prtData(dataBf, 512);
*/
/*
printf("\n WRITING: ");
measureTransferRateStart(0x200 * 1);
wrSect(port, sct0x132, 0x132, 1, 1, addrMode);
measureTransferRateFinish(1);
*/


initBlockDev();





#if 0


	printf("\n CMD 6 tests for 50MHz clock ");

	sendSdCmd(port, 6, 0x00FFFFFF, 0, &r1, &rsp); //bit 31=0 = Mode0 = "Check"-mode; 23:00= 0xF,... means no change   //R1
	//if (r1 != 0x00) {
	//	printf("\n ERR: Block size in byte addressing mode seting failed %02X ", r1);
	//}
	printf("\n CMD 6 resp %02X %04X ", r1, rsp);


#if 0

	u8 *bf = (void*)SdCmdInternBuf;
	for (i=0;i<64;i++) bf[i] = 0xFF;
	int ioLen = 7; //7

//	sendCmd(bf, bf, ioLen, ioLen, port);
	prtData(bf, ioLen);
for (i=0;i<64;i++) bf[i] = 0xFF;
	//sendCmd(bf, bf, ioLen, ioLen, port);
	prtData(bf, ioLen);


	i = 0;
	int dataToken;
#if 1
	rdPollBytePrepare(port);
	do {
		dataToken = rdPollByteWait(0xFF, port);
		i++;
		if (i> 0x800) {printf("\n ERR: Timeout on waiting for data (token). ");  break; }
		//int h; for( h=0; h<0xFFFFF; h++) {} //test whether the number of clock pulses matters or only the time duration matters.
	} while ((dataToken == 0xFF));// | (dataToken == 0x00));
#endif
//for some reason returns 0x00 once the very first time - maybe it thinks the response is re-read, given how it also usually outputs just 4 0xFF bytes after that 0x00 byte and then the data token. There are cases however when the 0xFF bytes are 5, so this loop is necessary.  Although the /CS in that polling does get interrupted every byte read and the durations for this test are ~1us active / ~1us inact, the card still switches between bytes correctly, so using this is probably OK...
//Bit isn't! - after polling for the data token, reading afterwards still reads the response and the token, so it is pretty certain the response-read must be done as a single /CS-pulse read.
//The above is somewhat wrong - data can stil be read afterwards
//A different effect is observed when reading only part of the data the first time, but entering the payload (7/8 bytes) - then the next read gets the data after that
//now it seems to work for some reason. It behaves just like a data-read which is great! After outputting the 64 bytes, there are two more bytes - CRC most likely and then all 0xFF (checked up to 0x100 bytes after that - the 64 data bytes do not repeat, so this looks good!).


	rdPollByteComplete(port);
	printf("\n Data Token wait loop iterations  %08X ", i);
	dataToken = reverseByte(dataToken);
	printf("\n data token %02X ", dataToken);



	//read additional status of cmd6 = 512 bits = 64 bytes
	//int i;
	//u8 *bf = (void*)SdCmdInternBuf;
ioLen = 0x100; //64
	for (i=0;i<ioLen;i++) bf[i] = 0xFF;


	sendCmd(bf, bf, ioLen, ioLen, port);

 	//for (i=6; i<(6+8); i++) { if ((bf[i] & 0x80) == 0) break; } //if (bf[i] != 0xFF) break; }
	//u8 r1 = bf[i];
	//u32 rsp = (bf[i+1] <<24) | (bf[i+2] <<16) | (bf[i+3] <<8) | (bf[i+4] <<0);
	// *r1out = r1;
	// *respOut = rsp;

	//data is read just like "data"-data - with a data-token preceding it.
	prtData(bf, ioLen);
#endif

	crc = 0;
	//get CMD 6 data:
	readSdUData(port, &token, dataBf, &crc, 64);
	prtData(dataBf, 64);


/*
The 2GB card responds with power-limit = 0x64 = 100mA. I have no idea why that filed can hopld up to 0xFFFF = >65A! Maybe somebody thought they would be making an SD-Card welding-machine... @_@

*/



	sendSdCmd(port, 6, 0x00FFFFF1, 0, &r1, &rsp); //bit 31=0 = Mode0 = "Check"-mode; 23:00= 0xF,... means no change, Access(busSpeed)Mode = 1 check.  //R1
	printf("\n CMD 6 resp %02X %04X ", r1, rsp);
	crc = 0;
	readSdUData(port, &token, dataBf, &crc, 64); 	//get CMD 6 data
	prtData(dataBf, 64);


	sendSdCmd(port, 6, 0x00FFFFF2, 0, &r1, &rsp); //bit 31=0 = Mode0 = "Check"-mode; 23:00= 0xF,... means no change, Access(busSpeed)Mode = 1 check.  //R1
	printf("\n CMD 6 resp %02X %04X ", r1, rsp);
	crc = 0;
	readSdUData(port, &token, dataBf, &crc, 64); 	//get CMD 6 data
	prtData(dataBf, 64);

	sendSdCmd(port, 6, 0x00FFFFF3, 0, &r1, &rsp); //bit 31=0 = Mode0 = "Check"-mode; 23:00= 0xF,... means no change, Access(busSpeed)Mode = 1 check.  //R1
	printf("\n CMD 6 resp %02X %04X ", r1, rsp);
	crc = 0;
	readSdUData(port, &token, dataBf, &crc, 64); 	//get CMD 6 data
	prtData(dataBf, 64);

	sendSdCmd(port, 6, 0x00FFFFF4, 0, &r1, &rsp); //bit 31=0 = Mode0 = "Check"-mode; 23:00= 0xF,... means no change, Access(busSpeed)Mode = 1 check.  //R1
	printf("\n CMD 6 resp %02X %04X ", r1, rsp);
	crc = 0;
	readSdUData(port, &token, dataBf, &crc, 64); 	//get CMD 6 data
	prtData(dataBf, 64);

#if 0
	sendSdCmd(port, 6, 0x80FFFFF1, 0, &r1, &rsp); //bit 31=1 = Mode1 = "Set"-mode; 23:00= 0xF,... means no change, Access(busSpeed)Mode = 1 (25MHz / ???50MHz?) set.  //R1
	printf("\n CMD 6 resp %02X %04X ", r1, rsp);
	crc = 0;
	readSdUData(port, &token, dataBf, &crc, 64); 	//get CMD 6 data
	prtData(dataBf, 64);
#endif
#if 1
	sendSdCmd(port, 6, 0x80FFFFF2, 0, &r1, &rsp); //bit 31=1 = Mode1 = "Set"-mode; 23:00= 0xF,... means no change, Access(busSpeed)Mode = 1 (25MHz / ???50MHz?) set.  //R1
	printf("\n CMD 6 resp %02X %04X ", r1, rsp);
	crc = 0;
	readSdUData(port, &token, dataBf, &crc, 64); 	//get CMD 6 data
	prtData(dataBf, 64);

#endif

	sendSdCmd(port, 6, 0x00FFFFFF, 0, &r1, &rsp); //bit 31=0 = Mode0 = "Check"-mode; 23:00= 0xF,... means no change  //R1
	printf("\n CMD 6 resp %02X %04X ", r1, rsp);
	crc = 0;
	readSdUData(port, &token, dataBf, &crc, 64); 	//get CMD 6 data
	prtData(dataBf, 64);


	sendSdCmd(port, 6, 0x00FFFFF4, 0, &r1, &rsp); //bit 31=0 = Mode0 = "Check"-mode; 23:00= 0xF,... means no change, Access(busSpeed)Mode = 1 check.  //R1
	printf("\n CMD 6 resp %02X %04X ", r1, rsp);
	crc = 0;
	readSdUData(port, &token, dataBf, &crc, 64); 	//get CMD 6 data
	prtData(dataBf, 64);

	sendSdCmd(port, 6, 0x00FFFFF4, 0, &r1, &rsp); //bit 31=0 = Mode0 = "Check"-mode; 23:00= 0xF,... means no change, Access(busSpeed)Mode = 1 check.  //R1
	printf("\n CMD 6 resp %02X %04X ", r1, rsp);
	crc = 0;
	readSdUData(port, &token, dataBf, &crc, 64  | 1); 	//get CMD 6 data
	prtData(dataBf, 64);



#endif



	unknownCard:

	return;
}


int _start(int argc, char *argv[]) {

//	ppcModInit();

	printf(WELCOME_STR);

/*
	printf("reg library entries...\n");

	if (RegisterLibraryEntries(&_exp_sdCard) != 0) {
		dbg_printf("RegisterLibraryEntries failed\n");
		return MODULE_NO_RESIDENT_END;
	}

	sema.attr = 1;
	sema.option = 0;
	sema.initial = 1;
	sema.max = 1;
	sdCardSema = CreateSema(&sema);
*/


	testSio2SdCardIo();


	printf("Init done\n");
	return MODULE_RESIDENT_END; //MODULE_NO_RESIDENT_END;
}

