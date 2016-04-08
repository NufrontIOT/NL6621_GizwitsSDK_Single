#ifdef  __cplusplus
extern "C"{
#endif


#include "../include/common.h"
/***********          LOCAL OTA                   ************/
#define BOOT_PARAM_DATA_ADDR	(0x33000)
#define OTA_FW_BAK_OFFSET		(0x34000)
UINT8 OtaCtrlBuf[OTA_CTRL_BUF_SIZE];
UINT8 OtaDataBuf[FLASH_SECTOR_SIZE]; 
UINT32 WritePtr;
UINT32 ReadPtr;
UINT32 upgradeFlag = 0;
UINT32 fileSize = 0;
UINT32 recvFileSize = 0;
UINT32 upgradeSector = 0;
SOCKET_INFO upgrade_socket_info;
void FwUpgradeProcess(UINT8 * cmd_pkt, unsigned short cmd_len, SOCKET_INFO* sock_info);
int DevSendCtrlMsg(UINT8 * buf, int len, SOCKET_INFO* sock_info);
static void FwUpgradeFinishAck(void);




/***********          LOCAL MACRO               ***********/
#define XPG_PACKET_VERSIONLEN 4
#define SOCKET_MAXTCPCLENT 8
#define LAN_CLIENT_MAXLIVETIME 12   /*12S, timeout*/

/***********          LOCAL VARIABLES           ***********/
typedef struct SocketClent
{
    int SocketFD;
    struct sockaddr_in TCPClientAddr;
    int TimeOut;
} SOCKET_CLIENT_S;
static SOCKET_CLIENT_S g_stTCPSocketClients[SOCKET_MAXTCPCLENT];

/***********          GLOBAL VARIABLES           ***********/
u8 *g_stSocketRecBuffer=NULL;
int g_TCPServerFd = -1;
int g_UDPServerFd = -1;
int g_UDPBroadcastServerFd=-1;
int g_TCPSocketMaxClientFd=0;
struct sockaddr_in g_stUDPBroadcastAddr;//用于UDP广播

void Socket_ResetClientTimeout(int socketfd)
{
    int i;

    for (i = 0; i < SOCKET_MAXTCPCLENT; i++)
    {
        if (g_stTCPSocketClients[i].SocketFD == socketfd)
        {
            g_stTCPSocketClients[i].TimeOut = LAN_CLIENT_MAXLIVETIME;
            break;
        }
    }
    return;
}

void Socket_ClientTimeoutTimer(void)
{
    int i,j;

    for (i = 0; i < SOCKET_MAXTCPCLENT; i++)
    {
        if (g_stTCPSocketClients[i].TimeOut > 0)
        {
            g_stTCPSocketClients[i].TimeOut --;

            if (g_stTCPSocketClients[i].TimeOut ==0)
            {
				if(g_stTCPSocketClients[i].SocketFD > 0)
				{
	                GAgent_Printf(GAGENT_DEBUG,
	                        "Close socket(%d) now[time:%08x second: %08x MS] ",
	                        g_stTCPSocketClients[i].SocketFD, nl6621_GetTime_S(),
	                        nl6621_GetTime_MS());
	                close(g_stTCPSocketClients[i].SocketFD);
//					g_stTCPSocketClients[i].SocketFD = 0;

	                for( j=0;j<8;j++ )
	                {
	                    if(g_SocketLogin[j]==g_stTCPSocketClients[i].SocketFD)
	                        g_SocketLogin[j]=0;
	                }
	                g_stTCPSocketClients[i].SocketFD = -1;
				}

            }
        }
    }

    return;
}


void Init_Service(void)
{
    int i;

    memset((void *)&g_stTCPSocketClients, 0x0, sizeof(g_stTCPSocketClients));

    for (i = 0; i < SOCKET_MAXTCPCLENT; i++)
    {
        g_stTCPSocketClients[i].SocketFD = -1;
        g_stTCPSocketClients[i].TimeOut = 0;
    }

    g_stSocketRecBuffer = (unsigned char *)mem_malloc(SOCKET_RECBUFFER_LEN);
    if (g_stSocketRecBuffer == NULL)
    {
        /**/
        ;
    }
    GAgent_CreateTimer(GAGENT_TIMER_PERIOD, 1000, Socket_ClientTimeoutTimer);
    return;
}


/***********************************************
 *
 *   UdpFlag:    0 respond udp discover
 *               1 wifi send udp broadcast
 *
 ***********************************************/
void Build_BroadCastPacket(  int UdpFlag, u8* Udp_Broadcast )
{
    int i;
    int TempFirmwareVerLen=0;
    int len;

    TempFirmwareVerLen = g_stGAgentConfigData.FirmwareVerLen[1];
    if ( (TempFirmwareVerLen > 32) || (TempFirmwareVerLen <= 0) )
    {
        //当固件版本信息长度超出规定范围 hard core一个版本信息。
        Udp_Broadcast[9+Udp_Broadcast[9]+2+8] = 4;
        TempFirmwareVerLen=4;
        memcpy( (unsigned char*)g_stGAgentConfigData.FirmwareVer,"V1.0",4);
    }

    //protocolver
    Udp_Broadcast[0]=0x00;
    Udp_Broadcast[1]=0x00;
    Udp_Broadcast[2]=0x00;
    Udp_Broadcast[3]=0x03;

    //varlen =flag(1b)+cmd(2b)+didlen(2b)+did(didLen)+maclen(2b)+mac+firwareverLen(2b)+firwarever+2+productkeyLen
    Udp_Broadcast[4] =1+2+2+DIdLen+2+6+2+TempFirmwareVerLen+2+g_productKeyLen;
    //flag
    Udp_Broadcast[5] = 0x00;
    //cmd
    if( UdpFlag==1 )
    {
        Udp_Broadcast[6] = 0x00;
        Udp_Broadcast[7] = 0x05;
    }
    if( UdpFlag==0 )
    {
        Udp_Broadcast[6] = 0x00;
        Udp_Broadcast[7] = 0x04;
    }
    // didlen
    Udp_Broadcast[8]=0x00;
    Udp_Broadcast[9]=DIdLen;
    //did
    for(i=0;i<DIdLen;i++)
    {
        Udp_Broadcast[10+i]=g_Xpg_GlobalVar.DID[i];
    }
    //maclen
    Udp_Broadcast[9+Udp_Broadcast[9]+1]=0x00;
    Udp_Broadcast[9+Udp_Broadcast[9]+2]=0x06;//macLEN
    //mac
    for(i=0;i<6;i++)
    {
        Udp_Broadcast[9+Udp_Broadcast[9]+3+i] = Service_mac[i];
    }

    //firmwarelen
    Udp_Broadcast[9+Udp_Broadcast[9]+2+7]=0x00;
    Udp_Broadcast[9+Udp_Broadcast[9]+2+8]=TempFirmwareVerLen;//firmwareVerLen
    //firmware
    memcpy( (unsigned char*)&Udp_Broadcast[9+Udp_Broadcast[9]+2+9], (unsigned char*)g_stGAgentConfigData.FirmwareVer,TempFirmwareVerLen );
    len = 9+Udp_Broadcast[9]+2+8+TempFirmwareVerLen+1;
    //productkeylen
    Udp_Broadcast[len]=0x00;
    Udp_Broadcast[len+1]=g_productKeyLen;
    len=len+2;
    memcpy( &Udp_Broadcast[len],g_Xpg_GlobalVar.Xpg_Mcu.product_key,32 );
    len=len+g_productKeyLen;
}


/*****************************************
 *
 *   time    :   send broadcast time
 *
 *******************************************/
void Socket_SendBroadCastPacket( int time )
{
    int i;
    int ret;
    u8 Udp_Broadcast[200];

    if (g_UDPBroadcastServerFd == -1)
    {
        return;
    }

    Build_BroadCastPacket( 1,Udp_Broadcast );

    for( i=0;i<time;i++)
    {
        if(g_UDPBroadcastServerFd>-1)
        {
            ret = Socket_sendto(g_UDPBroadcastServerFd, Udp_Broadcast,Udp_Broadcast[4]+5, &g_stUDPBroadcastAddr, sizeof(g_stUDPBroadcastAddr));
            GAgent_Printf(GAGENT_DEBUG, "Socket_SendBroadCastPacket, sendto ret %d", ret);
        }
    }
    return;
}


int Socket_CheckNewTCPClient(void)
{
    fd_set readfds, exceptfds;
    int ret,bytes_received;
    int newClientfd;
    int i;
    struct sockaddr_in addr;
    struct timeval t;
    socklen_t addrlen= sizeof(addr);
	BOOT_PARAM param;

    t.tv_sec = 0;//秒
    t.tv_usec = 0;//微秒
    /*Check status on erery sockets */
    FD_ZERO(&readfds);
    FD_SET(g_TCPServerFd, &readfds);

    ret = select((g_TCPServerFd+1), &readfds, NULL, &exceptfds, &t);
    if (ret > 0)
    {
        newClientfd = Socket_accept(g_TCPServerFd, &addr, &addrlen);
        GAgent_Printf(GAGENT_DEBUG, "detected new client as %d", newClientfd);
        if (newClientfd > 0)
        {
            if(g_TCPSocketMaxClientFd < newClientfd)
            {
                g_TCPSocketMaxClientFd = (newClientfd + 1); /*Select接口要求+1*/
            }

/****************************Check Socket Ota data**************************************/
			if (upgradeFlag & UPGRADE_START)
			{
			
				 while (1)
				{
			            /*B:handle fw upgrade*/
			            if (upgradeFlag & UPGRADE_START)
			            {
			                int i = 0;
			                
			                if (!(upgradeFlag & UPGRADE_DOING))
			                {
			                    WritePtr = 0;
			                    recvFileSize = 0;
			                    upgradeFlag |= UPGRADE_DOING;    
			                }
			
							for( i = 0; i < SOCKET_MAXTCPCLENT; i++)
							{
								if(g_stTCPSocketClients[i].SocketFD > 0)
								{
									close(g_stTCPSocketClients[i].SocketFD);
								}
							}
							
			
			                /* 从connected socket中接收数据，接收buffer是1024大小，但并不一定能够收到1024大小的数据 */
			                bytes_received = recv(newClientfd, &OtaDataBuf[WritePtr], FLASH_SECTOR_SIZE - WritePtr, 0);
			                if (bytes_received <= 0)
			                {
			                    if (bytes_received == 0)
			                        DBGPRINT_LWIP(DEBUG_ERROR, "client close connection.\n");
			                    else
			                        DBGPRINT_LWIP(DEBUG_ERROR, "received failed, server close connection.\n");
			                    
			                    /* 接收失败，关闭这个connected socket */
			                    lwip_close(newClientfd);
			                    upgradeFlag = 0;    
			                    return -1;
			                }
			
			                recvFileSize += bytes_received;
			                WritePtr += bytes_received;

#ifdef	DUAL_LAUNCHER			
			                if (recvFileSize == (fileSize - 256))
			                {
			                    upgradeFlag |= UPGRADE_OKFILE;
			                    /*写最后一个sector*/
			                    if (QSpiWriteOneSector(OTA_FW_BAK_OFFSET + upgradeSector * FLASH_SECTOR_SIZE, OtaDataBuf) != 0)
			                    {
			                        lwip_close(newClientfd);
			                        upgradeFlag = 0;
			                        break;
			                    }
			                    
			                    upgradeSector++;
			                    WritePtr = 0; 
						
			                    upgradeFlag |= UPGRADE_DONE;
			                    upgradeSector = 0;
			                    recvFileSize = 0;
			                    printf("Firmware update success\n");
			
								QSpiFlashRead(BOOT_PARAM_DATA_ADDR, &param, sizeof(param));
								param.boot_flag	= 0x2;
								QSpiFlashEraseSector(BOOT_PARAM_DATA_ADDR);
								QSpiWriteOneSector(BOOT_PARAM_DATA_ADDR,&param);
								
			
								OSTimeDly(100);
			                    FwUpgradeFinishAck();
			                    OSTimeDly(100); // wait for upgrade finish ack being sent out
			                    lwip_close(newClientfd);
			                    upgradeFlag = 0;
								nl6621_Reset();
			                    break;
			                }
			                else if (WritePtr == FLASH_SECTOR_SIZE)
			                {				
			                    /*写一个sector*/
			                    if (QSpiWriteOneSector(OTA_FW_BAK_OFFSET + upgradeSector * FLASH_SECTOR_SIZE, OtaDataBuf) != 0)
			                    {
			                        lwip_close(newClientfd);
			                        upgradeFlag = 0;   
			                        break;
			                    }
			                    
			                    upgradeSector++;
			                    WritePtr = 0;
			                }
			                
			                printf("use sector:%d, tot:%d, WritePtr:%d, bytes_received:%d\n", 
									upgradeSector, recvFileSize, WritePtr, bytes_received);
			            }
			            else
			            {
			                break;
			            }
				}
#else
			                if (recvFileSize == fileSize)
			                {
			                    upgradeFlag |= UPGRADE_OKFILE;
			                    /*写最后一个sector*/
			                    if (QSpiWriteOneSector(OTA_FW_BAK_OFFSET + upgradeSector * FLASH_SECTOR_SIZE, OtaDataBuf) != 0)
			                    {
			                        lwip_close(newClientfd);
			                        upgradeFlag = 0;
			                        break;
			                    }
			                    
			                    upgradeSector++;
			                    WritePtr = 0; 
			
			                    printf("Download success, start to replace the current firmware\n");
			                    for (i = 0; i < upgradeSector; i++)
			                    {
			                        QSpiFlashRead(OTA_FW_BAK_OFFSET + i * FLASH_SECTOR_SIZE, OtaDataBuf, FLASH_SECTOR_SIZE);
			                        if (QSpiWriteOneSector(NVM_FW_MAIN_OFFSET + i * FLASH_SECTOR_SIZE, OtaDataBuf) != 0)
			                        {
			                            lwip_close(newClientfd);
			                            upgradeFlag = 0;
			                            ret = -1;
			                            break;
			                        }
			                    }
			
			                    if (ret <0)
			                        break;
						
			                    upgradeFlag |= UPGRADE_DONE;
			                    upgradeSector = 0;
			                    recvFileSize = 0;
			                    printf("Firmware update success\n");
			
			                    FwUpgradeFinishAck();
			                    OSTimeDly(100); // wait for upgrade finish ack being sent out
			                    lwip_close(newClientfd);
			                    upgradeFlag = 0;
								nl6621_Reset();
			                    break;
			                }
			                else if (WritePtr == FLASH_SECTOR_SIZE)
			                {				
			                    /*写一个sector*/
			                    if (QSpiWriteOneSector(OTA_FW_BAK_OFFSET + upgradeSector * FLASH_SECTOR_SIZE, OtaDataBuf) != 0)
			                    {
			                        lwip_close(newClientfd);
			                        upgradeFlag = 0;   
			                        break;
			                    }
			                    
			                    upgradeSector++;
			                    WritePtr = 0;
			                }
			                
			                printf("use sector:%d, tot:%d, WritePtr:%d, bytes_received:%d\n", 
									upgradeSector, recvFileSize, WritePtr, bytes_received);
			            }
			            else
			            {
			                break;
			            }
			        }
#endif
			
		}
/***************************************************************************************/

            for( i = 0; i < SOCKET_MAXTCPCLENT; i++)
            {
                if (g_stTCPSocketClients[i].SocketFD == -1)
                {
                    memcpy(&g_stTCPSocketClients[i].TCPClientAddr, &addr, sizeof(addr));
                    g_stTCPSocketClients[i].SocketFD = newClientfd;
                    g_stTCPSocketClients[i].TimeOut = LAN_CLIENT_MAXLIVETIME;
                    GAgent_Printf(GAGENT_INFO,
                            "new tcp client, clientid:%d[%d] [time:%08x second: %08x MS]",
                            newClientfd, i, nl6621_GetTime_S(), nl6621_GetTime_MS() );

                    return newClientfd; //返回连接ID
                }
            }
            /*此时连接个数大于系统定义的最大个数,应该关闭释放掉对应的socket资源*/
            close( newClientfd );
        }
        else
        {
            GAgent_Printf(GAGENT_DEBUG, "Failed to accept client");
        }
    }
    return -1;
}


