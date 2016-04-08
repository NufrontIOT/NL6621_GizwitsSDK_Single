/*
 * =====================================================================================
 *
 *       Filename:  control.c
 *
 *    Description:  device control implementation file
 *
 *        Version:  0.0.1
 *        Created:  2015/1/27 14:14:07
 *       Revision:  none
 *
 *         Author:  Lin Hui (Link), linhui.568@163.com
 *   Organization:  Nufront
 *
 *--------------------------------------------------------------------------------------          
 *       ChangLog:
 *        version    Author      Date          Purpose
 *        0.0.1      Lin Hui    2015/1/27      
 *           
 * =====================================================================================
 */

#include "control.h"



void device_control(unsigned char action, unsigned char attr_flags, unsigned char *ctrl_data)
{
#ifdef DEV_LIGHT_MASK
    light_start(action, attr_flags, ctrl_data);
#endif

#ifdef DEV_STRIP_MASK
    strip_start(action, attr_flags, ctrl_data);
#endif
}


void device_init(void)
{
#ifdef DEV_LIGHT_MASK
    light_init();
#endif

#ifdef DEV_STRIP_MASK
    strip_init();
#endif
}


