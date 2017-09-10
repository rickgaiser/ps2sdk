#include <errno.h>
#include <sysclib.h>
#include <thsemap.h>
#include <stdio.h>

#include <bdm.h>
#include "scsi.h"

//#define DEBUG  //comment out this line when not debugging
#include "module_debug.h"


#define getBI32(__buf) ((((u8 *) (__buf))[3] << 0) | (((u8 *) (__buf))[2] << 8) | (((u8 *) (__buf))[1] << 16) | (((u8 *) (__buf))[0] << 24))


typedef struct _inquiry_data {
	u8 peripheral_device_type;	// 00h - Direct access (Floppy), 1Fh none (no FDD connected)
	u8 removable_media;		// 80h - removeable
	u8 iso_ecma_ansi;
	u8 response_data_format;
	u8 additional_length;
	u8 res[3];
	u8 vendor[8];
	u8 product[16];
	u8 revision[4];
} inquiry_data;

typedef struct _sense_data {
	u8 error_code;
	u8 res1;
	u8 sense_key;
	u8 information[4];
	u8 add_sense_len;
	u8 res3[4];
	u8 add_sense_code;
	u8 add_sense_qual;
	u8 res4[4];
} sense_data;

typedef struct _read_capacity_data {
	u8 last_lba[4];
	u8 block_length[4];
} read_capacity_data;


#define NUM_DEVICES 2
static struct block_device g_scsi_bd[NUM_DEVICES];


