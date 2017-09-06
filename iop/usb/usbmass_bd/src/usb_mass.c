/*
 * usb_driver.c - USB Mass Storage Driver
 * See usbmass-ufi10.pdf and usbmassbulk_10.pdf
 */

#include <errno.h>
#include <sysclib.h>
#include <thbase.h>
#include <thsemap.h>
#include <stdio.h>
#include <usbd.h>
#include <usbd_macro.h>

#include <usbhdfsd-common.h>
#include "scsi.h"

//#define DEBUG  //comment out this line when not debugging
#include "module_debug.h"


#define USB_SUBCLASS_MASS_RBC		0x01
#define USB_SUBCLASS_MASS_ATAPI		0x02
#define USB_SUBCLASS_MASS_QIC		0x03
#define USB_SUBCLASS_MASS_UFI		0x04
#define USB_SUBCLASS_MASS_SFF_8070I 	0x05
#define USB_SUBCLASS_MASS_SCSI		0x06

#define USB_PROTOCOL_MASS_CBI		0x00
#define USB_PROTOCOL_MASS_CBI_NO_CCI	0x01
#define USB_PROTOCOL_MASS_BULK_ONLY	0x50

#define USB_BLK_EP_IN		0
#define USB_BLK_EP_OUT		1

#define USB_XFER_MAX_RETRIES	8

#define CBW_TAG 0x43425355
#define CSW_TAG 0x53425355

typedef struct _mass_dev
{
	int controlEp;			//config endpoint id
	int bulkEpI;			//in endpoint id
	int bulkEpO;			//out endpoint id
	int devId;			//device id
	unsigned char configId;		//configuration id
	unsigned char status;
	unsigned char interfaceNumber;	//interface number
	unsigned char interfaceAlt;	//interface alternate setting
	int ioSema;
	struct scsi_interface scsi;
} mass_dev;

typedef struct _cbw_packet {
	unsigned int signature;
	unsigned int tag;
	unsigned int dataTransferLength;
	unsigned char flags;			//80->data in,  00->out
	unsigned char lun;
	unsigned char comLength;		//command data length
	unsigned char comData[16];		//command data
} cbw_packet;

typedef struct _csw_packet {
	unsigned int signature;
	unsigned int tag;
	unsigned int dataResidue;
	unsigned char status;
} csw_packet;

static sceUsbdLddOps driver;

typedef struct _usb_callback_data {
	int semh;
	int returnCode;
	int returnSize;
} usb_callback_data;


#define NUM_DEVICES 2
static mass_dev g_mass_device[NUM_DEVICES];
static int usb_mass_update_sema;


static void usb_mass_release(mass_dev *dev);

