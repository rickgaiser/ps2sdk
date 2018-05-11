#include "lwip/udp.h"

unsigned int udp_packet_count = 0;
unsigned int udp_data_size = 0;

static void udp_iperf_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    udp_packet_count += 1;
    udp_data_size += p->len;
	pbuf_free(p);
}

void udp_iperf()
{
    struct udp_pcb* pcb = udp_new();

    udp_bind(pcb, IP4_ADDR_ANY, 5001);
    udp_recv(pcb, udp_iperf_recv, NULL);
}