//
// Private Low level SCSI commands
//
static int scsi_cmd(struct block_device* bd, unsigned char cmd, void *buffer, int buf_size, int cmd_size)
{
	unsigned char comData[12] = {0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	struct scsi_interface* scsi = (struct scsi_interface*)bd->priv;

	comData[0] = cmd;
	comData[4] = cmd_size;

	return scsi->queue_cmd(scsi, comData, 12, buffer, buf_size, 0, NULL, NULL);
}

static inline int scsi_cmd_test_unit_ready(struct block_device* bd)
{
	M_DEBUG("scsi_cmd_test_unit_ready\n");

	return scsi_cmd(bd, 0x00, NULL, 0, 0);
}

static inline int scsi_cmd_request_sense(struct block_device* bd, void *buffer, int size)
{
	M_DEBUG("scsi_cmd_request_sense\n");

	return scsi_cmd(bd, 0x03, buffer, size, size);
}

static inline int scsi_cmd_inquiry(struct block_device* bd, void *buffer, int size)
{
	M_DEBUG("scsi_cmd_inquiry\n");

	return scsi_cmd(bd, 0x12, buffer, size, size);
}

static inline int scsi_cmd_start_stop_unit(struct block_device* bd)
{
	M_DEBUG("scsi_cmd_start_stop_unit\n");

	return scsi_cmd(bd, 0x1b, NULL, 0, 1);
}

static inline int scsi_cmd_read_capacity(struct block_device* bd, void *buffer, int size)
{
	M_DEBUG("scsi_cmd_read_capacity\n");

	return scsi_cmd(bd, 0x25, buffer, size, 0);
}

static int scsi_cmd_rw_sector(struct block_device* bd, unsigned int lba, const void* buffer, unsigned short int sectorCount, unsigned int write, scsi_cb cb, void* cb_arg)
{
	unsigned char comData[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	struct scsi_interface* scsi = (struct scsi_interface*)bd->priv;

	M_DEBUG("scsi_cmd_rw_sector - 0x%08x %p 0x%04x\n", lba, buffer, sectorCount);

	comData[0] = write ? 0x2a : 0x28;
	comData[2] = (lba & 0xFF000000) >> 24;	//lba 1 (MSB)
	comData[3] = (lba & 0xFF0000) >> 16;	//lba 2
	comData[4] = (lba & 0xFF00) >> 8;		//lba 3
	comData[5] = (lba & 0xFF);			//lba 4 (LSB)
	comData[7] = (sectorCount & 0xFF00) >> 8;	//Transfer length MSB
	comData[8] = (sectorCount & 0xFF);		//Transfer length LSB
	return scsi->queue_cmd(scsi, comData, 12, (void *)buffer, bd->sectorSize * sectorCount, write, cb, cb_arg);
}

//
// Private
//
static int scsi_warmup(struct block_device* bd) {
	struct scsi_interface* scsi = (struct scsi_interface*)bd->priv;
	inquiry_data id;
	sense_data sd;
	read_capacity_data rcd;
	int stat;

	M_DEBUG("scsi_warmup\n");

	stat = scsi->get_max_lun(scsi);
	M_DEBUG("scsi->get_max_lun %d\n", stat);

	memset(&id, 0, sizeof(inquiry_data));
	if ((stat = scsi_cmd_inquiry(bd, &id, sizeof(inquiry_data))) < 0) {
		M_PRINTF("ERROR: scsi_cmd_inquiry %d\n", stat);
		return -1;
	}

	M_PRINTF("Vendor: %.8s\n", id.vendor);
	M_PRINTF("Product: %.16s\n", id.product);
	M_PRINTF("Revision: %.4s\n", id.revision);

	while((stat = scsi_cmd_test_unit_ready(bd)) != 0)
	{
		M_PRINTF("ERROR: scsi_cmd_test_unit_ready %d\n", stat);

		stat = scsi_cmd_request_sense(bd, &sd, sizeof(sense_data));
		if (stat != 0)
			M_PRINTF("ERROR: scsi_cmd_request_sense %d\n", stat);

		if ((sd.error_code == 0x70) && (sd.sense_key != 0x00))
		{
			M_PRINTF("Sense Data key: %02X code: %02X qual: %02X\n", sd.sense_key, sd.add_sense_code, sd.add_sense_qual);

			if ((sd.sense_key == 0x02) && (sd.add_sense_code == 0x04) && (sd.add_sense_qual == 0x02))
			{
				M_PRINTF("ERROR: Additional initalization is required for this device!\n");
				if ((stat = scsi_cmd_start_stop_unit(bd)) != 0) {
					M_PRINTF("ERROR: scsi_cmd_start_stop_unit %d\n", stat);
					return -1;
				}
			}
		}
	}

	if ((stat = scsi_cmd_read_capacity(bd, &rcd, sizeof(read_capacity_data))) != 0) {
		M_PRINTF("ERROR: scsi_cmd_read_capacity %d\n", stat);
		return -1;
	}

	bd->sectorSize  = getBI32(&rcd.block_length);
	bd->sectorCount = getBI32(&rcd.last_lba);
	M_PRINTF("%u %u-byte logical blocks: (%uMB / %uMiB)\n", bd->sectorCount, bd->sectorSize, bd->sectorCount / ((1000*1000)/bd->sectorSize), bd->sectorCount / ((1024*1024)/bd->sectorSize));

	return 0;
}

//
// Block device interface
//
#define SCSI_READ_AHEAD
#ifdef SCSI_READ_AHEAD
int io_sem;
void scsi_read_async_cb(void* arg)
{
	SignalSema(io_sem);
}

static u32 last_sector = 0xffffffff;
static u16 last_count = 0;
static char ra_buffer[16*1024];
static u32 ra_sector = 0xffffffff;
static u16 ra_count = 0;
static u16 ra_active = 0;
#endif
static int scsi_read(struct block_device* bd, u32 sector, void* buffer, u16 count)
{
	M_DEBUG("scsi_read: sector=%d, count=%d\n", sector, count);

#ifdef SCSI_READ_AHEAD
	// Wait for any read-ahead actions to finish
	if (ra_active == 1) {
		WaitSema(io_sem);
		ra_active = 0;
	}

	if ((ra_sector == sector) && (ra_count >= count)) {
		//printf("using read-ahead sector=%d, count=%d\n", sector, count);
		// Read data from read-ahead buffer
		memcpy(buffer, ra_buffer, bd->sectorSize * count);
	}
	else {
		//printf("using disk sector=%d, count=%d\n", sector, count);
		// Read data from disk
		if(scsi_cmd_rw_sector(bd, sector, buffer, count, 0, NULL, NULL) < 0)
			return -EIO;
	}

	// Sequential read detection -> do read-ahead
	if ((last_sector + last_count) == sector) {
		if (count <= (16*2)) {
			//printf("read-ahead: sector=%d, count=%d\n", sector+count, count);
			if(scsi_cmd_rw_sector(bd, sector+count, ra_buffer, count, 0, scsi_read_async_cb, NULL) == 0) {
				ra_active = 1;
				ra_sector = sector+count;
				ra_count = count;
			}
		}
	}

	last_sector = sector;
	last_count = count;
#else
	if(scsi_cmd_rw_sector(bd, sector, buffer, count, 0, NULL, NULL) < 0)
		return -EIO;
#endif

	return count;
}

static int scsi_write(struct block_device* bd, u32 sector, const void* buffer, u16 count)
{
	M_DEBUG("scsi_write: sector=%d, count=%d\n", sector, count);

	if(scsi_cmd_rw_sector(bd, sector, buffer, count, 1, NULL, NULL) < 0)
		return -EIO;

	return count;
}

static void scsi_flush(struct block_device* bd)
{
    // Dummy function
}

//
// Public functions
//
void scsi_connect(struct scsi_interface* scsi)
{
	int i;

	M_DEBUG("scsi_connect\n");

	for (i = 0; i < NUM_DEVICES; ++i) {
		if (g_scsi_bd[i].priv == NULL) {
			struct block_device* bd = &g_scsi_bd[i];
			bd->priv = scsi;
			scsi_warmup(bd);
			bdm_connect_bd(bd);
			break;
		}
	}
}

void scsi_disconnect(struct scsi_interface* scsi)
{
	int i;

	M_DEBUG("scsi_disconnect\n");

	for (i = 0; i < NUM_DEVICES; ++i) {
		if (g_scsi_bd[i].priv == scsi) {
			struct block_device* bd = &g_scsi_bd[i];
			bdm_disconnect_bd(bd);
			bd->priv = NULL;
			break;
		}
	}
}

int scsi_init(void)
{
#ifdef SCSI_READ_AHEAD
	iop_sema_t sema;
#endif
	int i;

	M_DEBUG("scsi_init\n");

#ifdef SCSI_READ_AHEAD
	sema.attr = 0;
	sema.option = 0;
	sema.initial = 0;
	sema.max = 1;
	io_sem = CreateSema(&sema);
#endif

	for (i = 0; i < NUM_DEVICES; ++i) {
		g_scsi_bd[i].name = "usb";
		g_scsi_bd[i].devNr = i;
		g_scsi_bd[i].parNr = 0;

		g_scsi_bd[i].priv  = NULL;
		g_scsi_bd[i].read  = scsi_read;
		g_scsi_bd[i].write = scsi_write;
		g_scsi_bd[i].flush = scsi_flush;
	}

	return 0;
}
