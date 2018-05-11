#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "tamtypes.h"

typedef u8  uint8_t;
typedef u16 uint16_t;
typedef u32 uint32_t;

#define PORT    0xBDBD  //The port on which to listen for incoming data

#define BDFS_HEADER_MAGIC 0xBDBDBDBD
#define BDFS_CMD_INFO     0x00
#define BDFS_CMD_READ     0x01
#define BDFS_CMD_WRITE    0x02
struct SBDFS_Header {       //   18 bytes
    uint16_t ps2_align;     //    2 bytes

	uint32_t bdmagic;       //    4 bytes
	uint32_t cmd:8;         //    4 bytes
	uint32_t cmdid:8;       //             increment with every request
	uint32_t count:16;      //             0=request, 1 or more are response packets

	uint32_t par1;          //    4 bytes  sectorSize / startSector
	uint32_t par2;          //    4 bytes  sectorCount
} __attribute__((__packed__));

#define BDFS_MAX_DATA 1408  // 1408 bytes = max 11 x 128b blocks
struct SDBFS_Data {
    struct SBDFS_Header hdr;
    uint8_t data[BDFS_MAX_DATA];
} __attribute__((__packed__));

extern unsigned int udp_packet_count;
extern unsigned int udp_data_size;

static struct udp_pcb* pcb;

static void udpbd_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    udp_packet_count += 1;
    udp_data_size += p->len;
	pbuf_free(p);
}

void udpbd_init()
{
    ip_addr_t ipaddr;
    IP4_ADDR(&ipaddr, 192, 168, 1, 198);

    pcb = udp_new();

    // Connect to server
    udp_connect(pcb, &ipaddr, PORT);
    udp_recv(pcb, udpbd_recv, NULL);
}

void udpbd_read()
{
    struct pbuf * p;
    struct SBDFS_Header hdr;

    // Create udp read request
    hdr.bdmagic = BDFS_HEADER_MAGIC;
    hdr.cmd = BDFS_CMD_READ;
    hdr.cmdid = 1;
    hdr.count = 0; // request
    hdr.par1 = 0; // sector offset
    hdr.par2 = 5*1024*2; // sector count, 32*512 = 16KiB

    p = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct SBDFS_Header), PBUF_REF);
    p->payload = &hdr;
    udp_send(pcb, p);
    pbuf_free(p);
}
