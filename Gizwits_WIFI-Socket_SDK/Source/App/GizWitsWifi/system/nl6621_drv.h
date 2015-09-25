#ifndef __nl6621_DRV_H__
#define __nl6621_DRV_H__

/* product 相关的宏定义 */
#define WIFI_HARD_VERSION   "06NL6621"
#define WIFI_HARDVER        "06NL6621"
#define WIFI_SOFTVAR        "04000001"


typedef void (*nl6621_timer_callback)(void *ptmr, void *parg);

typedef struct {
	 unsigned int boot_addr;		
	 unsigned short boot_flag;			
	 unsigned short boot_status;
	 unsigned int fw1_times;
	 unsigned int fw2_times;
}__attribute__((packed))BOOT_PARAM;

extern int nl6621_GetConfigData(GAgent_CONFIG_S *pConfig);
extern int nl6621_SaveConfigData(GAgent_CONFIG_S *pConfig);
extern void nl6621_GetWiFiMacAddress(unsigned char *pMacAddress);

extern unsigned int nl6621_GetTime_MS(void);
extern unsigned int nl6621_GetTime_S(void);

extern void DRV_WiFi_StationCustomModeStart(char *StaSsid,char *StaPass );
extern void DRV_WiFi_SoftAPModeStart(void);

extern void nl6621_timer_handle(void *ptmr, void *parg);
extern void nl6621_Timer_Creat(int period_s, unsigned long period_us,
        nl6621_timer_callback p_callback);
extern void nl6621_Reset(void);

#endif  /* __nl6621_DRV_H__ */