static void usb_callback(int resultCode, int bytes, void *arg)
{
	usb_callback_data* data = (usb_callback_data*) arg;
	data->returnCode = resultCode;
	data->returnSize = bytes;
	M_DEBUG("callback: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);
	SignalSema(data->semh);
}

static void usb_set_configuration(mass_dev* dev, int configNumber) {
	int ret;
	usb_callback_data cb_data;

	cb_data.semh = dev->ioSema;

	M_DEBUG("setting configuration controlEp=%i, confNum=%i \n", dev->controlEp, configNumber);
	ret = sceUsbdSetConfiguration(dev->controlEp, configNumber, usb_callback, (void*)&cb_data);

	if (ret == USB_RC_OK) {
		WaitSema(cb_data.semh);
		ret = cb_data.returnCode;
	}
	if (ret != USB_RC_OK) {
		M_DEBUG("sending set_configuration %d\n", ret);
	}
}

static void usb_set_interface(mass_dev* dev, int interface, int altSetting) {
	int ret;
	usb_callback_data cb_data;

	cb_data.semh = dev->ioSema;

	M_DEBUG("setting interface controlEp=%i, interface=%i altSetting=%i\n", dev->controlEp, interface, altSetting);
	ret = sceUsbdSetInterface(dev->controlEp, interface, altSetting, usb_callback, (void*)&cb_data);

	if (ret == USB_RC_OK) {
		WaitSema(cb_data.semh);
		ret = cb_data.returnCode;
	}
	if (ret != USB_RC_OK) {
		M_DEBUG("ERROR: sending set_interface %d\n", ret);
	}
}

static int usb_bulk_clear_halt(mass_dev* dev, int endpoint) {
	int ret;
	usb_callback_data cb_data;

	cb_data.semh = dev->ioSema;

	ret = sceUsbdClearEndpointFeature(
		dev->controlEp, 	//Config pipe
		0,			//HALT feature
		(endpoint==USB_BLK_EP_IN) ? dev->bulkEpI : dev->bulkEpO,
		usb_callback,
		(void*)&cb_data
		);

	if (ret == USB_RC_OK) {
		WaitSema(cb_data.semh);
		ret = cb_data.returnCode;
	}
	if (ret != USB_RC_OK) {
		M_DEBUG("ERROR: sending clear halt %d\n", ret);
	}

	return ret;
}

static void usb_bulk_reset(mass_dev* dev, int mode) {
	int ret;
	usb_callback_data cb_data;

	cb_data.semh = dev->ioSema;

	//Call Bulk only mass storage reset
	ret = sceUsbdControlTransfer(
		dev->controlEp, 		//default pipe
		0x21,			//bulk reset
		0xFF,
		0,
		dev->interfaceNumber, //interface number
		0,			//length
		NULL,			//data
		usb_callback,
		(void*) &cb_data
		);

	if (ret == USB_RC_OK) {
		WaitSema(cb_data.semh);
		ret = cb_data.returnCode;
	}
	if(ret == USB_RC_OK) {
		//clear bulk-in endpoint
		if (mode & 0x01)
			ret = usb_bulk_clear_halt(dev, USB_BLK_EP_IN);
	}
	if(ret == USB_RC_OK) {
		//clear bulk-out endpoint
		if (mode & 0x02)
			ret = usb_bulk_clear_halt(dev, USB_BLK_EP_OUT);
	}
	if(ret != USB_RC_OK){
		M_DEBUG("ERROR: sending reset %d to device %d.\n", ret, dev->devId);
		dev->status |= USBMASS_DEV_STAT_ERR;
	}
}

static int usb_bulk_status(mass_dev* dev, csw_packet* csw, unsigned int tag) {
	int ret;
	usb_callback_data cb_data;

	cb_data.semh = dev->ioSema;

	csw->signature = CSW_TAG;
	csw->tag = tag;
	csw->dataResidue = 0;
	csw->status = 0;

	ret = sceUsbdBulkTransfer(
		dev->bulkEpI,		//bulk input pipe
		csw,			//data ptr
		13,	//data length
		usb_callback,
		(void*)&cb_data
        );

	if (ret == USB_RC_OK) {
		WaitSema(cb_data.semh);
		ret = cb_data.returnCode;

#ifdef DEBUG
		if (cb_data.returnSize != 13)
			M_PRINTF("bulk csw.status returnSize: %i != 13\n", cb_data.returnSize);
		if (csw->dataResidue != 0)
			M_PRINTF("bulk csw.status residue: %i\n", csw->dataResidue);
		M_PRINTF("bulk csw result: %d, csw.status: %i\n", ret, csw->status);
#endif
	}

	return ret;
}

/* see flow chart in the usbmassbulk_10.pdf doc (page 15)

	Returned values:
		<0 Low-level USBD error.
		0 = Command completed successfully.
		1 = Command failed.
		2 = Phase error.
*/
static int usb_bulk_manage_status(mass_dev* dev, unsigned int tag) {
	int ret;
	csw_packet csw;

	//M_DEBUG("usb_bulk_manage_status 1 ...\n");
	ret = usb_bulk_status(dev, &csw, tag); /* Attempt to read CSW from bulk in endpoint */
	if (ret != USB_RC_OK) { /* STALL bulk in  -OR- Bulk error */
		usb_bulk_clear_halt(dev, USB_BLK_EP_IN); /* clear the stall condition for bulk in */

		M_DEBUG("usb_bulk_manage_status error %d ...\n", ret);
		ret = usb_bulk_status(dev, &csw, tag); /* Attempt to read CSW from bulk in endpoint */
	}

	/* CSW not valid or stalled or phase error */
	if (ret != USB_RC_OK || csw.signature != CSW_TAG || csw.tag != tag || csw.status == 2) {
		M_PRINTF("usb_bulk_manage_status call reset recovery ...\n");
		usb_bulk_reset(dev, 3);	/* Perform reset recovery */
	}

	return((ret == USB_RC_OK && csw.signature == CSW_TAG && csw.tag == tag) ? csw.status : -1);
}

static int usb_bulk_get_max_lun(struct scsi_interface* scsi) {
	mass_dev* dev = (mass_dev*)scsi->priv;
	int ret;
	usb_callback_data cb_data;
	char max_lun;

	cb_data.semh = dev->ioSema;

	//Call Bulk only mass storage reset
	ret = sceUsbdControlTransfer(
		dev->controlEp, 	//default pipe
		0xA1,
		0xFE,
		0,
		dev->interfaceNumber,	//interface number
		1,			//length
		&max_lun,		//data
		usb_callback,
		(void*) &cb_data
		);

	if (ret == USB_RC_OK) {
		WaitSema(cb_data.semh);
		ret = cb_data.returnCode;
	}
	if(ret == USB_RC_OK){
		ret = max_lun;
	} else {
		//Devices that do not support multiple LUNs may STALL this command.
		usb_bulk_clear_halt(dev, USB_BLK_EP_IN);
		usb_bulk_clear_halt(dev, USB_BLK_EP_OUT);

		ret = -ret;
	}

	return ret;
}

static int usb_bulk_command(mass_dev* dev, cbw_packet* packet) {
	int ret;
	usb_callback_data cb_data;

	if(dev->status & USBMASS_DEV_STAT_ERR) {
		M_PRINTF("Rejecting I/O to offline device %d.\n", dev->devId);
		return -1;
	}

	cb_data.semh = dev->ioSema;

	ret =  sceUsbdBulkTransfer(
		dev->bulkEpO,		//bulk output pipe
		packet,			//data ptr
		31,	//data length
		usb_callback,
		(void*)&cb_data
	);

	if (ret == USB_RC_OK) {
		WaitSema(cb_data.semh);
		ret = cb_data.returnCode;
	}
	if (ret != USB_RC_OK) {
		M_DEBUG("ERROR: sending bulk command %d. Calling reset recovery.\n", ret);
		usb_bulk_reset(dev, 3);
	}

	return ret;
}

#define MAX_SIZE (4*1024)
int usb_scsi_cmd(struct scsi_interface* scsi, const unsigned char *cmd, unsigned int cmd_len, unsigned char *data, unsigned int data_len, unsigned int data_wr)
{
	mass_dev* dev = (mass_dev*)scsi->priv;
	int rcode = USB_RC_OK, result = -EIO, retries;
	static unsigned int tag = 0;
	static cbw_packet cbw;

	tag++;
	cbw.signature = CBW_TAG;
	cbw.tag = tag;
	cbw.dataTransferLength = data_len;
	cbw.flags = 0x80;
	cbw.lun = 0;
	cbw.comLength = cmd_len;

	memcpy(cbw.comData, cmd, cmd_len);

	for (retries = USB_XFER_MAX_RETRIES; retries > 0; retries--) {
		// Send command
		if (usb_bulk_command(dev, &cbw) == USB_RC_OK){
			// Send/Receive data
			while (data_len > 0) {
				unsigned int tr_len = (data_len < MAX_SIZE) ? data_len : MAX_SIZE;
				// NOTE: return value ignored
				//usb_bulk_transfer(dev, data_wr ? USB_BLK_EP_OUT : USB_BLK_EP_IN, data, tr_len);
				sceUsbdBulkTransfer(data_wr ? dev->bulkEpO : dev->bulkEpI, data, tr_len, NULL, NULL);
				data_len -= tr_len;
				data += tr_len;
			}

			// Check status
			result = usb_bulk_manage_status(dev, tag);

			if(rcode == USB_RC_OK && result == 0)
				return 0;
		}
	}

	return result;
}

static mass_dev* usb_mass_findDevice(int devId, int create)
{
	mass_dev* dev = NULL;
	int i;
	M_DEBUG("usb_mass_findDevice devId %i\n", devId);
	for (i = 0; i < NUM_DEVICES; ++i) {
		if (g_mass_device[i].devId == devId) {
			M_DEBUG("usb_mass_findDevice exists %i\n", i);
			dev = &g_mass_device[i];
			break;
		}
		else if (create && dev == NULL && g_mass_device[i].devId == -1) {
			dev = &g_mass_device[i];
			break;
		}
	}
	return dev;
}

/* test that endpoint is bulk endpoint and if so, update device info */
static void usb_bulk_probeEndpoint(int devId, mass_dev* dev, UsbEndpointDescriptor* endpoint) {
	if (endpoint->bmAttributes == USB_ENDPOINT_XFER_BULK) {
		/* out transfer */
		if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && dev->bulkEpO < 0) {
			dev->bulkEpO = sceUsbdOpenPipeAligned(devId, endpoint);
			M_DEBUG("register Output endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpO, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB<<8 | endpoint->wMaxPacketSizeLB);
		} else
		/* in transfer */
		if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && dev->bulkEpI < 0) {
			dev->bulkEpI = sceUsbdOpenPipeAligned(devId, endpoint);
			M_DEBUG("register Input endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpI, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB<<8 | endpoint->wMaxPacketSizeLB);
		}
	}
}

static int usb_mass_probe(int devId)
{
	UsbDeviceDescriptor *device = NULL;
	UsbConfigDescriptor *config = NULL;
	UsbInterfaceDescriptor *intf = NULL;

	M_DEBUG("probe: devId=%i\n", devId);

	mass_dev* mass_device = usb_mass_findDevice(devId, 0);

	/* only one device supported */
	if ((mass_device != NULL) && (mass_device->status & USBMASS_DEV_STAT_CONN)) {
		M_PRINTF("ERROR: Only one mass storage device allowed!\n");
		return 0;
	}

	/* get device descriptor */
	device = (UsbDeviceDescriptor*)sceUsbdScanStaticDescriptor(devId, NULL, USB_DT_DEVICE);
	if (device == NULL)  {
		M_DEBUG("ERROR: Couldn't get device descriptor\n");
		return 0;
	}

	/* Check if the device has at least one configuration */
	if (device->bNumConfigurations < 1) { return 0; }

	/* read configuration */
	config = (UsbConfigDescriptor*)sceUsbdScanStaticDescriptor(devId, device, USB_DT_CONFIG);
	if (config == NULL) {
	    M_DEBUG("ERROR: Couldn't get configuration descriptor\n");
	    return 0;
	}
	/* check that at least one interface exists */
	M_DEBUG("bNumInterfaces %d\n", config->bNumInterfaces);
	if ((config->bNumInterfaces < 1) ||
		(config->wTotalLength < (sizeof(UsbConfigDescriptor) + sizeof(UsbInterfaceDescriptor)))) {
			M_DEBUG("ERROR: No interfaces available\n");
			return 0;
	}
        /* get interface */
	intf = (UsbInterfaceDescriptor *) ((char *) config + config->bLength); /* Get first interface */
	M_DEBUG("bInterfaceClass %X bInterfaceSubClass %X bInterfaceProtocol %X\n",
        intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol);

	if ((intf->bInterfaceClass		!= USB_CLASS_MASS_STORAGE) ||
		(intf->bInterfaceSubClass	!= USB_SUBCLASS_MASS_SCSI  &&
		 intf->bInterfaceSubClass	!= USB_SUBCLASS_MASS_SFF_8070I) ||
		(intf->bInterfaceProtocol	!= USB_PROTOCOL_MASS_BULK_ONLY) ||
		(intf->bNumEndpoints < 2))    { //one bulk endpoint is not enough because
			return 0;		//we send the CBW to te bulk out endpoint
	}

	return 1;
}

static int usb_mass_connect(int devId)
{
	int i;
	int epCount;
	UsbDeviceDescriptor *device;
	UsbConfigDescriptor *config;
	UsbInterfaceDescriptor *interface;
	UsbEndpointDescriptor *endpoint;
	iop_sema_t SemaData;
	mass_dev* dev;

	M_PRINTF("connect: devId=%i\n", devId);
	dev = usb_mass_findDevice(devId, 1);

	if (dev == NULL) {
		M_PRINTF("ERROR: Unable to allocate space!\n");
		return 1;
	}

	/* only one mass device allowed */
	if (dev->devId != -1) {
		M_PRINTF("ERROR: Only one mass storage device allowed!\n");
		return 1;
	}

	dev->status = 0;
	dev->bulkEpI = -1;
	dev->bulkEpO = -1;

	/* open the config endpoint */
	dev->controlEp = sceUsbdOpenPipe(devId, NULL);

	device = (UsbDeviceDescriptor*)sceUsbdScanStaticDescriptor(devId, NULL, USB_DT_DEVICE);

	config = (UsbConfigDescriptor*)sceUsbdScanStaticDescriptor(devId, device, USB_DT_CONFIG);

	interface = (UsbInterfaceDescriptor *) ((char *) config + config->bLength); /* Get first interface */

	// store interface numbers
	dev->interfaceNumber = interface->bInterfaceNumber;
	dev->interfaceAlt    = interface->bAlternateSetting;

	epCount = interface->bNumEndpoints;
	endpoint = (UsbEndpointDescriptor*)sceUsbdScanStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);
	usb_bulk_probeEndpoint(devId, dev, endpoint);

	for (i = 1; i < epCount; i++) {
		endpoint = (UsbEndpointDescriptor*) ((char *) endpoint + endpoint->bLength);
		usb_bulk_probeEndpoint(devId, dev, endpoint);
	}

	// Bail out if we do NOT have enough bulk endpoints.
	if (dev->bulkEpI < 0 || dev->bulkEpO < 0) {
		usb_mass_release(dev);
		M_PRINTF("ERROR: connect failed: not enough bulk endpoints!\n");
		return -1;
	}

	SemaData.initial = 0;
	SemaData.max = 1;
	SemaData.option = 0;
	SemaData.attr = 0;
	if((dev->ioSema = CreateSema(&SemaData)) <0){
		M_PRINTF("ERROR: Failed to allocate I/O semaphore\n");
		return -1;
	}

	/*store current configuration id - can't call set_configuration here */
	dev->devId = devId;
	dev->configId = config->bConfigurationValue;
	dev->status = USBMASS_DEV_STAT_CONN;
	M_DEBUG("connect ok: epI=%i, epO=%i\n", dev->bulkEpI, dev->bulkEpO);

	SignalSema(usb_mass_update_sema);

	return 0;
}

