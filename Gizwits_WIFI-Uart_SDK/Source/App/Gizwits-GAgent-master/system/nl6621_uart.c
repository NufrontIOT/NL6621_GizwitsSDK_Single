/*
 * =====================================================================================
 *
 *       Filename:  nl6621_uart.c
 *
 *    Description:  nl6621 uart driver file
 *
 *        Version:  0.0.1
 *        Created:  2015/7/1 13:30:44
 *       Revision:  none
 *
 *         Author:  Lin Hui (Link), linhui.568@163.com
 *   Organization:  Nufront
 *
 *--------------------------------------------------------------------------------------          
 *       ChangLog:
 *        version    Author      Date          Purpose
 *        0.0.1      Lin Hui    2015/7/1      
 *
 * =====================================================================================
 */
#include "common.h"
#include "nl6621_uart.h"
#include "../lib/ring_buffer.h"

ring_buffer_t uartrxbuf;

NST_TIMER * recv_timer;	/* receive timer */
OS_EVENT * recv_sem;	/* receive sem */
NST_LOCK * recv_lock;	/* receive lock */

/* store data receive from uart, user can change the data store address.
 * */
char recv_data[1024];


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  uart_data_recv
 *  Description:  Interface of uart data receive. 
 *         Note:
 * =====================================================================================
 */
void uart_data_recv(unsigned char Dummy)
{
	ring_buf_write_char(&uartrxbuf, (char)Dummy);
//	printf("**************************************uart_data_recv\r\n");			
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  zigbee_recv_timer_handle
 *  Description:  When soft timer coming ervery 50ms, check ring buffer and send to 
 *                remote server. 
 *         Note:
 * =====================================================================================
 */
void uart_recv_timer_handle(void *ptmr, void *parg)
{
	if (ring_buf_cnt(&uartrxbuf) > 0) {
		OSSemPost(recv_sem);
	}	    
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  uart_init
 *  Description:  Init uart resource, sended and received   
 *         Note:
 * =====================================================================================
 */
void uart_init(void)
{
	/* Init uart receive ring buffer */
	ring_buf_alloc(&uartrxbuf, UART_RECV_BUF_SIZE);

	memset(recv_data, 0x0, sizeof(recv_data));

	recv_sem = OSSemCreate(0);
	NST_ALLOC_LOCK(&recv_lock);

	/* register receive timer */
//    NST_InitTimer(&recv_timer, uart_recv_timer_handle, NULL, NST_TRUE);

	/* init system device */	
//	simu_uart_init();

}		/* -----  end of function uart_init  ----- */

