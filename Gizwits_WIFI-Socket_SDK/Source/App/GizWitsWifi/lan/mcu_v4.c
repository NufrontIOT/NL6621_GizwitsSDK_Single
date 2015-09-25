#ifdef  __cplusplus
extern "C"{
#endif

#include "../include/common.h"

pro_commonCmd 	m_pro_commonCmd;
m2w_mcuStatus	m_m2w_mcuStatus; //当前最新的mcu状态帧

extern int g_SocketLogin[8]; 

int GAgent_GetMCUInfo( u32 Time )
{
    memcpy( g_Xpg_GlobalVar.Xpg_Mcu.protocol_ver, "00000001", 8 );
    g_Xpg_GlobalVar.Xpg_Mcu.protocol_ver[8] = '\0';

    memcpy( g_Xpg_GlobalVar.Xpg_Mcu.p0_ver, "00000004", 8 );
    g_Xpg_GlobalVar.Xpg_Mcu.p0_ver[8] = '\0';

    memcpy( g_Xpg_GlobalVar.Xpg_Mcu.hard_ver, "00000001", 8 );
    g_Xpg_GlobalVar.Xpg_Mcu.hard_ver[8] = '\0';

    memcpy( g_Xpg_GlobalVar.Xpg_Mcu.soft_ver, "00000001", 8 );
    g_Xpg_GlobalVar.Xpg_Mcu.soft_ver[8] = '\0';

    memcpy( g_Xpg_GlobalVar.Xpg_Mcu.product_key, "9877965c9cf04dff9a585e5f8d77dedb", 32 );
    g_Xpg_GlobalVar.Xpg_Mcu.product_key[32] = '\0';
    g_productKeyLen = 32;

    log_notice("MCU Protocol Version:%s.\n", g_Xpg_GlobalVar.Xpg_Mcu.protocol_ver);
    log_notice("MCU P0 Version:%s.\n", g_Xpg_GlobalVar.Xpg_Mcu.p0_ver);
    log_notice("MCU Hard Version:%s.\n", g_Xpg_GlobalVar.Xpg_Mcu.hard_ver);
    log_notice("MCU Soft Version:%s.\n", g_Xpg_GlobalVar.Xpg_Mcu.soft_ver);
    log_notice("MCU Product Version:%s.\n", g_Xpg_GlobalVar.Xpg_Mcu.product_key);

    return 0;
}


/******************************************************
*
*       Re_buf      : uart receive data pointer,
*       Pload       :   uart receive playload data pointer
*       flag        :   0:cmd payload
                        1:debug payload;
*       return      :   the playload length
*       Add by Alex lin  2014-03-20
*
*******************************************************/
int get_uart_Pload( u8* Re_buf,u8* Pload,int Plen,unsigned char  Cmd )
{
    if( Cmd==0x91)
        memcpy( Pload,Re_buf+8,Plen );
    else
        memcpy( Pload,Re_buf,Plen );

    return Plen;
}

/**********************************************************
*
*               Function     :      add wifi to phone head
*               buf          :      uart receive buf
*               PHead        :      the head to phone
*               return       :      the head length
*               Add by Alex     2014-04-21
***********************************************************/
int Add_W2PHead( short ULen,u8*PHead,unsigned char Cmd )
{
    char W2PHead[100]= {0};
    short packLen;
    int HeadLen;
    varc Uart_varatt;
    int i;

    packLen = ULen;
    //packLen = packLen-4+1;
    Uart_varatt = Tran2varc   (packLen);
    W2PHead[0] = 0x00;
    W2PHead[1] = 0x00;
    W2PHead[2] = 0x00;
    W2PHead[3] = 0x03;
    for(i=0;i<Uart_varatt.varcbty;i++)
    {
        W2PHead[4+i] = Uart_varatt.var[i];
    }
    W2PHead[4+Uart_varatt.varcbty] = 0x00;
    W2PHead[4+Uart_varatt.varcbty+1] = 0x00;
    W2PHead[4+Uart_varatt.varcbty+2] = Cmd;
    HeadLen = 4+Uart_varatt.varcbty+2+1;
    memcpy(PHead, (unsigned char*)W2PHead, HeadLen);
    return HeadLen;
}


/***************************************************************************************
*
*               FUNCTION :  send mcu cmd to phone
*               uartbuf  :  uart receive buf
*               fd       :  fd>0    :   socketid
                            0       :   send data to all phone
*               Add by Alex 2014-04-21
*
*****************************************************************************************/
void Dev_SendPacket2Phone(  u8* uartbuf, int uartbufLen, unsigned char Cmd, int fd )
{
    u8 HPhead[100];
    int socketId;
    int HPheadLen=0;
    u8 *SendPack=NULL;
    int SendPackLen=0;
    int PLen=0;
    short ULen;
    char* Pload=NULL;
    int i;

    if ( Cmd == 0x91 ) {
        PLen = uartbuf[2]*256+uartbuf[3]-5;
    } else if ( Cmd == 0x12 ) {
        PLen = uartbufLen;
    }

    ULen = PLen+3;
    Pload = (char *)mem_malloc( PLen );
    if (Pload == NULL) {
        return;
    }
    memset(Pload, 0, PLen);
    socketId = fd;

    get_uart_Pload( uartbuf, (unsigned char*)Pload, PLen, Cmd);
    HPheadLen = Add_W2PHead( ULen, HPhead, Cmd );

    SendPackLen = HPheadLen+PLen;
    SendPack = (u8*)mem_malloc(SendPackLen);
    if (SendPack == NULL) {
        mem_free(Pload);
        return;
    }

    memcpy(SendPack, HPhead, HPheadLen);
    memcpy(SendPack + HPheadLen, (unsigned char*)Pload, PLen);


    if ( fd > 0 ) {
        for ( i = 0; i < 8; i++ ) {
            if ( socketId == g_SocketLogin[i] )
            {
                send(socketId, SendPack, SendPackLen, 0);
                break;
            }
        }
    } else {
        Socket_SendData2Client( SendPack,SendPackLen,Cmd );
    }
    mem_free(SendPack) ;
    mem_free(Pload);

    return;
}

/**************************************************************************
*
*   Function    :   MCU_SendPublishPacket()
*                   send data to cloud
*   uartbuf     :   uart receive data
*   uartbufLen  :   uart receive data len
*
**************************************************************************/
int MCU_SendPublishPacket(u8 *uartbuf,int uartbufLen )
{
    varc SendVarc;
    u8 *SendPack=NULL;
    int SendPackLen=0;
    int UartP0Len=0;
    u8 McuSendTopic[200]={0};
    short VarLen=0;
    int i;

    if ( g_MQTTStatus != MQTT_STATUS_RUNNING )
    {
        log_info("MCU_SendPublishPacket:%d\n", g_MQTTStatus);
        return  1;
    }

    //head(2b)+len(2b)+sn(1B)+cmd(1B)+flags(2b)+p0
    if( uartbuf[0] == 0xff && uartbuf[1]==0xff )
	{
		UartP0Len = uartbufLen-8;
 	}
    else
	{
		return 1;
	}
    memcpy( McuSendTopic,"dev2app/", strlen("dev2app/"));
    memcpy( McuSendTopic+strlen("dev2app/"), (unsigned char*)g_Xpg_GlobalVar.DID, strlen(g_Xpg_GlobalVar.DID));
    McuSendTopic[strlen("dev2app/")+strlen(g_Xpg_GlobalVar.DID)]='\0';

    //protocolVer(4B)+varLen(1~4B)+flag(1B)+cmd(2B)+P0
    VarLen=1+2+UartP0Len;

    SendVarc = Tran2varc(VarLen);
    SendPackLen = 4+SendVarc.varcbty+1+2+UartP0Len;

    SendPack = (u8*)mem_malloc(SendPackLen);
    if( SendPack == NULL ) return 1;

    //protocolVer
    SendPack[0] = 0x00;
    SendPack[1] = 0x00;
    SendPack[2] = 0x00;
    SendPack[3] = 0x03;
    //varLen
    for (i=0; i<SendVarc.varcbty; i++)
	{
		SendPack[4+i] = SendVarc.var[i];
	}
     //flag
    SendPack[4+SendVarc.varcbty] = 0x00;
    //CMD
    SendPack[4+SendVarc.varcbty+1]=0x00;
    SendPack[4+SendVarc.varcbty+2]=0x91;
    //p0

    memcpy( (SendPack+4+SendVarc.varcbty+3),uartbuf+8,UartP0Len);
    log_info("MCU Send Topic : %s\r\n", McuSendTopic);

    PubMsg( &g_stMQTTBroker, (const char*)McuSendTopic, (char*)SendPack, SendPackLen,0);
    mem_free( SendPack );

    return 0;
}


void SendCtlAckCmd(uint8_t cmd, uint8_t sn)
{
	short data_len;

	memset(&m_pro_commonCmd, 0, sizeof(pro_commonCmd));
	decodeInt16((const char *)5, &data_len);

	m_pro_commonCmd.head_part.head[0] = 0xFF;
	m_pro_commonCmd.head_part.head[1] = 0xFF;
	m_pro_commonCmd.head_part.len = data_len;
	m_pro_commonCmd.head_part.cmd = cmd;
	m_pro_commonCmd.head_part.sn = sn;
	m_pro_commonCmd.sum = GAgent_SetCheckSum((uint8_t *)&m_pro_commonCmd, sizeof(pro_commonCmd));

	Dev_SendPacket2Phone( &m_pro_commonCmd, sizeof(pro_commonCmd) + 1, 0x91,0 );
}

/*******************************************************************************
* Function Name  : ReportStatus
* Description    : 上报mcu状态
* Input          : tag=REPORT_STATUS，主动上报，使用CMD_SEND_MODULE_P0命令字；
				   tag=REQUEST_STATUS，被动查询，使用CMD_SEND_MCU_P0_ACK命令字；
* Output         : None
* Return         : None
* Attention		   : None
*******************************************************************************/
void ReportStatus(uint8_t tag, unsigned char SN)
{
	if(tag == REPORT_STATUS)
	{
		m_m2w_mcuStatus.head_part.head[0] = 0xff;
		m_m2w_mcuStatus.head_part.head[1] = 0xff;
		
#ifdef DEV_STRIP_MASK
		m_m2w_mcuStatus.head_part.len = htons(16);
#endif

#ifdef DEV_LIGHT_MASK
		m_m2w_mcuStatus.head_part.len = htons(11);
#endif

		m_m2w_mcuStatus.head_part.cmd = CMD_SEND_MODULE_P0;
		m_m2w_mcuStatus.head_part.sn = ++SN;
		m_m2w_mcuStatus.head_part.flags[0] = 0x0;
		m_m2w_mcuStatus.head_part.flags[1] = 0x0;
		m_m2w_mcuStatus.sub_cmd = SUB_CMD_REPORT_MCU_STATUS;

#ifdef DEV_LIGHT_MASK
		m_m2w_mcuStatus.status_w.socket = get_light_gpio_status(SOCKET_GPIO_NUM);
		m_m2w_mcuStatus.status_w.led_r = get_light_gpio_status(LED_RED_GPIO_NUM);
		m_m2w_mcuStatus.status_w.led_g = get_light_gpio_status(LED_GREEN_GPIO_NUM);
		m_m2w_mcuStatus.status_w.led_b = get_light_gpio_status(LED_BLUE_GPIO_NUM);
		m_m2w_mcuStatus.status_w.led_w = get_light_gpio_status(LED_WHITE_GPIO_NUM);
#endif

#ifdef DEV_STRIP_MASK
		get_socket_status(STRIP_GPIO_NUM_ONE, (socket_status_t *)&m_m2w_mcuStatus.status_w);
		decodeInt16((const char *)&m_m2w_mcuStatus.status_w.time_on_minute,(short *)&m_m2w_mcuStatus.status_w.time_on_minute);
		decodeInt16((const char *)&m_m2w_mcuStatus.status_w.time_off_minute,(short *)&m_m2w_mcuStatus.status_w.time_off_minute);
		decodeInt16((const char *)&m_m2w_mcuStatus.status_w.countdown_minute,(short *)&m_m2w_mcuStatus.status_w.countdown_minute);
#endif

		m_m2w_mcuStatus.sum = GAgent_SetCheckSum((uint8_t *)&m_m2w_mcuStatus, sizeof(m2w_mcuStatus));
	}
	else if(tag == REQUEST_STATUS)
	{
		m_m2w_mcuStatus.head_part.head[0] = 0xff;
		m_m2w_mcuStatus.head_part.head[1] = 0xff;
		m_m2w_mcuStatus.head_part.len = htons(11);
		m_m2w_mcuStatus.head_part.cmd = CMD_SEND_MCU_P0_ACK;
		m_m2w_mcuStatus.head_part.sn = ++SN;
		m_m2w_mcuStatus.head_part.flags[0] = 0x0;
		m_m2w_mcuStatus.head_part.flags[1] = 0x0;
		m_m2w_mcuStatus.sub_cmd = SUB_CMD_REQUIRE_STATUS_ACK;

#ifdef DEV_LIGHT_MASK
		m_m2w_mcuStatus.status_w.socket = get_light_gpio_status(SOCKET_GPIO_NUM);
		m_m2w_mcuStatus.status_w.led_r = get_light_gpio_status(LED_RED_GPIO_NUM);
		m_m2w_mcuStatus.status_w.led_g = get_light_gpio_status(LED_GREEN_GPIO_NUM);
		m_m2w_mcuStatus.status_w.led_b = get_light_gpio_status(LED_BLUE_GPIO_NUM);
		m_m2w_mcuStatus.status_w.led_w = get_light_gpio_status(LED_WHITE_GPIO_NUM);
#endif

#ifdef DEV_STRIP_MASK
		get_socket_status(STRIP_GPIO_NUM_ONE, (socket_status_t *)&m_m2w_mcuStatus.status_w);
#endif

		m_m2w_mcuStatus.sum = GAgent_SetCheckSum((uint8_t *)&m_m2w_mcuStatus, sizeof(m2w_mcuStatus));
	}

#ifdef DEV_LIGHT_MASK
	log_info("Report data: Socket:%d, RED:%d, GREEN:%d, BLUE:%d, WHITE:%d.\n",
			m_m2w_mcuStatus.status_w.socket, 
			m_m2w_mcuStatus.status_w.led_r,
			m_m2w_mcuStatus.status_w.led_g,
			m_m2w_mcuStatus.status_w.led_b,
			m_m2w_mcuStatus.status_w.led_w);
#endif

#ifdef DEV_STRIP_MASK
	log_info("\tOnOff:0x%x, week repeat:%d, timer on:%d, timer off:%d, countDown:%d, power consumption:%d.\n\n",
			m_m2w_mcuStatus.status_w.onff,
			m_m2w_mcuStatus.status_w.week_repeat,
			m_m2w_mcuStatus.status_w.time_on_minute,
			m_m2w_mcuStatus.status_w.time_off_minute,
			m_m2w_mcuStatus.status_w.countdown_minute,
			m_m2w_mcuStatus.status_w.power_consumption);
#endif

}


void ResponseDevStatus(int fd)
{
	ReportStatus(REPORT_STATUS, 0);
	Dev_SendPacket2Phone( &m_m2w_mcuStatus, sizeof(m2w_mcuStatus) + 1, 0x91,0 );

	/* Response the device status to cloud server */
	MCU_SendPublishPacket(&m_m2w_mcuStatus, sizeof(m2w_mcuStatus));
}


int GAgentV4_CtlDev_with_p0(int fd, unsigned char cmd, unsigned char *data, unsigned short data_len)
{
    int ret = 0;

    if (data_len > (4 * 1024))
    {
        log_info("%s:%d P0 is too long[%08x]\n", __FUNCTION__, __LINE__, data_len);
        return -1;

    } else {
		if (data[1] && 0xff != 0x0) {
			/* Control Device */
			log_info("Start to control Device.action:0x%x, attr_flags:0x%x.\n", data[0], data[1]);
		
			device_control(data[0], data[1], (unsigned char*)(data + 2));
		}
    }

    return ret;
}


#ifdef  __cplusplus
}
#endif /* end of __cplusplus */