static void usb_mass_release(mass_dev *dev)
{
	if (dev->bulkEpI >= 0) {
		sceUsbdClosePipe(dev->bulkEpI);
	}

	if (dev->bulkEpO >= 0) {
		sceUsbdClosePipe(dev->bulkEpO);
	}

	dev->bulkEpI = -1;
	dev->bulkEpO = -1;
	dev->controlEp = -1;
	dev->status = 0;
}

static int usb_mass_disconnect(int devId) {
	mass_dev* dev;
	dev = usb_mass_findDevice(devId, 0);

	M_PRINTF("disconnect: devId=%i\n", devId);

	if (dev == NULL) {
		M_PRINTF("ERROR: disconnect: no device storage!\n");
		return 0;
	}

	if (dev->status & USBMASS_DEV_STAT_CONN) {
		usb_mass_release(dev);
		dev->devId = -1;

		DeleteSema(dev->ioSema);

		// Should this move to the thread
		// just like the scsi_connect?
		scsi_disconnect(&dev->scsi);
	}

	return 0;
}

static void usb_mass_update(void *arg)
{
	int i;

	M_PRINTF("update thread running\n");

	while (1) {
		// Wait for event from USBD thread
		WaitSema(usb_mass_update_sema);

		// Connect new devices
		for (i = 0; i < NUM_DEVICES; ++i) {
			mass_dev* dev = &g_mass_device[i];
			if ((dev->status & USBMASS_DEV_STAT_CONN) && !(dev->status & USBMASS_DEV_STAT_CONF)) {
				usb_set_configuration(dev, dev->configId);
				usb_set_interface(dev, dev->interfaceNumber, dev->interfaceAlt);
				dev->status |= USBMASS_DEV_STAT_CONF;
				scsi_connect(&dev->scsi);
			}
		}
	}
}

