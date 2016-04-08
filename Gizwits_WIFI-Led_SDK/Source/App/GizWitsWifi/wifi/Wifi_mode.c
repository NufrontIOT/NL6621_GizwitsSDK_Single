
#include "../include/common.h"

/************************************************/
short wifiStatus = 0;
extern struct sys_status_t sys_status;

void GAgent_WiFiStatusCheckTimer(void)
{
    if((wifiStatus & WIFI_MODE_STATION) == WIFI_MODE_STATION
       && (wifiStatus & WIFI_STATION_STATUS) != WIFI_STATION_STATUS)
    {
        log_info("GAgent_WiFiStatusCheckTimer , Working at AP mode(%x)\r\n", wifiStatus);
        wifiStatus = WIFI_MODE_AP;
        DRV_WiFi_SoftAPModeStart();
    }
    return;
}


/* �����SSID��KEY, ��Stationģʽ���У������ӵ�ָ����SSID;
 * ������ΪAPģʽ���У�
 */
void Connect2LateWiFi(void)
{

	if ((g_stGAgentConfigData.flag & XPG_CFG_FLAG_CONNECTED) == XPG_CFG_FLAG_CONNECTED) {   //flash ������Ҫ�������������
	    //wifiStatus = WIFI_MODE_STATION;
        wifiStatus = wifiStatus | WIFI_STATION_STATUS | WIFI_MODE_STATION;
        log_info("try to connect wifi...[%08x]\n", g_stGAgentConfigData.flag);
		log_info("SSID: %s\n",g_stGAgentConfigData.wifi_ssid);
        log_info("KEY: %s\n",g_stGAgentConfigData.wifi_key);
		DRV_WiFi_StationCustomModeStart(g_stGAgentConfigData.wifi_ssid,g_stGAgentConfigData.wifi_key);

	} else {
	    wifiStatus = WIFI_MODE_AP;
		log_info("Connect2LateWiFi, Working at AP mode\r\n");
        DRV_WiFi_SoftAPModeStart();
	}

    return;
}

unsigned char wifi_sta_connect_ap_flag = 0;
extern VOID TestDirectConfig(VOID);

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  GAgent_SetDevNetWork
 *  Description:  Start to connect to a network when used DirectConfig mode or STA mode,
 *              otherwise, Create a network with AP mode.
 *         Note:  This interface will check the gpio 10 level, when it was press, then
 *              set to DirectConfig mode, else read flag from norflash, and judge launch
 *              SoftAP mode or STA mode.
 * =====================================================================================
 */
void GAgent_SetDevNetWork(void)
{
    UINT8 gpio_val;

	
	if (nl6621_GetConfigData(&g_stGAgentConfigData) != 0) {
		log_info("Get GAgentConfig Data error.\n");
	}

    /* ����Wifi���ӣ������Ƿ��ñ�������, ����ʹ��AP����Stationģʽ
	 * */
	if ((g_stGAgentConfigData.flag & XPG_CFG_FLAG_CONNECTED) == XPG_CFG_FLAG_CONNECTED) {   //flash ������Ҫ�������������
        /* Set system to STA status */
		sys_status.status = SYS_STATUS_WIFI_DIRECTCONFING;
		
		wifiStatus |= WIFI_STATION_STATUS | WIFI_MODE_STATION;
        log_info("Try to connect wifi...[%08x]\n", g_stGAgentConfigData.flag);
		log_info("SSID: %s\n", g_stGAgentConfigData.wifi_ssid);
        log_info("KEY: %s\n", g_stGAgentConfigData.wifi_key);
		DRV_WiFi_StationCustomModeStart(g_stGAgentConfigData.wifi_ssid, g_stGAgentConfigData.wifi_key);

		if (wifi_sta_connect_ap_flag != 0) { 	/* switch to softAP mode */
			goto SOFTAP;
		}

		/* Set system status to STA mode */
		sys_status.status = SYS_STATUS_WIFI_STA_CONNECTED;

	} else {
SOFTAP:
	    wifiStatus = WIFI_MODE_AP;
		log_info("Connect2LateWiFi, Working at AP mode\r\n");
        DRV_WiFi_SoftAPModeStart();
		/* Set system status to STA mode */
		sys_status.status = SYS_STATUS_WIFI_SOFTAP;
	}

    return;	
}

