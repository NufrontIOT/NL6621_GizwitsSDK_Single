#ifndef __STRIP_H__
#define __STRIP_H__

#define SOCKET_MAX_NUM  		          		(1)     /* socket number of strip */
#define SOCKET_STATUS_PRESS_DOWM	        	(1)     /* socket was pressed dowm */
#define SOCKET_STATUS_PRESS_UP     		    	(0)     /* socket was pressed up */

#define STRIP_GPIO_NUM_ONE 				        (30)    /* gpio number of socket on strip */
#define STRIP_GPIO_NAME                         ("SOCKET-one")   /* name for gpio STRIP_GPIO_NUM_ONE */

#pragma pack(1)

typedef struct {
    unsigned char onoff;
    unsigned char week_repeat;
    unsigned short time_on_minute;
    unsigned short time_off_minute;
    unsigned short countdown_minute;
    unsigned short power_consumption;
} socket_status_t;


typedef struct {
	unsigned char socket_gpio;                  /* gpio of socket */ 
	unsigned char socket_name[10];              /* name of socket  */ 
    socket_status_t socket_status;
} socket_def_t;


struct strip_data {
    unsigned char status_flag;                  /* 0:stop; 1:start */
	socket_def_t strip_table[SOCKET_MAX_NUM];
};


#pragma pack()

extern int get_socket_status(unsigned char gpio, socket_status_t *socket_status);

extern void strip_start(unsigned char action, unsigned char attr_flags, unsigned char *ctrl_data);
extern void strip_stop(void);
extern void strip_init(void);

#endif

