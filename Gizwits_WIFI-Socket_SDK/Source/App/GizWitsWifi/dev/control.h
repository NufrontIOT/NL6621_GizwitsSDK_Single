/*
 * =====================================================================================
 *
 *       Filename:  control.h
 *
 *    Description:  Head file for device control
 *
 *        Version:  0.0.1
 *        Created:  2015/1/27 14:19:12
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

#ifndef  __CONTROL_H__
#define  __CONTROL_H__

//#define DEV_LIGHT_MASK
#define DEV_STRIP_MASK

#include "../include/common.h"
#include "light.h"
#include "strip.h"

extern void device_init(void);
extern void device_control(unsigned char action, unsigned char attr_flags, unsigned char *ctrl_data);

#endif   /* ----- #ifndef __CONTROL_H__  ----- */


