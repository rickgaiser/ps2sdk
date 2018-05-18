/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/

#include <stdio.h>
#include <kernel.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <debug.h>
#include <netman.h>
#include <ps2ip.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <sbv_patches.h>

extern unsigned char EEDEBUG_irx[];
extern unsigned int size_EEDEBUG_irx;

extern unsigned char DEV9_irx[];
extern unsigned int size_DEV9_irx;

extern unsigned char SMAP_irx[];
extern unsigned int size_SMAP_irx;

extern unsigned char NETMAN_irx[];
extern unsigned int size_NETMAN_irx;

static int ethApplyNetIFConfig(int mode)
{
	int result;
	//By default, auto-negotiation is used.
	static int CurrentMode = NETMAN_NETIF_ETH_LINK_MODE_AUTO;

	if(CurrentMode != mode)
	{	//Change the setting, only if different.
		if((result = NetManSetLinkMode(mode)) == 0)
			CurrentMode = mode;
	}else
		result = 0;

	return result;
}

static void EthStatusCheckCb(s32 alarm_id, u16 time, void *common)
{
	iWakeupThread(*(int*)common);
}

// FIXME: this does not seem to work properly... time seems a bit random
static void msleep(u16 time)
{
	int ThreadID = GetThreadId();

	SetAlarm(time * 16, &EthStatusCheckCb, &ThreadID);
	SleepThread();
}

static int WaitValidNetState(int (*checkingFunction)(void))
{
	int retry_cycles;

	// Wait for a valid network status;
	for(retry_cycles = 0; checkingFunction() == 0; retry_cycles++)
	{
		msleep(1000);

		if(retry_cycles >= 10)	//10s = 10*1000ms
			return -1;
	}

	return 0;
}

static int ethGetNetIFLinkStatus(void)
{
	return(NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, NULL, 0, NULL, 0) == NETMAN_NETIF_ETH_LINK_STATE_UP);
}

static int ethWaitValidNetIFLinkState(void)
{
	return WaitValidNetState(&ethGetNetIFLinkStatus);
}

static int ethGetDHCPStatus(void)
{
	t_ip_info ip_info;
	int result;

	if ((result = ps2ip_getconfig("sm0", &ip_info)) >= 0)
	{	//Check for a successful state if DHCP is enabled.
		if (ip_info.dhcp_enabled)
			result = (ip_info.dhcp_status == DHCP_STATE_BOUND || (ip_info.dhcp_status == DHCP_STATE_OFF));
		else
			result = -1;
	}

	return result;
}

static int ethWaitValidDHCPState(void)
{
	return WaitValidNetState(&ethGetDHCPStatus);
}

static int ethApplyIPConfig(int use_dhcp, const struct ip4_addr *ip, const struct ip4_addr *netmask, const struct ip4_addr *gateway, const struct ip4_addr *dns)
{
	t_ip_info ip_info;
	struct ip4_addr dns_curr;
	int result;

	//SMAP is registered as the "sm0" device to the TCP/IP stack.
	if ((result = ps2ip_getconfig("sm0", &ip_info)) >= 0)
	{
		//Obtain the current DNS server settings.
		dns_curr = dns_getserver(0);

		//Check if it's the same. Otherwise, apply the new configuration.
		if ((use_dhcp != ip_info.dhcp_enabled)
		    ||	(!use_dhcp &&
			 (!ip_addr_cmp(ip, (struct ip4_addr *)&ip_info.ipaddr) ||
			 !ip_addr_cmp(netmask, (struct ip4_addr *)&ip_info.netmask) ||
			 !ip_addr_cmp(gateway, (struct ip4_addr *)&ip_info.gw) ||
			 !ip_addr_cmp(dns, &dns_curr))))
		{
			if (use_dhcp)
			{
				ip_info.dhcp_enabled = 1;
			}
			else
			{	//Copy over new settings if DHCP is not used.
				ip_addr_set((struct ip4_addr *)&ip_info.ipaddr, ip);
				ip_addr_set((struct ip4_addr *)&ip_info.netmask, netmask);
				ip_addr_set((struct ip4_addr *)&ip_info.gw, gateway);

				ip_info.dhcp_enabled = 0;
			}

			//Update settings.
			dns_setserver(0, dns);
			result = ps2ip_setconfig(&ip_info);
		}
		else
			result = 0;
	}

	return result;
}

