
#include "../include/common.h"
#include "strip.h"

struct strip_data strip_data;

NST_TIMER *Timer_on = NULL;
NST_TIMER *Timer_off = NULL;
NST_TIMER *Timer_countDown = NULL;
char repeat_flag;

int timing_week()
{
	unsigned int ticks;
	unsigned int system_sec;
	unsigned int days;
	unsigned int wday;

	ticks  = OSTimeGet();
	system_sec = (ticks / OS_TICKS_PER_SEC +1417305600 + 28800);	
	days = ((system_sec/60)/60)/24;
	wday = (4 + days)%7;

	if(wday == 0)
	{
		wday = 7;
	}

	return wday;
}

void timer_on_task(void *ptmr, void *parg)
{
	log_info("Start to open socket(%s).\n", strip_data.strip_table[0].socket_name);

	if(strip_data.strip_table[0].socket_status.week_repeat == 0)
	{
		BSP_GPIOSetValue(strip_data.strip_table[0].socket_gpio, 1);
		BSP_GPIOSetValue(USER_GPIO_IDX_LED, 0);
		SETBIT(strip_data.strip_table[0].socket_status.onoff, 0);
	}

	if(strip_data.strip_table[0].socket_status.week_repeat != 0 && repeat_flag == 1)
	{
		NST_SetTimer(Timer_on, 24*60*1000*60);
	}

	if(repeat_flag == 1 && ((strip_data.strip_table[0].socket_status.week_repeat >> (timing_week() - 1)) & 1) )
	{
		BSP_GPIOSetValue(strip_data.strip_table[0].socket_gpio, 1);
		BSP_GPIOSetValue(USER_GPIO_IDX_LED, 0);
		SETBIT(strip_data.strip_table[0].socket_status.onoff, 0);
	}
}

void timer_off_task(void *ptmr, void *parg)
{
	log_info("Start to shutdown socket(%s).\n", strip_data.strip_table[0].socket_name);

	if(strip_data.strip_table[0].socket_status.week_repeat == 0)
	{
		BSP_GPIOSetValue(strip_data.strip_table[0].socket_gpio, 0);
		BSP_GPIOSetValue(USER_GPIO_IDX_LED, 1);
		CLRBIT(strip_data.strip_table[0].socket_status.onoff, 0);
	}

	if(strip_data.strip_table[0].socket_status.week_repeat != 0 && repeat_flag == 1)
	{
		NST_SetTimer(Timer_off, 24*60*1000*60);
	}

	if(repeat_flag == 1 && ((strip_data.strip_table[0].socket_status.week_repeat >> (timing_week() - 1)) & 1) )
	{
		BSP_GPIOSetValue(strip_data.strip_table[0].socket_gpio, 0);
		BSP_GPIOSetValue(USER_GPIO_IDX_LED, 1);
		CLRBIT(strip_data.strip_table[0].socket_status.onoff, 0);
	}
}

void timer_countDown_task(void *ptmr, void *parg)
{
	log_info("Start to countdown on socket(%s).\n", strip_data.strip_table[0].socket_name);
//	BSP_GPIOSetValue(strip_data.strip_table[0].socket_gpio, 0);
}


/* Create soft timer */
void timer_task_init(void)
{    
    NST_InitTimer(&Timer_on, timer_on_task, NULL, NST_FALSE);
	NST_InitTimer(&Timer_off, timer_off_task, NULL, NST_FALSE);
	NST_InitTimer(&Timer_countDown, timer_countDown_task, NULL, NST_FALSE);
}

