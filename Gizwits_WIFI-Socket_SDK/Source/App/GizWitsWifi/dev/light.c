/*
 * =====================================================================================
 *
 *       Filename:  light.c
 *
 *    Description:  smart light control function
 *
 *        Version:  0.0.1
 *        Created:  2015/2/4 10:57:58
 *       Revision:  none
 *
 *         Author:  Lin Hui (Link), linhui.568@163.com
 *   Organization:  Nufront
 *
 *--------------------------------------------------------------------------------------          
 *       ChangLog:
 *        version    Author      Date          Purpose
 *        0.0.1      Lin Hui    2015/2/4      Create and intialize
 *        0.0.2     zhaoweixian 2015/3/2      fix light on or off fail bug
 *
 * =====================================================================================
 */

#include "../include/common.h"
#include "light.h"

static struct led_pwm_data led_data;

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  light_timer_irq_handle
 *  Description:  The handle of timer interrupt for light pwm output 
 *         Note:  Just called by TMR1_IRQHandler.
 * =====================================================================================
 */
void light_timer_irq_handle(void)
{
	int i;

    if (led_data.status_flag == 1) {
        /* restart timer1 */
        BSP_Timer1Init(4);

        /* Check red/green/blue/white led */
		for (i = 0; i < LED_MAX_NUM; i++) {
			if (led_data.led_data.led_table[i].led_pwm > LED_MIN_BRIGHTNESS &&
	            led_data.led_data.led_table[i].led_pwm <= LED_MAX_BRIGHTNESS) 
			{
				if (led_data.timer_cnt == led_data.led_data.led_table[i].led_pwm &&
					led_data.timer_cnt != LED_MAX_BRIGHTNESS) {
					BSP_GPIOSetValue(led_data.led_data.led_table[i].led_gpio, 0);

				} else if (led_data.timer_cnt == 0) {
					BSP_GPIOSetValue(led_data.led_data.led_table[i].led_gpio, 1);
				}	        
	        }else if(led_data.led_data.led_table[i].led_pwm == LED_MIN_BRIGHTNESS){
				if(led_data.timer_cnt == 0){
					BSP_GPIOSetValue(led_data.led_data.led_table[i].led_gpio, 0);
				}
			}	
		}

		if (led_data.timer_cnt >= LED_MAX_BRIGHTNESS) {
	    	led_data.timer_cnt = -1;
	    }
        
		led_data.timer_cnt++;
    }
}		/* -----  end of function light_timer_irq_handle  ----- */



/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  get_light_gpio_status
 *  Description:  Get the PWM value of the gpio 
 *         Note:
 * =====================================================================================
 */
unsigned char get_light_gpio_status(unsigned char gpio)
{
	if (gpio == SOCKET_GPIO_NUM) {
		return led_data.socket;

	} else if (gpio == LED_RED_GPIO_NUM) {
        return led_data.led_data.led_table[0].led_pwm;
    
    } else if (gpio == LED_GREEN_GPIO_NUM) {
        return led_data.led_data.led_table[1].led_pwm;
    
    } else if (gpio == LED_BLUE_GPIO_NUM) {
        return led_data.led_data.led_table[2].led_pwm;
    
    } else if (gpio == LED_WHITE_GPIO_NUM) {
        return led_data.led_data.led_table[3].led_pwm;
    }

    return 0;
}		/* -----  end of function get_light_gpio_status  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  light_data_parse
 *  Description:  Parse P0 data come from cloud server, if the bit is not set, reload the
 *  			current data to struct led_status structure.
 *   Parameters:
 *         Note:
 * =====================================================================================
 */