static void ethPrintIPConfig(void)
{
	t_ip_info ip_info;
	struct ip4_addr dns_curr;
	u8 ip_address[4], netmask[4], gateway[4], dns[4];

	//SMAP is registered as the "sm0" device to the TCP/IP stack.
	if (ps2ip_getconfig("sm0", &ip_info) >= 0)
	{
		//Obtain the current DNS server settings.
		dns_curr = dns_getserver(0);

		ip_address[0] = ip4_addr1((struct ip4_addr *)&ip_info.ipaddr);
		ip_address[1] = ip4_addr2((struct ip4_addr *)&ip_info.ipaddr);
		ip_address[2] = ip4_addr3((struct ip4_addr *)&ip_info.ipaddr);
		ip_address[3] = ip4_addr4((struct ip4_addr *)&ip_info.ipaddr);

		netmask[0] = ip4_addr1((struct ip4_addr *)&ip_info.netmask);
		netmask[1] = ip4_addr2((struct ip4_addr *)&ip_info.netmask);
		netmask[2] = ip4_addr3((struct ip4_addr *)&ip_info.netmask);
		netmask[3] = ip4_addr4((struct ip4_addr *)&ip_info.netmask);

		gateway[0] = ip4_addr1((struct ip4_addr *)&ip_info.gw);
		gateway[1] = ip4_addr2((struct ip4_addr *)&ip_info.gw);
		gateway[2] = ip4_addr3((struct ip4_addr *)&ip_info.gw);
		gateway[3] = ip4_addr4((struct ip4_addr *)&ip_info.gw);

		dns[0] = ip4_addr1(&dns_curr);
		dns[1] = ip4_addr2(&dns_curr);
		dns[2] = ip4_addr3(&dns_curr);
		dns[3] = ip4_addr4(&dns_curr);

		scr_printf(	"IP:\t%d.%d.%d.%d\n"
				"NM:\t%d.%d.%d.%d\n"
				"GW:\t%d.%d.%d.%d\n"
				"DNS:\t%d.%d.%d.%d\n",
					ip_address[0], ip_address[1], ip_address[2], ip_address[3],
					netmask[0], netmask[1], netmask[2], netmask[3],
					gateway[0], gateway[1], gateway[2], gateway[3],
					dns[0], dns[1], dns[2], dns[3]);
	}
	else
	{
		scr_printf("Unable to read IP address.\n");
	}
}

static void ethPrintLinkStatus(void)
{
	int mode;

	//SMAP is registered as the "sm0" device to the TCP/IP stack.
	scr_printf("Link:\t");
	if (NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, NULL, 0, NULL, 0) == NETMAN_NETIF_ETH_LINK_STATE_UP)
		scr_printf("Up\n");
	else
		scr_printf("Down\n");

	scr_printf("Mode:\t");
	mode = NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_LINK_MODE, NULL, 0, NULL, 0);

	//NETMAN_NETIF_ETH_LINK_MODE_PAUSE is a flag, so file it off first.
	switch((mode & ~NETMAN_NETIF_ETH_LINK_MODE_PAUSE))
	{
		case NETMAN_NETIF_ETH_LINK_MODE_10M_HDX:
			scr_printf("10M HDX");
			break;
		case NETMAN_NETIF_ETH_LINK_MODE_10M_FDX:
			scr_printf("10M FDX");
			break;
		case NETMAN_NETIF_ETH_LINK_MODE_100M_HDX:
			scr_printf("100M HDX");
			break;
		case NETMAN_NETIF_ETH_LINK_MODE_100M_FDX:
			scr_printf("100M FDX");
			break;
		default:
			scr_printf("Unknown");
	}
	if(mode & NETMAN_NETIF_ETH_LINK_MODE_PAUSE)
		scr_printf(" with ");
	else
		scr_printf(" without ");
	scr_printf("Flow Control\n");
}

extern unsigned int udp_packet_count;
extern unsigned int udp_data_size;
static void ethPrintStats(void)
{
	int i;

	scr_printf("Stats: ");
	i = NetManIoctl(NETMAN_NETIF_IOCTL_GET_TX_DROPPED_COUNT,     NULL, 0, NULL, 0);	scr_printf("%d ", i);
	i = NetManIoctl(NETMAN_NETIF_IOCTL_GET_RX_DROPPED_COUNT,     NULL, 0, NULL, 0);	scr_printf("%d ", i);
	i = NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_RX_EOVERRUN_CNT,  NULL, 0, NULL, 0);	scr_printf("%d ", i);
	i = NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_RX_EBADLEN_CNT,   NULL, 0, NULL, 0);	scr_printf("%d ", i);
	i = NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_RX_EBADFCS_CNT,   NULL, 0, NULL, 0);	scr_printf("%d ", i);
	i = NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_RX_EBADALIGN_CNT, NULL, 0, NULL, 0);	scr_printf("%d ", i);
	i = NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_TX_ELOSSCR_CNT,   NULL, 0, NULL, 0);	scr_printf("%d ", i);
	i = NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_TX_EEDEFER_CNT,   NULL, 0, NULL, 0);	scr_printf("%d ", i);
	i = NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_TX_ECOLL_CNT,     NULL, 0, NULL, 0);	scr_printf("%d ", i);
	i = NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_TX_EUNDERRUN_CNT, NULL, 0, NULL, 0);	scr_printf("%d ", i);
	scr_printf("pc=%d, len=%d", udp_packet_count, udp_data_size);
	scr_printf("\n");
}

