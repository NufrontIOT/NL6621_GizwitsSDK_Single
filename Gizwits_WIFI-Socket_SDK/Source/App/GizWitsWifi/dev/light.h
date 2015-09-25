#ifndef __LIGHT_H__
#define __LIGHT_H__

#define LED_MAX_NUM  				(4)
#define LED_MIN_BRIGHTNESS			(0)
#define LED_MAX_BRIGHTNESS			(255)

#define SOCKET_GPIO_NUM				(30)

#define LED_RED_GPIO_NUM			(6)
#define LED_GREEN_GPIO_NUM			(7)
#define LED_BLUE_GPIO_NUM			(5)
#define LED_WHITE_GPIO_NUM			(8)

#pragma pack(1)

typedef struct {
	unsigned char led_gpio;                  /* the pwm output gpio */ 
	unsigned char led_pwm;                  /* limit: 0 to 255 percent */ 
} led_status_t;

struct led_status {
	led_status_t led_table[LED_MAX_NUM]; 
};

struct led_pwm_data {
	unsigned char socket;
    struct led_status led_data;
	unsigned char status_flag;		        /* 0:stop; 1:start */
    int timer_cnt;     			            /* store the count of current timer */
};

#pragma pack()

extern void light_timer_irq_handle(void);
extern unsigned char get_light_gpio_status(unsigned char gpio);

extern void light_start(unsigned char action, 
		unsigned char attr_flags, 
        unsigned char *ctrl_data);

extern void light_stop(void);
extern void light_init(void);

#endif

