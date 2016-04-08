/*
******************************************************************************
**
** Project     : 
** Description :    file for application layer task
** Author      :    pchn                             
** Date        : 
**    
** UpdatedBy   : 
** UpdatedDate : 
**
******************************************************************************
*/
/*
******************************************************************************
**                               Include Files
******************************************************************************
*/
#include "includes.h"

extern int nl6621_gizwits_entry(void);

VOID TestAppMain(VOID * pParam)
{
	/* Giwits source code entry */
	nl6621_gizwits_entry();	  
}