/*****************************************************************
 *        Function Name    :    Socket_DoTCPServer
 *        add by Alex     2014-02-12
 *      读取TCP链接客户端的发送的数据，并进行相应的处理
 *******************************************************************/
void Socket_DoTCPServer(void)
{
    int i,j;
    fd_set readfds, exceptfds;
    int recdatalen;
    int ret;
    struct timeval t;
    t.tv_sec = 1;//秒
    t.tv_usec = 0;//微秒
    FD_ZERO(&readfds);
    FD_ZERO(&exceptfds);

    for (i = 0; i < SOCKET_MAXTCPCLENT; i++)
    {
        if (g_stTCPSocketClients[i].SocketFD != -1)
        {
            FD_SET(g_stTCPSocketClients[i].SocketFD, &readfds);
        }
    }
    /*因为是循环处理，所以这个地方是立即返回*/
    ret = select(g_TCPSocketMaxClientFd+1, &readfds, NULL, &exceptfds, &t);
    if (ret <0)  //0 成功 -1 失败
    {
        /*没有TCP Client有数据*/
        return;
    }

    /*Read data from tcp clients and send data back */
    for(i=0;i<SOCKET_MAXTCPCLENT;i++)
    {
        if ((g_stTCPSocketClients[i].SocketFD != -1) && (FD_ISSET(g_stTCPSocketClients[i].SocketFD, &readfds)))
        {
            memset(g_stSocketRecBuffer, 0x0, SOCKET_RECBUFFER_LEN);
            recdatalen = recv(g_stTCPSocketClients[i].SocketFD, 
                    g_stSocketRecBuffer, SOCKET_RECBUFFER_LEN, 0);
            if (recdatalen > 0)
            {
                if (recdatalen < SOCKET_RECBUFFER_LEN)
                {
                    /*一次已经获取所有的数据，处理报文*/
                    Socket_ResetClientTimeout(g_stTCPSocketClients[i].SocketFD);
                    DispatchTCPData(g_stTCPSocketClients[i].SocketFD, g_stSocketRecBuffer, recdatalen);
                }
                else
                {
                    GAgent_Printf(GAGENT_WARNING,"Invalid tcp packet length.");
                    /*此时recdatalen == SOCKET_RECBUFFER_LEN，还有数据需要处理。不过根据目前的协议，不可能出现这种情况的*/
                }
            }else if(recdatalen == 0)
			{
				if(g_stTCPSocketClients[i].SocketFD > 0)
				close(g_stTCPSocketClients[i].SocketFD);

				for( j=0;j<8;j++ )
                {
                    if(g_SocketLogin[j]==g_stTCPSocketClients[i].SocketFD)
                        g_SocketLogin[j]=0;
                }
                g_stTCPSocketClients[i].SocketFD = -1;
			}
        }
    }
    return;
}

