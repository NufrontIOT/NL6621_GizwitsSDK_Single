/*
******************************************************************************
**
** Project     : 
** Description :    file for application layer task
** Author      :    pchn                             
** Date        : 
**    
** UpdatedBy   : 
** UpdatedDate : 
**
******************************************************************************
*/
/*
******************************************************************************
**                               Include Files
******************************************************************************
*/
#include "includes.h"

#ifdef TEST_APP_SUPPORT
#ifdef LWIP_SUPPORT
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
#include "airkiss.h"
#include "../App/Gizwits-GAgent-master/include/gagent.h"
#include "../App/Gizwits-GAgent-master/include/common.h"
#include "../App/Gizwits-GAgent-master/system/nl6621_uart.h"


/*
******************************************************************************
**                               MACROS and VARIABLES
******************************************************************************
*/

OS_EVENT * pLinkUpSem;
SYS_EVT_ID link_status;
int DIRECT_CONFIG = 0;
extern void uart_init(void);
extern pgcontext pgContextData;

extern int gizwits_main(VOID);
extern void GAgent_Init( pgcontext *pgc );
extern void GAgent_dumpInfo( pgcontext pgc );
extern void GAgent_Tick( pgcontext pgc );
extern int32 GAgent_SelectFd(pgcontext pgc,int32 sec,int32 usec );
extern uint32 GAgent_Lan_Handle(pgcontext pgc, ppacket prxBuf,int32 len);
extern void GAgent_Local_Handle( pgcontext pgc,ppacket Rxbuf,int32 length );
extern void GAgent_Cloud_Handle( pgcontext pgc, ppacket Rxbuf,int32 length );
/*
******************************************************************************
**                        VOID AppEvtCallBack(UINT32 event)
**
** Description  : deal with system event, such as linkup/linkdown
** Arguments    : 
                  event: SYS_EVT_ID
                  
** Returns      : нч
** Author       :                                   
** Date         : 
**
******************************************************************************
*/
VOID AppEvtCallBack(SYS_EVT_ID event)
{
	switch (event)
	{
		case SYS_EVT_LINK_UP:
			DBGPRINT(DEBUG_TRACE, "SYS_EVT_LINK_UP\n");
			link_status = SYS_EVT_LINK_UP;
			OSSemPost(pLinkUpSem);
			if(DIRECT_CONFIG)
			{
				pgContextData->gc.flag = pgContextData->gc.flag | XPG_CFG_FLAG_CONFIG;
			}
			break;
		
		case SYS_EVT_LINK_DOWN:
			DBGPRINT(DEBUG_TRACE, "SYS_EVT_LINK_DOWN\n");
		#ifdef TEST_SOFTAP_CONFIG
       		link_status = SYS_EVT_LINK_DOWN;
       	#endif
			break;

		case SYS_EVT_JOIN_FAIL:
			link_status = SYS_EVT_JOIN_FAIL;
			OSSemPost(pLinkUpSem);
			DBGPRINT(DEBUG_TRACE, "SYS_EVT_JOIN_FAIL\n");
			break;

		case SYS_EVT_DHCP_FAIL:
			DBGPRINT(DEBUG_TRACE, "SYS_EVT_DHCP_FAIL\n");
			break;

		case SYS_EVT_SCAN_DONE:
			DBGPRINT(DEBUG_TRACE, "SYS_EVT_SCAN_DONE\n");
		#ifdef TEST_SNIFFER
			TestSnifferFlag = 1;
		#endif
		#ifdef TEST_AIRKISS
			TestAirkissFlag = 1;
		#endif
			break;

		case SYS_EVT_DIRECT_CFG_DONE:
		#ifdef TEST_DIRECT_CONFIG
			TestDirectCfgFlag = 1;
		#else
			DBGPRINT(DEBUG_TRACE, "SYS_EVT_DIRECT_CFG_DONE\n");
		#endif
		InfWiFiStop();
		DIRECT_CONFIG = 1;
		InfConTryTimesSet(5);
    	InfNetModeSet(PARAM_NET_MODE_STA_BSS);
		InfEncModeSet(PARAM_ENC_MODE_AUTO);
		InfWiFiStart();

			break;

		default:
			break;
	}
}

void GAgent_SelectFd_Thread(void *arg)
{
	printf("GAgent_SelectFd_Thread success\n\r");
	while(1)
	{
		GAgent_SelectFd( pgContextData,0, 0);
		OSTimeDly(10);
	}
}

void GAgent_Lan_Handle_Thread(void *arg)
{
	printf("GAgent_Lan_Handle_Thread success\n\r");
	while(1)
	{
		GAgent_Lan_Handle( pgContextData, pgContextData->rtinfo.Rxbuf , GAGENT_BUF_LEN );
		OSTimeDly(10);
	}
}

void GAgent_Local_Handle_Thread(void *arg)
{
	printf("GAgent_Local_Handle_Thread success\n\r");
	while(1)
	{
		GAgent_Local_Handle( pgContextData, pgContextData->rtinfo.Rxbuf, GAGENT_BUF_LEN );
		OSTimeDly(1);
	}
}

void GAgent_Cloud_Handle_Thread(void *arg)
{
	printf("GAgent_Cloud_Handle_Thread success\n\r");
	while(1)
	{
		GAgent_Cloud_Handle( pgContextData, pgContextData->rtinfo.Rxbuf, GAGENT_BUF_LEN );
		OSTimeDly(10);
	}
}
/*
******************************************************************************
**                        VOID TestAppMain(VOID * pParam)
**
** Description  : application test task
** Arguments    : 
                  pParam
                  
** Returns      : нч
** Author       :                                   
** Date         : 
**
******************************************************************************
*/
VOID TestAppMain(VOID * pParam)
{
	UINT8 prioUser = TCPIP_THREAD_PRIO + 1;    

	uart_init();
	

    pLinkUpSem = OSSemCreate(0);
    InfSysEvtCBSet(AppEvtCallBack);
    InfLoadDefaultParam();

	GAgent_Init( &pgContextData );
    GAgent_dumpInfo( pgContextData );

	GAgent_Tick( pgContextData ); 

	sys_thread_new("GAgent_SelectFd_Thread", GAgent_SelectFd_Thread, NULL, NST_TEST_APP_TASK_STK_SIZE, prioUser++);
	sys_thread_new("GAgent_Local_Handle_Thread", GAgent_Local_Handle_Thread, NULL, NST_TEST_APP_TASK_STK_SIZE, prioUser++);
	sys_thread_new("GAgent_Lan_Handle_Thread", GAgent_Lan_Handle_Thread, NULL, NST_TEST_APP_TASK_STK_SIZE, prioUser++);
	sys_thread_new("GAgent_Cloud_Handle_Thread", GAgent_Cloud_Handle_Thread, NULL, NST_TEST_APP_TASK_STK_SIZE, prioUser++);

    while (1) 
    {   
		GAgent_Tick( pgContextData );                                       /* Task body, always written as an infinite loop.       */
        OSTimeDly(100);
    }

}
#endif // LWIP_SUPPORT
#endif // TEST_APP_SUPPORT

