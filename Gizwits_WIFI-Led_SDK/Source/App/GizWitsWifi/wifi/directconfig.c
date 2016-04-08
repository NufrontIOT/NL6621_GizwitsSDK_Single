#include "../include/common.h"
 
OS_EVENT * pLinkUpSem;
OS_EVENT * pSoftApSem;
int TestDirectCfgFlag = 0;

extern unsigned char wifi_sta_connect_ap_flag;
extern struct sys_status_t sys_status;			/* system status */
extern void debug_gagent_config_data(void);

/*
******************************************************************************
**                        void RespondUdpBroadcast(void)
**
** Description  : respond with udp broadcast packet for 10 seconds.
** Arguments    : 
                  
** Returns      : 
** Author       :                                   
** Date         : 
**
******************************************************************************
*/
VOID RespondUdpBroadcast(VOID)
{
    int ret = 0;
    int count = 0;
    
    int udp_sock;
    const int opt = 1;    
    struct sockaddr_in addrto;
    int nlen = sizeof(addrto);
    char sendbuf[24] = {0}; 

    /* fill with response data */
    sendbuf[0] = 0x18;
    sendbuf[8] = 0x06;
    sendbuf[10] = 0x01;
    
    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
    {
        log_err("UDP Broadcast Socket error\n");
        return;
    }
    
    if (setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt)) == -1) 
    {  
        log_err("set UDP Broadcast socket error...\n");  
		lwip_close(udp_sock);  
        return;  
    }  
    
    memset(&addrto, 0, sizeof(struct sockaddr_in));  
    addrto.sin_family = AF_INET;  
    addrto.sin_addr.s_addr = htonl(INADDR_BROADCAST);  
    addrto.sin_port=htons(60002);  
    
    log_info("Respond with udp broadcast.");
    
    while (1)
    {
        ret = sendto(udp_sock, sendbuf, 24, 0, (struct sockaddr *)&addrto, nlen);
        if (ret < 0) 
        {
            continue;
        } 
        
        /* print debug message to console */
        if (count < 3 * 10)  // broadcast 5s
        {
            if (count % 200 == 0) 
                log_info("\b..");
    
            if (count % 3 == 0) 
                log_info("\b\\");
            else if (count % 3 == 1) 
                log_info("\b-");
            else if (count % 3 == 2) 
                log_info("\b/");
                
            OSTimeDly(10);
        } 
        else
        {
            log_info("\n");
            break;
        }
        count++;
    }

	lwip_close(udp_sock);
    log_info("Respond finished.\n");
}

/*
******************************************************************************
**                        void RespondSoftAPBroadcast(void)
**
** Description  : respond with SoftAP broadcast packet .
** Arguments    : 
                  
** Returns      : 
** Author       :                                   
** Date         : 
**
******************************************************************************
*/
VOID RespondSoftAPBroadcast(VOID)
{
	int ret = 0;
	int count = 0;
	
	int udp_sock;
	const int opt = 1;	  
	struct sockaddr_in addrto;
	int nlen = sizeof(addrto);
	char sendbuf[80] = {0};
 
	/* fill with response data */
    sendbuf[3] = 0x03;
    sendbuf[4] = 0x33;
    sendbuf[5] = 0x00;
	sendbuf[7] = 0x19;
	sendbuf[9] = 0x0c;
	sprintf((&sendbuf[10]),"%s",g_Xpg_GlobalVar.a_mac);
	sendbuf[23] = 0x20;
	sprintf((&sendbuf[24]),"%s",g_Xpg_GlobalVar.Xpg_Mcu.product_key);
	sendbuf[57] = 0x16;
	sprintf((&sendbuf[58]),"%s",g_Xpg_GlobalVar.DID);
	
	if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
	{
		log_err("UDP Broadcast Socket error\n");
		return;
	}

	
	if (setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt)) == -1) 
	{  
		log_err("set UDP Broadcast socket error...\n");  
		lwip_close(udp_sock);  
		return;  
	}
	
	memset(&addrto, 0, sizeof(struct sockaddr_in));  
	addrto.sin_family = AF_INET;  
	addrto.sin_addr.s_addr = htonl(INADDR_BROADCAST);  
	addrto.sin_port=htons(2415); 
		
	
	log_info("Respond with udp broadcast.");
	
	while (1)
	{
		ret = sendto(udp_sock, sendbuf, 80, 0, (struct sockaddr *)&addrto, nlen);
		if (ret < 0) 
		{
			continue;
		}
		 
		if(count == 10)
		{
			log_info("device connected!\n");
			count = 0;
			break;	
		}
		count++;
		OSTimeDly(10);
	}

	lwip_close(udp_sock);
	log_info("Respond finished.\n");
	sys_status.onboarding = SYS_STATUS_ONBOARDING_FAILED;
	return;
}


