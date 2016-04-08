/*
 * =====================================================================================
 *
 *       Filename:  common.h
 *
 *    Description:  Common head file for Gizwits    
 *
 *        Version:  0.0.1
 *        Created:  12/25/2014 09:35:55 AM
 *       Revision:  none
 *
 *         Author:  Lin Hui (Link), linhui.568@163.com
 *   Organization:  Guangdong Nufront CSC Co., Ltd
 *
 *--------------------------------------------------------------------------------------          
 * ChangLog:
 *  version    Author      Date         Purpose
 *  0.0.1      Lin Hui    12/25/2014    create and initialize  
 *
 * =====================================================================================
 */

#ifndef __COMMON_H__
#define __COMMON_H__


#include "includes.h"

#include "lwip/memp.h"
#include "lwIP.h"
#include "lwIP/tcp.h"
#include "lwIP/udp.h"
#include "lwIP/tcpip.h"
#include "netif/etharp.h"
#include "lwIP/dhcp.h"
#include "arch/sys_arch.h"
#include "lwIP/sockets.h"
#include "lwIP/netdb.h"
#include "lwIP/dns.h"

#include "lwIP/snmp.h"
#include "lwIP/stats.h"
#include "lwip/inet_chksum.h"  /* add for inet_chksum interface */

#include "errno.h"
#include <stdbool.h>
#include "../include/gagent_typedef.h"


#define malloc OSMMalloc
#define free OSMFree 


#define NUAGENT_UART_SWITCH					(1)	 	/* System uart(included simulater) switch macro */
/*
 * Debug Macro
 **/
#define HEART_BEAT_DEBUG_MACRO				(0)	  	/* 0:use timer of gizwits; 1:use new timer */


#define GAGENT_FEATURE_OTA 					(0)

#define USER_GPIO_IDX_LED   				(3)				/* System led indicator. 4:for develop board, 3:for NF-210S */
#define USER_GPIO_DIRECTCONFIG              (10)     		/* start DirectConfig mode. 3:for develop board, 10:for NF-210S */
#define USER_GPIO_RESET_BUTTON	USER_GPIO_DIRECTCONFIG		/* System reset gpio */

#define SETBIT(x, y) (x |= (1 << y))		/* set bit y to 1 on x */
#define CLRBIT(x, y) (x &= ~(1 << y)) 		/* set bit y to 0 on x */


/* structure for system store */
struct sys_status_t {

#define SYS_STATUS_WIFI_STOP				(0)
#define SYS_STATUS_WIFI_SOFTAP				(1)
#define SYS_STATUS_WIFI_DIRECTCONFING		(2)
#define SYS_STATUS_WIFI_STA_CONNECTED		(3)
#define SYS_STATUS_WIFI_STA_LOGIN			(4)

	unsigned char status;

#define SYS_STATUS_ONBOARDING_SUCCESS		(1)
#define SYS_STATUS_ONBOARDING_FAILED		(0)
	unsigned char onboarding;
};



/* ===================================================================================== */
/* ============================= external interface ==================================== */
/* ===================================================================================== */

int strncmp(register const char *s1, register const char *s2, register unsigned int n);


/* ****************************************
 * GAgent function interface
 * ****************************************/
extern int connect_mqtt_socket(int iSocketId, struct sockaddr_in *Msocket_address, unsigned short port, char *MqttServerIpAddr);

extern OS_EVENT * pLinkUpSem;
extern SYS_EVT_ID link_status;
extern char* GAgent_strstr(const char*s1, const char*s2);
extern void simu_uart_init(void);
extern void print(char* fmt, ...);
extern void SimuSendOneByte(unsigned char Byte);
void DRV_WiFi_SoftAPModeStart(void);
void DRV_WiFi_StationCustomModeStart(char *StaSsid, char *StaPass );
int8 GAgent_CheckSum( int8 *data,int dataLen );
#endif


