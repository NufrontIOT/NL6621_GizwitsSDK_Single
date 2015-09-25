/*
 * =====================================================================================
 *
 *       Filename:  gizwits_main.c
 *
 *    Description:  Entry of gizwits cloud project
 *
 *        Version:  0.0.1
 *        Created:  2015/2/13 10:18:05
 *       Revision:  none
 *
 *         Author:  Lin Hui (Link), linhui.568@163.com
 *   Organization:  Nufront
 *
 *--------------------------------------------------------------------------------------          
 *       ChangLog:
 *        version    Author      Date          Purpose
 *        0.0.1      Lin Hui    2015/2/13      Create and optimize protocol
 *
 * =====================================================================================
 */

#include "include/gizwits_wifi.h"
#include "include/common.h"


#define GAGENT_TIMER_S_1        1
#define GAGENT_TIMER_US         0

extern const INT8U FwVerNum[3];
const INT8U GAgentVerNum[3] = {
	0x01,  /* Main version */ 
	0x07,  /* Sub version */
	0x00   /* Internal version */
};

/* system status */
struct sys_status_t sys_status;

/* ****************************************
 * GAgent Initialize interface
 * ****************************************/
extern void GAgent_Timer_Init(void);
extern void GAgent_TimerInit(void);
extern void GAgent_ResetTask(void);
extern void check_system_reset(void);
extern void check_socket(void);


/* task thread */
extern void UdpServerThread(void *arg);
extern void UdpBCTThread(void *arg);
extern void TcpServerThread(void *arg);
extern void TcpCloudThread(void *arg);

extern void NTPThread(void *arg); 

extern void init_system_task(void);
extern void SysStatusThread(void *arg);
extern VOID DevDiscoverThread(VOID * arg);

void print_version(void)
{
	log_notice("\nSDK Version:<%x.%02x.%02x>; GAgent Version:<%x.%02x.%02x> Release:<%s>\n", 
		FwVerNum[0], FwVerNum[1], FwVerNum[2],
		GAgentVerNum[0], GAgentVerNum[1], GAgentVerNum[2],
		#if DEBUG_ON
		"dbg"
		#else
		"rel"
		#endif
	);
	log_notice("=============================================================\n\n");
}

int nl6621_gizwits_entry(void)
{
	int ret = 0;
	UINT8 prioUser = TCPIP_THREAD_PRIO + 1;
	
	/* Print GAgent version */			
	print_version();

	log_info("Entry gizwits initialize(time:%d(s); %d(ms)).\n", 
			nl6621_GetTime_S(), nl6621_GetTime_MS());

	init_system_task();

	sys_status.status = SYS_STATUS_WIFI_STOP;
	sys_status.onboarding = SYS_STATUS_ONBOARDING_FAILED;
		
	sys_thread_new("SysStatusThread", SysStatusThread, NULL, NST_TEST_APP_TASK_STK_SIZE, prioUser++);

	/* Initialize the system timer
	 **/
    GAgent_TimerInit();
    nl6621_Timer_Creat(GAGENT_TIMER_S_1, 
            GAGENT_TIMER_US, nl6621_timer_handle); /* 1S */

#if HEART_BEAT_DEBUG_MACRO
	GAgent_Timer_Init();
#endif

	/* initialize GAgent resource */
    GAgent_Init();

	OSTimeDly(100);

	/* After module network create finish, setup soft timer to process the RESET task.
	 * */
	GAgent_ResetTask();

	/* 
	 * Create network task thread. Include UDP server, UDP Broadcast, TCP server, TCP
	 * login cloud task thread.
	 * */
	sys_thread_new("UdpServerThread", UdpServerThread, NULL, NST_TEST_APP_TASK_STK_SIZE, prioUser++);
	sys_thread_new("DevDiscoverThread", DevDiscoverThread, NULL, NST_TEST_APP_TASK_STK_SIZE, prioUser++);/*ota DevDiscoverThread*/
	sys_thread_new("UdpBCTThread", UdpBCTThread, NULL, NST_TEST_APP_TASK_STK_SIZE, prioUser++);
	sys_thread_new("TcpServerThread", TcpServerThread, NULL, NST_TEST_APP_TASK_STK_SIZE, prioUser++);
	sys_thread_new("TcpCloudThread", TcpCloudThread, NULL, NST_TEST_APP_TASK_STK_SIZE * 4, prioUser++);

	/* support NTP protocol */
	sys_thread_new("RTCThread", NTPThread, NULL, NST_TEST_APP_TASK_STK_SIZE, prioUser++);

	log_info("Try to connect wifi...[%08x]\n", g_stGAgentConfigData.flag);

	/*power save enable,soft ps mode, mcu keep running, open/close radio power dynamically*/
//	InfListenIntervalSet(255, 1);
#ifdef POWERSAVE_SUPPORT
	ret = InfPowerSaveSet(PS_MODE_SOFT);
	printf("InfPowerSaveSet : %d\n",ret);
#endif

    while (1) {
		GAgent_TimerRun();
		check_socket();
		check_system_reset();
	    OSTimeDly(10);
    }
}