/* system status */
extern struct sys_status_t sys_status;

int Socket_DispatchUDPRecvData( u8 *pData, int varlen, int messagelen, struct sockaddr_in addr, socklen_t addrLen)
{

    u8 Udp_ReDiscover[200] =  {0};
    unsigned char Udp_ReOnBoarding[8] = {0x00,0x00,0x00,0x03,0x03,0x00,0x00,0x02};

    if ( pData[7] == 0x01 ) { //OnBoarding
        log_notice("OnBoarding\n");

		if(sys_status.onboarding == SYS_STATUS_ONBOARDING_FAILED &&  sys_status.status == SYS_STATUS_WIFI_SOFTAP)
		{
        	Udp_Onboarding(pData, messagelen);
		

	        Socket_sendto(g_UDPServerFd, Udp_ReOnBoarding, 8, &addr, sizeof(struct sockaddr_in));
	
			sys_status.onboarding = SYS_STATUS_ONBOARDING_SUCCESS;
		}
		return 0;

    } else if ( pData[7] == 0x03 ) { //Ondiscover
        log_notice("Ondiscover\n");
        Build_BroadCastPacket( 0, Udp_ReDiscover );
        Socket_sendto(g_UDPServerFd, Udp_ReDiscover, Udp_ReDiscover[4] + 5, &addr, sizeof(struct sockaddr_in));
        return 0;
    }
    log_err("invalid udp packet [%d]", pData[7]);

    return -1;
}