/*count time*/
int timing_value(int socket_status_minute)
{
	unsigned int ticks;
	unsigned int system_sec;
	unsigned int system_minute;
	unsigned int timing = 0;

	ticks  = OSTimeGet();
	system_sec = (ticks / OS_TICKS_PER_SEC +1417305600 + 28800);
	system_minute = (system_sec/60)%60 + (((system_sec/60)/60)%24)*60;

	if(system_minute == socket_status_minute){
			timing = 10;
	}else if(system_minute < socket_status_minute){
			timing = ((UINT32)socket_status_minute 
							- system_minute) * 1000 * 60 - (system_sec % 60) * 1000;
	}else if(system_minute > socket_status_minute){
				timing = (24*60 - (system_minute - socket_status_minute))*1000*60;
	}

	log_info("timing:%d\n", timing);
	return timing;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  get_socket_status
 *  Description:  Get the status of socket which gpio parameter indicator. 
 *         Note:
 * =====================================================================================
 */
int get_socket_status(unsigned char gpio, socket_status_t *socket_status)
{
	if (gpio == STRIP_GPIO_NUM_ONE) {
		memcpy(socket_status, &strip_data.strip_table[0].socket_status, sizeof(socket_status_t));
	} 

    return 0;
}		/* -----  end of function get_socket_status  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  strip_data_parse
 *  Description:  Parse P0 data come from cloud server, if the bit is not set, reload the
 *  			current data to strip_data structure.
 *   Parameters:
 *         Note:
 * =====================================================================================
 */
int strip_data_parse(unsigned char action, unsigned char attr_flags, unsigned char *ctrl_data)
{
	BOOL_T isCancelled = OS_FALSE;

    /* socket on/off */
    if ((attr_flags & 0x01) == 0x01) {
		if ((ctrl_data[0] & 0x01) == 0x01) {
			BSP_GPIOSetValue(strip_data.strip_table[0].socket_gpio, 1);
			BSP_GPIOSetValue(USER_GPIO_IDX_LED, 0);
			SETBIT(strip_data.strip_table[0].socket_status.onoff, 0);
		} else {
			BSP_GPIOSetValue(strip_data.strip_table[0].socket_gpio, 0);
			BSP_GPIOSetValue(USER_GPIO_IDX_LED, 1);
			CLRBIT(strip_data.strip_table[0].socket_status.onoff, 0);
		}
		log_info("\tSet Socket:(%s)\n", ((ctrl_data[0] & 0x01) == 0x0) ? "OFF" : "ON");	       
    }

	
    /* Time_OnOff */
	if ((attr_flags & 0x02) == 0x02) {
		if ((ctrl_data[0] & 0x02) == 0x02) {
			SETBIT(strip_data.strip_table[0].socket_status.onoff, 1);

			if(strip_data.strip_table[0].socket_status.week_repeat != 0)
			{
				repeat_flag = 1;
			}

			/* setup timer on/off */
			NST_SetTimer(Timer_on, timing_value(strip_data.strip_table[0].socket_status.time_on_minute));
			NST_SetTimer(Timer_off,timing_value(strip_data.strip_table[0].socket_status.time_off_minute));

		} else {
			CLRBIT(strip_data.strip_table[0].socket_status.onoff, 1);
			/* delete timer on/off */
			NST_CancelTimer(Timer_on, &isCancelled);
			NST_CancelTimer(Timer_off, &isCancelled);
			repeat_flag = 0;
		}

		log_info("\tSet Time_OnOff: %s\n", ((ctrl_data[0] & 0x02) == 0x0) ? "OFF" : "ON");		
	}

    /* CountDown_OnOff */
	if ((attr_flags & 0x04) == 0x04) {
		if ((ctrl_data[0] & 0x04) == 0x04) {
			SETBIT(strip_data.strip_table[0].socket_status.onoff, 2);
			/* setup countdown timer */
//			NST_SetTimer(Timer_countDown, (UINT32)strip_data.strip_table[0].socket_status.countdown_minute * 1000 * 60/* * 100 * 60 */);

		} else {
			CLRBIT(strip_data.strip_table[0].socket_status.onoff, 2);
			/* delete countdown timer */
//			NST_CancelTimer(Timer_countDown, &isCancelled);
		}
		log_info("\tSet CountDown_OnOff: %s\n", ((ctrl_data[0] & 0x04) == 0x0) ? "OFF" : "ON");		
	}

    /* Week_Repeat */
	if ((attr_flags & 0x08) == 0x08) {
		strip_data.strip_table[0].socket_status.week_repeat = ctrl_data[1];

		if(strip_data.strip_table[0].socket_status.week_repeat != 0)
		{
				repeat_flag = 1;
		}else if(strip_data.strip_table[0].socket_status.week_repeat == 0){
				repeat_flag = 0;
		}

		log_info("\tSet Week_Repeat:%d.\n", ctrl_data[1]);		
	}

	/* Time_On_Minute */
    if ((attr_flags & 0x10) == 0x10) { 	
		decodeInt16((const char *)(ctrl_data + 2), (short *)&strip_data.strip_table[0].socket_status.time_on_minute);
		NST_SetTimer(Timer_on, timing_value(strip_data.strip_table[0].socket_status.time_on_minute));
		log_info("\tSet Time_On_Minute:%d.\n", strip_data.strip_table[0].socket_status.time_on_minute);
    } 
	
	/* Time_OFF_Minute */
	if ((attr_flags & 0x20) == 0x20) {  
		decodeInt16((const char *)(ctrl_data + 4), (short *)&strip_data.strip_table[0].socket_status.time_off_minute);
		NST_SetTimer(Timer_off,timing_value(strip_data.strip_table[0].socket_status.time_off_minute));
		log_info("\tSet Time_OFF_Minute:%d.\n", strip_data.strip_table[0].socket_status.time_off_minute);			
	}
    
	/* CountDown_Minute */
	if ((attr_flags & 0x40) == 0x40) {
		decodeInt16((const char *)(ctrl_data + 6), (short *)&strip_data.strip_table[0].socket_status.countdown_minute);			
		log_info("\tSet CountDown_Minute:%d.\n", strip_data.strip_table[0].socket_status.countdown_minute);			
	}
	               
	log_info("\tSTRIP STATUS: action:0x%x; attr_flags:0x%x; onoff:0x%x.\n", 
			action, attr_flags, ctrl_data[0]);
	log_info("\tsocket:%s, week repeat:%d, timer on:%d, timer off:%d, countDown:%d.\n\n",
			((strip_data.strip_table[0].socket_status.onoff & 0x01) == 0x0) ? "OFF" : "ON",
			strip_data.strip_table[0].socket_status.week_repeat, 
			strip_data.strip_table[0].socket_status.time_on_minute,
			strip_data.strip_table[0].socket_status.time_off_minute,
			strip_data.strip_table[0].socket_status.countdown_minute);

    return 0;
}		/* -----  end of function strip_data_parse  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  strip_start
 *  Description:  Start strip device control. 
 *         Note:
 * =====================================================================================
 */
void strip_start(unsigned char action, unsigned char attr_flags, unsigned char *ctrl_data)
{
    if (strip_data.status_flag == 1) {
        log_info("strip control is running.\n");	 
    }

    strip_data_parse(action, attr_flags, ctrl_data);
	strip_data.status_flag = 1;
}		/* -----  end of function strip_start  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  strip_stop
 *  Description:  Stop strip device control 
 *         Note:
 * =====================================================================================
 */
void strip_stop(void)
{
	int i;
    for (i = 0; i < SOCKET_MAX_NUM; i++) {
        BSP_GPIOSetValue(strip_data.strip_table[0].socket_gpio, 0);
    }
    strip_data.status_flag = 0;
}		/* -----  end of function strip_stop  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  strip_init
 *  Description:  Init smart strip 
 *         Note:
 * =====================================================================================
 */
void strip_init(void)
{
    memset(&strip_data, 0, sizeof(strip_data));

	strip_data.strip_table[0].socket_gpio = STRIP_GPIO_NUM_ONE;
	memcpy(strip_data.strip_table[0].socket_name, STRIP_GPIO_NAME, strlen(STRIP_GPIO_NAME));

	/* Initialize the strip gpio with reg/green/blue/while */
	BSP_GPIOPinMux(STRIP_GPIO_NUM_ONE);		        /* Smart Socket */
	BSP_GPIOSetDir(STRIP_GPIO_NUM_ONE, 1);			/* output */
	BSP_GPIOSetValue(STRIP_GPIO_NUM_ONE, 0);	    /* low level */

	/* init onoff, countdown timer */
	timer_task_init();
}		/* -----  end of function strip_init  ----- */