extern void GAgent_SaveSSIDAndKey(char *pSSID, char *pKey);

VOID TestDirectConfig(VOID)
{
    UINT8 SsidLen = 0;
    UINT32 count = 0;

	memset(&SysParam.WiFiCfg.Ssid, 0, strlen(SysParam.WiFiCfg.Ssid));
	memset(&SysParam.WiFiCfg.PSK, 0, strlen(SysParam.WiFiCfg.PSK));

    /* start direct config */
    log_info("Direct config start, waiting ");
    InfDirectCfgStart(0, 0, 0);

    /* print steps */
    while (1) 
    {
        if (TestDirectCfgFlag == 1) 
        {
            TestDirectCfgFlag = 0;
            log_info("\n");
            break;
        }
        
        if (count % 10 == 0) 
            log_info("\b..");		
        
        if (count % 3 == 0) 
            log_info("\b\\");
        else if (count % 3 == 1) 
            log_info("\b-");
        else if (count % 3 == 2) 
            log_info("\b/");
            
        count++;
        OSTimeDly(30);				
    }

    log_info("Direct config succeeded\n");

    /* stop direct config */
    InfWiFiStop();

    /* print the received config parameters (ssid and key) */
    SsidLen = strlen((const char *)SysParam.WiFiCfg.Ssid);
    if (SsidLen)
    	log_info("Ssid:%s(len=%d)\n", SysParam.WiFiCfg.Ssid, SsidLen);
    if (SysParam.WiFiCfg.KeyLength)
    	log_info("Key:%s(len=%d)\n", SysParam.WiFiCfg.PSK, SysParam.WiFiCfg.KeyLength);

    /* use the received parameters to start wifi connection */
    InfNetModeSet(PARAM_NET_MODE_STA_BSS);
	InfConTryTimesSet(5);
    InfWiFiStart();
    log_info("Start wifi connection.\n");

    /* wait wifi link up */
    while (TestDirectCfgFlag == 0)
        OSTimeDly(30);

	if(TestDirectCfgFlag == 2)
	{
		BSP_ChipReset();
		return;
	}
    TestDirectCfgFlag = 0;

	/* Save SSID and password to nor flash */
	GAgent_SaveSSIDAndKey((char*)SysParam.WiFiCfg.Ssid, (char*)SysParam.WiFiCfg.PSK);

    /* send broadcast message to inform direct config peer*/
    RespondSoftAPBroadcast();
}


VOID AppEvtCallBack(SYS_EVT_ID event)
{
	switch (event)
	{
		case SYS_EVT_LINK_UP:
			log_info("SYS_EVT_LINK_UP\n");
			wifi_sta_connect_ap_flag = 0;

			/* if device onboarding, reset system */
			if (sys_status.onboarding == SYS_STATUS_ONBOARDING_SUCCESS) {
				NVIC_DisableIRQ(TMR1_IRQn);
				nl6621_SaveConfigData(&g_stGAgentConfigData);
				debug_gagent_config_data();
				OSSemPost(pSoftApSem);
			}

			OSSemPost(pLinkUpSem);
       		TestDirectCfgFlag = 1;
			break;
		
		case SYS_EVT_LINK_DOWN:
			log_info("SYS_EVT_LINK_DOWN\n");
			break;

		case SYS_EVT_JOIN_FAIL:

			wifi_sta_connect_ap_flag = 1;

			if(sys_status.status == SYS_STATUS_WIFI_DIRECTCONFING)
			{
				TestDirectCfgFlag = 2;
			}

			if(sys_status.status == SYS_STATUS_WIFI_SOFTAP)
			{
			   log_info("SoftAP connected fail\n");
			   BSP_ChipReset();
			}

			OSSemPost(pLinkUpSem);

			log_info("SYS_EVT_JOIN_FAIL\n");
			break;

		case SYS_EVT_DHCP_FAIL:
			log_info("SYS_EVT_DHCP_FAIL\n");
			break;

		case SYS_EVT_SCAN_DONE:
			log_info("SYS_EVT_SCAN_DONE\n");
			break;

		case SYS_EVT_DIRECT_CFG_DONE:
			TestDirectCfgFlag = 1;
			log_info("SYS_EVT_DIRECT_CFG_DONE\n");
			break;

		default:
			break;
	}
}

void init_system_task(void)
{
	pLinkUpSem = OSSemCreate(0);
	pSoftApSem = OSSemCreate(0);

    InfSysEvtCBSet(AppEvtCallBack);
    InfLoadDefaultParam();		
}

