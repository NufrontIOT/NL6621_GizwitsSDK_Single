/*
 * =====================================================================================
 *
 *       Filename:  str_lib.c
 *
 *    Description:  Process string function
 *
 *        Version:  0.0.1
 *        Created:  2015/1/6 16:31:02
 *       Revision:  none
 *
 *         Author:  Lin Hui (Link), linhui.568@163.com
 *   Organization:  Nufront
 *
 *--------------------------------------------------------------------------------------          
 *       ChangLog:
 *        version    Author      Date          Purpose
 *        0.0.1      Lin Hui    2015/1/6      Initialize
 *
 * =====================================================================================
 */

/*  
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.  
 * See the copyright notice in the ACK home directory, in the file "Copyright".  
 */   
/* $Id: strncmp.c,v 1.4 1994/06/24 11:57:04 ceriel Exp $ */   

int strncmp(register const char *s1, register const char *s2, register unsigned int n)   
{   
    if (n) {   
        do {   
            if (*s1 != *s2++)   
                break;   
            if (*s1++ == '\0')   
                return 0;   
        } while (--n > 0);   
        if (n > 0) {   
            if (*s1 == '\0') return -1;   
            if (*--s2 == '\0') return 1;   
            return (unsigned char) *s1 - (unsigned char) *s2;   
        }   
    }   
    return 0;   
}  