int light_data_parse(
		unsigned char action, 
		unsigned char attr_flags, 
        unsigned char *ctrl_data)
{
    
    /* Control socket */
    if ((attr_flags & 0x01) == 0x01) {
        led_data.socket = ctrl_data[0] & 0x1;	         
        if ((ctrl_data[0] & 0x1) == 0x1) {
			BSP_GPIOSetValue(SOCKET_GPIO_NUM, 1);		 
        } else {
        	BSP_GPIOSetValue(SOCKET_GPIO_NUM, 0); 	
        }
		log_info("Set Socket level: %d\n", led_data.socket);
    }

    /* get the valid data in ctrl_data buffer */
    if ((attr_flags & 0x02) == 0x02) {         
        led_data.led_data.led_table[0].led_pwm = ctrl_data[1];
		log_info("Set RED: %d\n", led_data.led_data.led_table[0].led_pwm);
    }
    
    if ((attr_flags & 0x04) == 0x04) {        
        led_data.led_data.led_table[1].led_pwm = ctrl_data[2];
		log_info("Set GREEN: %d\n", led_data.led_data.led_table[1].led_pwm);
    }

    if ((attr_flags & 0x08) == 0x08) {        
        led_data.led_data.led_table[2].led_pwm = ctrl_data[3];
		log_info("Set BLUE: %d\n", led_data.led_data.led_table[2].led_pwm);
    }
    
    if ((attr_flags & 0x10) == 0x10) {        
        led_data.led_data.led_table[3].led_pwm = ctrl_data[4];
		log_info("Set WHITE: %d\n", led_data.led_data.led_table[3].led_pwm);
    }

	log_info("\nAfter data: action:0x%x; attr_flags:0x%x; RED:%d, GREEN:%d, BLUE:%d, WHITE:%d.\n\n", 
			action, attr_flags, 
			led_data.led_data.led_table[0].led_pwm, 
			led_data.led_data.led_table[1].led_pwm,	
			led_data.led_data.led_table[2].led_pwm, 
			led_data.led_data.led_table[3].led_pwm);

    return 0;
}		/* -----  end of function light_data_parse  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  light_start
 *  Description:  Start light PWM control. 
 *         Note:
 * =====================================================================================
 */
void light_start(unsigned char action, 
		unsigned char attr_flags, 
        unsigned char *ctrl_data)
{
    if (led_data.status_flag == 1) {
        log_info("Light PWM output is running.\n");	 
    }

	led_data.status_flag = 0;
	OSTimeDly(1);
    light_data_parse(action, attr_flags, ctrl_data);
	OSTimeDly(1); 
    led_data.status_flag = 1;    

    /* Start hard timer1 to output LED PWM signal */
    BSP_Timer1Init(4);
}		/* -----  end of function light_start  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  light_stop
 *  Description:  Stop light PWM output 
 *         Note:
 * =====================================================================================
 */
void light_stop(void)
{
	int i;
    for (i = 0; i < LED_MAX_NUM; i++) {
        BSP_GPIOSetValue(led_data.led_data.led_table[i].led_gpio, 0);
    }
    led_data.status_flag = 1;
}		/* -----  end of function light_stop  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  light_init
 *  Description:  Init smart light 
 *         Note:
 * =====================================================================================
 */
void light_init(void)
{
    memset(&led_data, 0, sizeof(led_data));

	led_data.led_data.led_table[0].led_gpio = LED_RED_GPIO_NUM;
	led_data.led_data.led_table[1].led_gpio = LED_GREEN_GPIO_NUM;
	led_data.led_data.led_table[2].led_gpio = LED_BLUE_GPIO_NUM;
	led_data.led_data.led_table[3].led_gpio = LED_WHITE_GPIO_NUM;

	/* Initialize the Light gpio with reg/green/blue/while */
	BSP_GPIOPinMux(SOCKET_GPIO_NUM);		    /* Smart Socket */
	BSP_GPIOSetDir(SOCKET_GPIO_NUM, 1);			/* output */
	BSP_GPIOSetValue(SOCKET_GPIO_NUM, 0);	    /* low level */

	BSP_GPIOPinMux(LED_RED_GPIO_NUM);		    /* RED */
	BSP_GPIOSetDir(LED_RED_GPIO_NUM, 1);		/* output */
	BSP_GPIOSetValue(LED_RED_GPIO_NUM, 0);	    /* low level */

	BSP_GPIOPinMux(LED_GREEN_GPIO_NUM);			/* GREEN */
	BSP_GPIOSetDir(LED_GREEN_GPIO_NUM, 1);		/* output */
	BSP_GPIOSetValue(LED_GREEN_GPIO_NUM, 0);	/* low level */

	BSP_GPIOPinMux(LED_BLUE_GPIO_NUM);			/* BLUE */
	BSP_GPIOSetDir(LED_BLUE_GPIO_NUM, 1);		/* output */
	BSP_GPIOSetValue(LED_BLUE_GPIO_NUM, 0);	    /* low level */

	BSP_GPIOPinMux(LED_WHITE_GPIO_NUM);			/* WHITE */
	BSP_GPIOSetDir(LED_WHITE_GPIO_NUM, 1);		/* output */
	BSP_GPIOSetValue(LED_WHITE_GPIO_NUM, 0);	/* low level */

}		/* -----  end of function light_init  ----- */