int usb_mass_init(void)
{
	iop_thread_t thread;
	iop_sema_t sema;
	int ret;
	int i;

	M_DEBUG("usb_mass_init\n");

	for (i = 0; i < NUM_DEVICES; ++i) {
		g_mass_device[i].status = 0;
		g_mass_device[i].devId = -1;

		g_mass_device[i].scsi.priv = &g_mass_device[i];
		g_mass_device[i].scsi.get_max_lun = usb_bulk_get_max_lun;
		g_mass_device[i].scsi.scsi_cmd = usb_scsi_cmd;
	}

	driver.next       = NULL;
	driver.prev       = NULL;
	driver.name       = "mass-stor";
	driver.probe      = usb_mass_probe;
	driver.connect    = usb_mass_connect;
	driver.disconnect = usb_mass_disconnect;

	ret = sceUsbdRegisterLdd(&driver);
	M_DEBUG("registerDriver=%i\n", ret);
	if (ret < 0) {
		M_PRINTF("ERROR: register driver failed! ret=%d\n", ret);
		return -1;
	}

	sema.attr = 0;
	sema.option = 0;
	sema.initial = 1; // Run the update thread, just in case
	sema.max = 1;
	usb_mass_update_sema = CreateSema(&sema);

	thread.attr = TH_C;
	thread.option = 0;
	thread.thread = usb_mass_update;
	thread.stacksize = 0x1000; // 4K stack / memory
	thread.priority = 0x10;

	ret = CreateThread(&thread);
	if (ret < 0) {
		M_PRINTF("ERROR: CreateThread failed! ret=%d\n", ret);
		return -1;
	}

	ret = StartThread(ret, 0);
	if (ret < 0) {
		M_PRINTF("ERROR: StartThread failed! ret=%d\n", ret);
		DeleteThread(ret);
		return -1;
	}

	return 0;
}
