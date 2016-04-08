
#include "../include/common.h"

#define GAGENT_VERSION "B4R012D0"

GAgent_CONFIG_S g_stGAgentConfigData;
XPG_GLOBALVAR g_Xpg_GlobalVar;  
unsigned char Service_mac[6];


/*********************************************/
extern void GAgent_Login_Cloud(void);
extern void GAgent_SetDevNetWork(void);
extern void InitDomainSem(void);
extern VOID RespondSoftAPBroadcast(VOID);
extern OS_EVENT * pSoftApSem;
//extern void light_init(void);

/* system status */
extern struct sys_status_t sys_status;

/*********************************************/

void GAgent_initGPIO(void)
{
	BSP_GPIOPinMux(USER_GPIO_IDX_LED);		    /* led indicator */
	BSP_GPIOSetDir(USER_GPIO_IDX_LED, 1);		/* output */
	BSP_GPIOSetValue(USER_GPIO_IDX_LED, 0);	    /* low level */

	/* DirectConfig gpio is valied when set to low level */
	BSP_GPIOPinMux(USER_GPIO_DIRECTCONFIG);  
	BSP_GPIOSetDir(USER_GPIO_DIRECTCONFIG, GPIO_DIRECTION_INPUT); 

}


void GAgent_Global_Status_Init(void)
{
	/* Initialize Light resource */
	device_init();

    memset( &g_stGAgentConfigData, 0, sizeof(g_stGAgentConfigData));
    memset( &g_Xpg_GlobalVar, 0, sizeof(g_Xpg_GlobalVar));

    g_Xpg_GlobalVar.connect2CloudLastTime = nl6621_GetTime_S();

    memset( &g_stMQTTBroker, 0, sizeof(g_stMQTTBroker.socketid) );
    g_stMQTTBroker.socketid = -1;

	Cloud_HB_Status_init();

}

int GAgent_Config_Status_Init()
{
    //从存储获取保存的配置信息
	if (nl6621_GetConfigData(&g_stGAgentConfigData) == 0) {
        /* 判断数据是否有效 */
        if ( g_stGAgentConfigData.magicNumber == GAGENT_MAGIC_NUMBER )
        {
            /* 判断DID是否有效 */
            DIdLen = strlen(g_stGAgentConfigData.Cloud_DId);
            if (DIdLen == GAGENT_DID_LEN) {
                /* 有效DID */
                strcpy(g_Xpg_GlobalVar.DID, g_stGAgentConfigData.Cloud_DId);
                log_info( "with DID:%s len:%d\n", g_Xpg_GlobalVar.DID, DIdLen);
                return 0;
            }
        }
    }

    /* 需要初始化配置信息 */
    DIdLen = 0;
	
	if (g_stGAgentConfigData.flag != XPG_CFG_FLAG_CONNECTED) {
		memset(&g_stGAgentConfigData, 0x0, sizeof(g_stGAgentConfigData));
	}

    g_stGAgentConfigData.magicNumber = GAGENT_MAGIC_NUMBER;
    make_rand(g_stGAgentConfigData.wifipasscode);
    nl6621_SaveConfigData(&g_stGAgentConfigData);
	log_info("Reset GAgent config data.\n");

    return 1;
}

void GAgent_Init(void)
{
	log_notice("GAgent Version: %s.\n", GAGENT_VERSION);
    log_notice("Product Version: %s.\n", WIFI_HARD_VERSION);

    /* Set Gloabal varialbes and device resource */
	InitDomainSem(); 	/* initialize Domain analyze semaphare */
	GAgent_initGPIO();
    GAgent_Global_Status_Init();

    //获取网卡MAC 地址
    nl6621_GetWiFiMacAddress(Service_mac);
    sprintf(g_Xpg_GlobalVar.a_mac,"%02x%02x%02x%02x%02x%02x",Service_mac[0],Service_mac[1],Service_mac[2],
            Service_mac[3],Service_mac[4],Service_mac[5]);
    g_Xpg_GlobalVar.a_mac[12]='\0';

    log_notice("MCU WiFi MAC:%s\n", g_Xpg_GlobalVar.a_mac);

    GAgent_GetMCUInfo( nl6621_GetTime_MS() );

    if (GAgent_Config_Status_Init() == 0) {
        g_ConCloud_Status = CONCLOUD_REQ_M2MINFO;
        log_notice("MQTT_STATUS_PROVISION\n");

    } else {
        g_ConCloud_Status = CONCLOUD_REQ_DID;
        log_notice("MQTT_STATUS_START\n");
    }
    log_notice("passcode:%s(len:%d)\n", g_stGAgentConfigData.wifipasscode,strlen(g_stGAgentConfigData.wifipasscode));

    Init_Service();

	/* Create or connect network for WIFI device */
	GAgent_SetDevNetWork();

	GAgent_CreateTimer(GAGENT_TIMER_PERIOD, 1000, GAgent_Cloud_Timer);
    
    return;
}



/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  UdpServerThread
 *  Description:  Task thread for UDP server which can be used to process device scan
 *         Note:
 * =====================================================================================
 */
void UdpServerThread(void *arg)
{
	int ret = 0;

	while (1) {
	    /* Create UDP server socket */
		while (1) {
			OSTimeDly(50);
		    ret = Socket_CreateUDPServer(12414);
			if (ret < 0) {
				log_notice("Create UDP server socket error.\n");
			} else {
				log_notice("Create UDP server socket success.\n");
				break;
			} 
		}
	    
	    while (1) {
	        Socket_DoUDPServer();
			OSTimeDly(10); 
	    }
	}
}		/* -----  end of function UdpServerThread  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  UdpBCTThread
 *  Description:  Task thread for UDP Broadcast 
 *         Note:
 * =====================================================================================
 */
void UdpBCTThread(void *arg)
{   
	int err;

    /* Create UDP server socket */
	while (1) {
 		OSSemPend(pSoftApSem, 0, &err);
		OSTimeDly(500);
		RespondSoftAPBroadcast();
		BSP_ChipReset();	
	}
}		/* -----  end of function UdpServerThread  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  TcpServerThread
 *  Description:  Task thread for TCP server which can be used to receive MQTT and Gserver
 *              communication data.
 *         Note:
 * =====================================================================================
 */
void TcpServerThread(void *arg)
{   
	int ret = 0;

	while (1) {

		while (sys_status.onboarding == SYS_STATUS_ONBOARDING_SUCCESS);

	    /* Create TCP server socket */
		while (1) {
	        Socket_CreateTCPServer(12416);
			if (ret < 0) {
				OSTimeDly(50);
				log_notice("Create TCP server socket error.\n");
			} else {
				log_notice("Create TCP server socket success.\n");
				break;
			} 
		}
	    
	    while (1) {
	        Socket_CheckNewTCPClient();
	        Socket_DoTCPServer(); 
	    }
	}
}		/* -----  end of function UdpServerThread  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  TcpCloudThread
 *  Description:  Task thread for TCP login MQTT cloud server 
 *         Note:
 * =====================================================================================
 */
void TcpCloudThread(void *arg)
{   
	OSTimeDly(100);
	log_notice("Create cloud task success.\n"); 
    while (1) {
		OSTimeDly(10);
        GAgent_Login_Cloud();		 
    }
}		/* -----  end of function UdpServerThread  ----- */