extern void* lwiperf_start_tcp_server_default(void* report_fn, void* report_arg);
extern void udp_iperf();
extern void udpbd_init();
extern void udpbd_read();

//#define EEDEBUG
#ifdef EEDEBUG
typedef struct {
       struct t_SifCmdHeader sifcmd;
       char text[80];
} iop_text_data_t;
static void iopdebug_printk(void *packet, void *arg)
{
    iop_text_data_t *data = (iop_text_data_t *)packet;
	scr_printf("%s", data->text);
}
#endif
int main(int argc, char *argv[])
{
	struct ip4_addr IP, NM, GW, DNS;
	int EthernetLinkMode;

	//Reboot IOP
	SifInitRpc(0);
	while(!SifIopReset("", 0)){};
	while(!SifIopSync()){};

	//Initialize SIF services
	SifInitRpc(0);
	SifLoadFileInit();
	SifInitIopHeap();
	sbv_patch_enable_lmb();

	init_scr();

#ifdef EEDEBUG
	//IOP will send debugging output to the EE using eedebug.irx
	SifAddCmdHandler(0x8000000E, &iopdebug_printk, NULL);
	SifExecModuleBuffer(EEDEBUG_irx, size_EEDEBUG_irx, 0, NULL, NULL);
#endif

	//Load modules
	SifExecModuleBuffer(DEV9_irx, size_DEV9_irx, 0, NULL, NULL);
	SifExecModuleBuffer(NETMAN_irx, size_NETMAN_irx, 0, NULL, NULL);
	SifExecModuleBuffer(SMAP_irx, size_SMAP_irx, 0, NULL, NULL);

	//Initialize NETMAN
	NetManInit();

	//The network interface link mode/duplex can be set.
	EthernetLinkMode = NETMAN_NETIF_ETH_LINK_MODE_AUTO;

	do{
		//Wait for the link to become ready before changing the setting.
		if(ethWaitValidNetIFLinkState() != 0) {
			scr_printf("Error: failed to get valid link status.\n");
			goto end;
		}

		//Attempt to apply the new link setting.
	} while(ethApplyNetIFConfig(EthernetLinkMode) != 0);

	//Initialize IP address.
	//In this example, DHCP is enabled, hence the IP, NM, GW and DNS fields are cleared to 0..
	ip4_addr_set_zero(&IP);
	ip4_addr_set_zero(&NM);
	ip4_addr_set_zero(&GW);
	ip4_addr_set_zero(&DNS);

	//Initialize the TCP/IP protocol stack.
	ps2ipInit(&IP, &NM, &GW);

	//Before enabling DHCP, wait for a valid link status.
	if(ethWaitValidNetIFLinkState() != 0)
	{
		scr_printf("Failed to get valid link status.\n");
		goto end;
	}

	//Enable DHCP
	ethApplyIPConfig(1, &IP, &NM, &GW, &DNS);

	scr_printf("Waiting for DHCP lease...");
	//Wait for DHCP to initialize, if DHCP is enabled.
	if (ethWaitValidDHCPState() != 0)
	{
		scr_printf("DHCP failed\n.");
		goto end;
	}
	scr_printf("done!\n");

	scr_printf("Initialized:\n");
	ethPrintLinkStatus();
	ethPrintIPConfig();

	// Start (LW)IPERF
	//lwiperf_start_tcp_server_default(NULL, NULL);
	udp_iperf();

	//At this point, network support has been initialized and the PS2 can be pinged.
	while (1) {
		ethPrintStats();
		msleep(1000);
	}

end:
	//To cleanup, just call these functions.
	ps2ipDeinit();
	NetManDeinit();

	//Deinitialize SIF services
	SifExitRpc();

	return 0;
}
