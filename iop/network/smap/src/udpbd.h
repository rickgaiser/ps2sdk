#ifndef UDPBD_H
#define UDPBD_H


#include "tamtypes.h"


#define UDPBD_PORT            0xBDBD //The port on which to listen for incoming data
#define UDPBD_HEADER_MAGIC    0xBDBDBDBD
#define UDPBD_CMD_INFO        0x00
#define UDPBD_CMD_READ        0x01
#define UDPBD_CMD_WRITE       0x02
#define UDPBD_MAX_DATA        1408 // 1408 bytes = max 11 x 128b blocks
#define UDPBD_MAX_SECTOR_READ 128

/* These automatically convert the address and port to network order.  */
#define IP_ADDR(a, b, c, d)	(((d & 0xff) << 24) | ((c & 0xff) << 16) | \
				((b & 0xff) << 8) | ((a & 0xff)))
#define IP_PORT(port)	(((port & 0xff00) >> 8) | ((port & 0xff) << 8))

typedef struct { u8 addr[4]; } ip_addr_t __attribute__((packed));

typedef struct {
	/* UDP BD header (16) */
	u32 magic;
	u8  cmd;
	u8  cmdid;
	u8  cmdpkt;
	u8  count;
	u32 par1;
	u32 par2;

	/* Data */
} udpbd_header_t __attribute__((packed));

typedef struct {
	/* Ethernet header (14) */
	u8	eth_addr_dst[6];
	u8	eth_addr_src[6];
	u16	eth_type;

	/* IP header (20) */
	u8	ip_hlen;
	u8	ip_tos;
	u16	ip_len;
	u16	ip_id;
	u8	ip_flags;
	u8	ip_frag_offset;
	u8	ip_ttl;
	u8	ip_proto;
	u16	ip_csum;
	ip_addr_t ip_addr_src;
	ip_addr_t ip_addr_dst;

	/* UDP header (8) */
	u16	udp_port_src;
	u16	udp_port_dst;
	u16	udp_len;
	u16	udp_csum;

    /* Align to 32 bits (2) */
    u16 ps2_align;

    /* UDP BD header (16) */
	udpbd_header_t bd;

	/* Data */
} udpbd_pkt_t __attribute__((packed));

int udpbd_init(void);
void udpbd_rx(u16 pointer);
void udpbd_test(void);

#endif