/*-------------------------------local ota function-----------------------------------------*/
#if 1
void DevDiscoverResponse(const char *ip_buf)
{
    int count = 0;
    int udp_sock;
    struct sockaddr_in addrto;
    int nlen = sizeof(addrto);
    char *sendbuf = "ok";		 
    
    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
    {
        DBGPRINT(DEBUG_ERROR, "UDP Broadcast Socket error\n");
        return;
    }
  
    memset(&addrto, 0, sizeof(struct sockaddr_in));  
    addrto.sin_family = AF_INET;  
    addrto.sin_addr.s_addr = inet_addr(ip_buf); 
    addrto.sin_port = htons(UDP_SCAN_BROADCAST_RESPONSE_PORT); /* port:8787 */  
	
    while (1) 
    {	
        sendto(udp_sock, sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&addrto, nlen);
        if (count < UDP_SCAN_RESPONSE_TIME) 
            OSTimeDly(1);
        else
            break;
        count++;
    }
    
    lwip_close(udp_sock);
    DBGPRINT(DEBUG_TRACE, "Respond APP discover device finish.\n");
}

VOID DevDiscoverThread(VOID * arg)
{
    int udp_sock,count;
    struct sockaddr_in server_addr, client_addr,addrto;
    socklen_t addr_len = sizeof(struct sockaddr);
    char buffer[UPP_SCAN_DATA_LEN_MAX];
    unsigned char ip_buf[20];
	char *sendbuf = "ok"; 
    
    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
    {
        DBGPRINT(DEBUG_ERROR, "UDP Broadcast Socket error\n");
        goto DEL_TASK;
    }
  
    memset(&server_addr, 0, sizeof(struct sockaddr_in));  
    server_addr.sin_family = AF_INET;  
    server_addr.sin_addr.s_addr = htonl(IPADDR_ANY); 
    server_addr.sin_port=htons(UDP_SCAN_BROADCAST_PORT); /* port:7878 */ 

    if (bind(udp_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) 
    {
        DBGPRINT_LWIP(DEBUG_ERROR, "bind error\n");
        goto DEL_TASK;
    }

    while (1)
    {
        if (upgradeFlag != 0)
        {
            fcntl(udp_sock, F_SETFL, O_NONBLOCK);
            recvfrom(udp_sock, buffer, UPP_SCAN_DATA_LEN_MAX, 0,
                           (struct sockaddr *)&client_addr, &addr_len);
            OSTimeDly(1);
            continue;
        }
        fcntl(udp_sock, F_SETFL, 0);
        
        DBGPRINT(DEBUG_TRACE, "Waiting for device scan on port %d....\n", UDP_SCAN_BROADCAST_PORT);
        
        memset(buffer, '\0', sizeof(buffer));
        memset(ip_buf, '\0', sizeof(ip_buf));	
        DBGPRINT(DEBUG_WARN, "Poll to read UDP data..........\n");
        
        recvfrom(udp_sock, buffer, UPP_SCAN_DATA_LEN_MAX, 0,
                       (struct sockaddr *)&client_addr, &addr_len);
        
        DBGPRINT(DEBUG_WARN, "Receive APP(IP:%s) discover device broadcast message.......\n", inet_ntoa(client_addr.sin_addr));

        if (strcmp(buffer, UDP_SCAN_STRING) != 0) 
        {
            DBGPRINT(DEBUG_TRACE, "Receive device scan data error.\n");
            continue;
        } 
        else 
        {
            memcpy(ip_buf, (INT8U *)inet_ntoa(client_addr.sin_addr), 
            strlen(inet_ntoa(client_addr.sin_addr)));		
            //DevDiscoverResponse((const char *)ip_buf);
		//	client_addr.sin_port = htons(UDP_SCAN_BROADCAST_RESPONSE_PORT);
		//	printf("udp receive port:%d\n",client_addr.sin_port);
			memset(&addrto, 0, sizeof(struct sockaddr_in));  
    		addrto.sin_family = AF_INET;  
    		addrto.sin_addr.s_addr = inet_addr(ip_buf); 
    		addrto.sin_port = htons(UDP_SCAN_BROADCAST_RESPONSE_PORT); /* port:8787 */

			for(count = 0; count < UDP_SCAN_RESPONSE_TIME; count++)
			{
			  	sendto(udp_sock,sendbuf,strlen(sendbuf), 0, (struct sockaddr*)&addrto, sizeof(addrto));
				OSTimeDly(1);
			}  
        }
    }

DEL_TASK:	
	OSTaskDel(OS_PRIO_SELF);
}

static void FwUpgradeFinishAck(void)
{
	int ret;
	int count = 0;
    GEN_ACK_PKT upgrade_resp;

    upgrade_resp.header.cmd = DEV_ACK_CMD; 
    upgrade_resp.header.dev_type = WIFI_DEV;
    upgrade_resp.ack_cmd_id = FW_UPGRADE_CMD;
    upgrade_resp.ack_status = 1;
    upgrade_resp.reserve1 = 0;
    upgrade_resp.reserve2 = 0;
    upgrade_resp.header.pkt_len = htons(sizeof(upgrade_resp) - 2); //2个字节的报文长度字段。 
	
    while (1) 
    {	
	    ret = DevSendCtrlMsg((UINT8 *)&upgrade_resp, sizeof(upgrade_resp), &upgrade_socket_info);
	    if (ret < 0) 
	    {
	        printf("Failed to send upgrade data finish ack\n");
	    }													
        
		if (count < UDP_UPGRADE_RESPONSE_TIME) 
            OSTimeDly(1);
        else
            break;
        count++; 	
    }
}

static void FwUpgradeProcess(UINT8 * cmd_pkt, unsigned short cmd_len, SOCKET_INFO* sock_info)
{
    GEN_ACK_PKT upgrade_resp;
    FW_UPGRADE_PKT *pkt = (FW_UPGRADE_PKT *)cmd_pkt;
    UINT8 ret = 0;
        
    printf("upgrade start\n");
    upgradeFlag |= UPGRADE_START;
    fileSize = htonl(pkt->fileSize);
    
    upgrade_resp.header.cmd = DEV_ACK_CMD; 
    upgrade_resp.header.dev_type = WIFI_DEV;
    upgrade_resp.ack_cmd_id = FW_UPGRADE_CMD;
    upgrade_resp.ack_status = (ret == 0) ? 1 : 0;
    upgrade_resp.reserve1 = 0;
    upgrade_resp.reserve2 = 0;
    upgrade_resp.header.pkt_len = htons(sizeof(upgrade_resp) - 2); //2个字节的报文长度字段。

    if (DevSendCtrlMsg((UINT8 *)&upgrade_resp, sizeof(upgrade_resp), sock_info) != 0)
    {
        upgradeFlag = 0;
        printf("Failed to send upgrade cmd ack\n");
    }

    return;
}

int DevSendCtrlMsg(UINT8 * buf, int len, SOCKET_INFO* sock_info)
{
    struct sockaddr_in PeerAddr;    // peer address information
    int snd;
    
    PeerAddr.sin_family = AF_INET;         
    PeerAddr.sin_port = sock_info->port;    
    PeerAddr.sin_addr.s_addr = sock_info->ip; 
    memset(PeerAddr.sin_zero, '\0', sizeof(PeerAddr.sin_zero));

    	snd = sendto(sock_info->local_fd, buf, len, 0,
                        (struct sockaddr *) &PeerAddr, 
                        sizeof(struct sockaddr));

    if (snd < 0) 
    {
        printf("DevSendCtrlMsg failed\n");
        return -1;
    }
    return 0;
}
#endif
/*------------------------------------------------------------------------*/

void Socket_DoUDPServer(void)
{ 
    int readnum;
	int length;
    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(struct sockaddr_in);
	FW_UPGRADE_PKT* pkt_data;
	SOCKET_INFO client_socket_info;
    int messagelen; /*报文长度*/
    int varlen;     /*可变数据长度字段的长度*/
		
    if ( g_UDPServerFd <= 0 )
    {
        return ;
    }
		
	while(1)
	{
		readnum = Socket_recvfrom(g_UDPServerFd, g_stSocketRecBuffer, SOCKET_RECBUFFER_LEN, &addr, &addrLen);
		pkt_data = (FW_UPGRADE_PKT*)g_stSocketRecBuffer;
		length = ntohs(pkt_data->header.pkt_len);

/***************************************local ota****************************************/		
		if(readnum ==  (length+ 2))
		{
			switch(pkt_data->header.cmd)
			{
				case FW_UPGRADE_CMD:
					NVIC_DisableIRQ(TMR1_IRQn);
	
					printf("UDP control info: pkt_len:%d, device type:%u, cmd_id:%u, flag:%u, fileSize:%u; data package:%u.\n", 
	            					ntohs(pkt_data->header.pkt_len), 
	            					pkt_data->header.dev_type, 
	            					pkt_data->header.cmd,
	            					pkt_data->flag,
	            					htonl(pkt_data->fileSize),
	            					readnum);
						
					client_socket_info.local_fd = g_UDPServerFd;
					client_socket_info.ip = addr.sin_addr.s_addr;
					client_socket_info.port = addr.sin_port;
					upgrade_socket_info = client_socket_info;
					FwUpgradeProcess(pkt_data,length,&client_socket_info);
					break;
				default:
					break;
			}
	
			continue;
		}
/****************************************************************************************/
	
		        /*根据报文中的报文长度确定报文是否是一个有效的报文*/
	        varlen = mqtt_num_rem_len_bytes(g_stSocketRecBuffer + 4);
	
	        //这个地方+3是因为MQTT库里面实现把 UDP flag算到messagelen里面，这里为了跟mqtt库保持一致所以加3
	        messagelen = mqtt_parse_rem_len(g_stSocketRecBuffer + 3);
	
	        if ((messagelen + varlen + XPG_PACKET_VERSIONLEN) != readnum)
	        {
	            /*报文长度错误*/
	            GAgent_Printf(GAGENT_WARNING, "Invalid UDP packet length");
	            return;
	        }
	
	        if (readnum < SOCKET_RECBUFFER_LEN)
	        {
	            Socket_DispatchUDPRecvData(g_stSocketRecBuffer, varlen, messagelen, addr, addrLen);
	            return;
	        }
	
	        if (readnum >= SOCKET_RECBUFFER_LEN)
	        {
	            /*根据目前的情况，不可能出现这个问题。增加调试信息*/
	            GAgent_Printf(GAGENT_WARNING, "TOO LENGTH OF UDP Packet Size.");
	        }

			return;	
	}
	return;	
}

/****************************************************************
 *        Description        :    send data to cliets
 *        pdata              :    uart pdata
 *        datalength         :    uart data length
 *           cmd             :   0x12:all clients include no login
0x91:just login clients
 *   Add by Alex : 2014-02-13
 *
 ****************************************************************/
int Socket_SendData2Client(u8* pdata, int datalength, unsigned char Cmd )
{
    int j,i;
    int ret;
    if (pdata == NULL) {
        return -1;
    }

    //send data to all udp and tcp clients
    for(j=0;j<8;j++)
    {
        if(g_stTCPSocketClients[j].SocketFD>0)
        {
            if(Cmd==0x91)
            {
                for( i=0;i<8;i++ )
                {
                    if((g_stTCPSocketClients[j].SocketFD == g_SocketLogin[i]) )
                    {
                        GAgent_Printf(GAGENT_INFO,"send data(len:%d) to TCP client [%d]",
                                datalength, g_stTCPSocketClients[j].SocketFD);
                        ret = send(g_stTCPSocketClients[j].SocketFD, pdata, datalength,0);
                    }
                }
            }
            else
            {
                ret = send( g_stTCPSocketClients[j].SocketFD, pdata, datalength,0);
            }

            if (ret != datalength)
            {
                /*增加Log处理*/
                ;
            }
        }
    }

    return 1;
}

#ifdef  __cplusplus
}
#endif /* end of __cplusplus */

