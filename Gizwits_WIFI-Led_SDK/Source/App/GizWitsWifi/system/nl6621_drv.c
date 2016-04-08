
#include "../include/common.h"


extern OS_EVENT * pLinkUpSem;

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ReadFlash
 *  Description:  Read data from nor flash. 
 * =====================================================================================
 */
static VOID ReadFlash(UINT8* pBuf, UINT32 DataLen, UINT32 ReadStartPos)
{
    QSpiFlashRead(ReadStartPos, pBuf, DataLen);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  writeFlash
 *  Description:  write data to nor flash 
 * =====================================================================================
 */
static VOID WriteFlash(UINT8* pData, UINT32 DataLen, UINT32 BurnStartPos)
{
	UINT32 j;
    UINT32 WriteAddr = BurnStartPos;
    UINT32 PageCnt;
    
    PageCnt = DataLen / FLASH_SECTOR_SIZE + ((DataLen % FLASH_SECTOR_SIZE) > 0 ? 1 : 0);
    
    for (j = 0; j < PageCnt; j++) {
        QSpiFlashEraseSector(WriteAddr);
		OSTimeDly(1); 
        QSpiWriteOneSector(WriteAddr, pData);
        pData += FLASH_SECTOR_SIZE;
        WriteAddr += FLASH_SECTOR_SIZE;
    }
}

#define GAGENT_CONFIG_DATA_ADDR      (0x64000)  /* about 400k Bytes offset */

/* 
 * |-- flag --|-- GAgent_CONFIG_S --|
 * Read gagent config data, when flag set to 0x87654321, it means the Config data had set, 
 * otherwise means there is no gagent config data.
 * */
int nl6621_GetConfigData(GAgent_CONFIG_S *pConfig)
{
	ReadFlash((UINT8 *)pConfig, sizeof(GAgent_CONFIG_S), GAGENT_CONFIG_DATA_ADDR);
	return 0;
}

// 保存gagent配置信息
int nl6621_SaveConfigData(GAgent_CONFIG_S *pConfig)
{
	WriteFlash((UINT8*)pConfig, sizeof(GAgent_CONFIG_S), GAGENT_CONFIG_DATA_ADDR);
    return 0;
}

#define GAGENT_SMART_LED_DATA_ADDR   (0x66000)//
 int nl6621_GetSmartLedData(led_status_flash *pConfig)
{
	ReadFlash((UINT8 *)pConfig, sizeof(led_status_flash), GAGENT_SMART_LED_DATA_ADDR);
	return 0;
}
//保存led的RGB以及其他数据
int nl6621_SaveSmartLedData(led_status_flash *pConfig)
{
	WriteFlash((UINT8*)pConfig, sizeof(led_status_flash), GAGENT_SMART_LED_DATA_ADDR);
    return 0;
}

void debug_gagent_config_data(void)
{
	GAgent_CONFIG_S temp;
	memset(&temp, 0, sizeof(GAgent_CONFIG_S));
	OSTimeDly(10);
	nl6621_GetConfigData(&temp);
	log_info("Get config data: MAGIC Number:%d, flag:0x%x, SSID:%s, password:%s.\n\n", 
			temp.magicNumber, temp.flag, temp.wifi_ssid, temp.wifi_key);
}


void nl6621_GetWiFiMacAddress(unsigned char *pMacAddress)
{
    memcpy((unsigned char *)pMacAddress, PermanentAddress, MAC_ADDR_LEN);
}

unsigned int nl6621_GetTime_MS(void)
{     
	return (OSTimeGet() % (100)) * 10;
}

unsigned int nl6621_GetTime_S(void)
{
    return (OSTimeGet() / (100));
}

/* stub */
void DRV_WiFi_StationCustomModeStart(char *StaSsid, char *StaPass )
{
	unsigned char err;

    IP_CFG IpParam;
    /* 
     * Start Wifi STA mode and connect to the AP with StaSsid and StaPass.
     * */ 
    InfWiFiStop();                                                                               
    OSTimeDly(10);                                                                               

    InfLoadDefaultParam();                                                                       
    
    memset(&IpParam, 0, sizeof(IpParam));                                                        
    IpParam.DhcpTryTimes = 0;                                                                    
    IpParam.bDhcp = 1;                                                                           
    InfIpSet(&IpParam);                                                                          

	InfConTryTimesSet(5);
    InfNetModeSet(PARAM_NET_MODE_STA_BSS);
	InfEncModeSet(PARAM_ENC_MODE_AUTO);	
    InfSsidSet((UINT8 *)StaSsid, strlen(StaSsid));    /* set ssid */                             
    InfKeySet(0, (UINT8 *)StaPass, strlen(StaPass));  /* set pw */                               

    log_info("Connecting to AP(%s,%s)...\n\r", StaSsid, StaPass);                                                             
    InfWiFiStart(); 
	
	OSSemPend(pLinkUpSem, 0, &err);                                                                             
    
    return;
}

/* stub */
void DRV_WiFi_SoftAPModeStart(void)
{
    /* 
     * Create AP network.
     * */ 
    InfWiFiStop();                                                                               
    OSTimeDly(10);                                                                               

    InfLoadDefaultParam();                                                                       
    InfNetModeSet(PARAM_NET_MODE_SOFTAP);                                                        

    InfWiFiStart();
    return;
}



void nl6621_timer_handle(void *ptmr, void *parg)
{
    GAgent_TimerHandler(0, NULL);
}


/* NL6621 only support 10 ms software timing interval */
void nl6621_Timer_Creat(int period_s, unsigned long period_us, nl6621_timer_callback p_callback)
{
    NST_TIMER *pTimer = NULL;
    int time_value = 0;

    /* caculator the nl6621 timer */
    if (period_s > 0) {
        time_value = period_s * 1000;
    }

    if (period_us > 10000) {
        time_value += period_us / 10000; 
    }

    NST_InitTimer(&pTimer, p_callback, NULL, NST_TRUE);
    NST_SetTimer(pTimer, time_value);

	log_info("Timer value:%d.\n", time_value);

    return;
}

void nl6621_Reset(void)
{
    BSP_ChipReset();
    return;
}

/****************************************************************/
/****************** GAgent LED indicater interface **************/
/****************************************************************/
extern struct sys_status_t sys_status;

void update_led_indicator(unsigned int low_time, unsigned int high_time)
{
	BSP_GPIOSetValue(USER_GPIO_IDX_LED, GPIO_LOW_LEVEL);
	OSTimeDly(low_time);	
	BSP_GPIOSetValue(USER_GPIO_IDX_LED, GPIO_HIGH_LEVEL);
	OSTimeDly(high_time);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SysStatusThread
 *  Description:  Task thread for led indicator of system 
 *         Note:
 * =====================================================================================
 */
void SysStatusThread(void *arg)
{
	log_notice("Create system LED indicator task success.\n");
	while (1) {
		switch (sys_status.status) {
			case SYS_STATUS_WIFI_STOP:
				while (sys_status.status == SYS_STATUS_WIFI_STOP) {
					BSP_GPIOSetValue(USER_GPIO_IDX_LED, GPIO_LOW_LEVEL);
					OSTimeDly(200);
				}
				break;

			case SYS_STATUS_WIFI_SOFTAP:
				update_led_indicator(300, 100);
				break;

			case SYS_STATUS_WIFI_DIRECTCONFING:
				update_led_indicator(10, 10);
				break;

			case SYS_STATUS_WIFI_STA_CONNECTED:
				update_led_indicator(100, 100);
				break;

			case SYS_STATUS_WIFI_STA_LOGIN:
				while (sys_status.status == SYS_STATUS_WIFI_STA_LOGIN) {
					//BSP_GPIOSetValue(USER_GPIO_IDX_LED, GPIO_HIGH_LEVEL);
					OSTimeDly(200);
				}
				break;

			default:
				break;
		}		
	}
}


/****************************************************************/
/****************** GAgent reset system interface ***************/
/****************************************************************/
#define GAGENT_SYSTEM_RESET_DELAY_TIME		(7 * 100)
#define USER_RESET_BUTTON_PRESS				(0)		/* 1:for qfn88; 0:for NF-210S */
#define USER_RESET_BUTTON_UP				(0)
#define USER_RESET_PRESS_TIMEOUT			(7)		/* when press 4s, reset system */

static unsigned int system_reset_flag = 0;

void GAgent_System_reset(void *ptmr, void *parg)
{
	/* Reset gpio is valied when set to low level */
	BSP_GPIOPinMux(USER_GPIO_RESET_BUTTON);  
	BSP_GPIOSetDir(USER_GPIO_RESET_BUTTON, GPIO_DIRECTION_INPUT);

	/* Set GAgent reset flag */
	system_reset_flag = 0x12345678;
	log_notice("Create GAgent reset task success.\n");
}


/* Create soft timer, interrupt */
void GAgent_ResetTask(void)
{
    NST_TIMER *pTimer = NULL;
    int time_value = GAGENT_SYSTEM_RESET_DELAY_TIME;

    NST_InitTimer(&pTimer, GAgent_System_reset, NULL, NST_FALSE);
    NST_SetTimer(pTimer, time_value);
}

void check_socket(void)
{
	UINT8 gpio10_val;
	UINT8 gpio30_val;
	int i;

		gpio10_val = BSP_GPIOGetValue(USER_GPIO_DIRECTCONFIG);
		if (gpio10_val == 0) {
			OSTimeDly(5);		/* delay 20ms filter button shake */
        	gpio10_val = BSP_GPIOGetValue(USER_GPIO_DIRECTCONFIG);
			if(gpio10_val == 0)
			{
				gpio30_val = ((NST_RD_GPIO_REG(SWPORTA_DR) & (1 << STRIP_GPIO_NUM_ONE))==0) ? 0 : 1;
				if(gpio30_val == 0)
				{
					BSP_GPIOSetValue(STRIP_GPIO_NUM_ONE, 1);
					BSP_GPIOSetValue(USER_GPIO_IDX_LED, 0);
				}else if(gpio30_val == 1)
				{
					BSP_GPIOSetValue(STRIP_GPIO_NUM_ONE, 0);
					BSP_GPIOSetValue(USER_GPIO_IDX_LED, 1);		
				}
			}
	}
}


void check_system_reset(void)
{
	UINT8 gpio_val;
	int i;

	if (system_reset_flag == 0x12345678) {
		gpio_val = BSP_GPIOGetValue(USER_GPIO_RESET_BUTTON);
		if (gpio_val == USER_RESET_BUTTON_PRESS) {
			for (i = 0; i < USER_RESET_PRESS_TIMEOUT; i++) {
				OSTimeDly(50);		/* delay 500ms filter button shake */
				gpio_val = BSP_GPIOGetValue(USER_GPIO_RESET_BUTTON);
				if (gpio_val != USER_RESET_BUTTON_PRESS) {
					break;
				}
			}
			
			if (i == USER_RESET_PRESS_TIMEOUT) {
				/* reset module */
				log_warn("RESET button press more than 4 seconds, reseting System.\n");
				OSTimeDly(100);
				memset( &g_stGAgentConfigData.wifi_ssid, 0, sizeof(g_stGAgentConfigData.wifi_ssid));
				memset( &g_stGAgentConfigData.wifi_key, 0, sizeof(g_stGAgentConfigData.wifi_key));
				memset( &g_stGAgentConfigData.flag, 0, sizeof(g_stGAgentConfigData.flag));
				nl6621_SaveConfigData(&g_stGAgentConfigData);
				OSTimeDly(10);
				nl6621_Reset();
			} 
		}				
	}
}

