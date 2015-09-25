#ifdef  __cplusplus
extern "C"{
#endif

#include "../include/common.h"
#include "stdlib.h"

/***************************************************************/
/***************** External process function *******************/
/***************************************************************/
extern int GAgentV4_CtlDev_with_p0(int fd, unsigned char cmd, unsigned char *data, unsigned short data_len);
extern void SendCtlAckCmd(uint8_t cmd, uint8_t sn);
extern void ResponseDevStatus(int fd);

/****************************************************
*****************************************************
***************LOCAL MACO **************************
*****************************************************
*****************************************************/
#define MQTT_SOCKET_BUFFER_LEN 2048

/****************************************************
*****************************************************
***************LOCAL VARIABLES **********************
*****************************************************
*****************************************************/
static uint16_t g_Msgsub_id=0;
static char Clientid[24];

NST_TIMER *HeartBeatTimer = NULL;
/****************************************************
*****************************************************
***************GLOBAL VIRIABLES**********************
*****************************************************
*****************************************************/
u8 g_MQTTBuffer[MQTT_SOCKET_BUFFER_LEN] = {0};
int g_MQTTStatus=0;
int DIdLen=0;
int socket_flag=0;
mqtt_broker_handle_t g_stMQTTBroker;
GAGENT_CLOUD_TYPE Gagent_Cloud_status = {0};

extern struct sys_status_t sys_status;

/******************************************************************************
 ******************************************************************************
 ******************************************************************************
 *************                              LOCAL FUNCTIONS                       *******************
 ******************************************************************************
 ******************************************************************************
 *****************************************************************************/
static int GetAskOtaPayload()
{
    char HAsk[6]={0x00,0x00,0x00,0x03,0x02,0x05	};
	int Len;
    char *AskOtaPay;

	AskOtaPay = (char*)mem_malloc(100);
    if (AskOtaPay == NULL)
    {
        mem_free( AskOtaPay );
        return 0;
    }
	memset(AskOtaPay,0,100);
	memcpy((unsigned char*)AskOtaPay, (unsigned char*)HAsk,6);
    AskOtaPay[6] = 0x00;
    AskOtaPay[7] = g_productKeyLen;	
    memcpy( (unsigned char*)(AskOtaPay+8), (unsigned char*)g_Xpg_GlobalVar.Xpg_Mcu.product_key, 32 );    
    Len = 8+g_productKeyLen;
	memcpy((unsigned char*)(AskOtaPay+Len), (unsigned char*)g_stGAgentConfigData.FirmwareId, 8);
	Len=Len+8;
	//genProtocolVer[4]
	AskOtaPay[Len]	=0x00;
	AskOtaPay[Len+1]=0x00;
	AskOtaPay[Len+2]=0x00;
	AskOtaPay[Len+3]=0x03;
	Len+=4;
    // g_busiProtocolVerLen
    AskOtaPay[Len] = g_busiProtocolVerLen/255;
    AskOtaPay[Len+1] = g_busiProtocolVerLen%255;	
	Len+=2;
    //g_busiProtocolVer
	memcpy((unsigned char*)(AskOtaPay+Len), (unsigned char*)g_busiProtocolVer, g_busiProtocolVerLen);
	Len+=g_busiProtocolVerLen;    
    PubMsg( &g_stMQTTBroker,"cli2ser_req",AskOtaPay,Len,0);    
    mem_free( AskOtaPay );
	return Len;
}

static char *GetRegClientid()
{
	int i;
	Clientid[0]= 'a';
	Clientid[1]= 'n';
	Clientid[2]= 'o';
	memset(Clientid+3,0,20);
	for( i=3;i<23;i++ )
	{
		Clientid[i] = 65+rand()%(90-65);
		
	}
	Clientid[23] = '\0';
    log_info("Clientid:%s\r\n",Clientid);
    
	return Clientid;
}

/**************************************************
*
*
*		return :0 
*
***************************************************/
static int init_subTopic( mqtt_broker_handle_t* broker ,char *Sub_TopicBuf,int Mqtt_Subflag)
{    
    switch(Mqtt_Subflag)
    {
        case 1:            
            //4.6
            memcpy((unsigned char*)Sub_TopicBuf,"ser2cli_noti/",strlen("ser2cli_noti/"));            
            memcpy((unsigned char*)(Sub_TopicBuf+strlen("ser2cli_noti/")),g_Xpg_GlobalVar.Xpg_Mcu.product_key,32);
            Sub_TopicBuf[strlen("ser2cli_noti/")+g_productKeyLen] = '\0';
            break;
        case 2:
            // 4.7 4.9		
            memcpy((unsigned char*)Sub_TopicBuf,"ser2cli_res/",strlen("ser2cli_res/"));
            memcpy((unsigned char*)(Sub_TopicBuf+strlen("ser2cli_res/")), (unsigned char*)g_Xpg_GlobalVar.DID,strlen(g_Xpg_GlobalVar.DID));
            Sub_TopicBuf[strlen("ser2cli_res/")+strlen(g_Xpg_GlobalVar.DID)] = '\0';	
            break;
        case 3:
            // 4.13			
            memcpy((unsigned char*)Sub_TopicBuf,"app2dev/",strlen("app2dev/"));
            memcpy((unsigned char*)(Sub_TopicBuf+strlen("app2dev/")), (unsigned char*)g_Xpg_GlobalVar.DID,strlen(g_Xpg_GlobalVar.DID));
            Sub_TopicBuf[strlen("app2dev/")+strlen(g_Xpg_GlobalVar.DID)] = '/';
            Sub_TopicBuf[strlen("app2dev/")+strlen(g_Xpg_GlobalVar.DID)+1] = '#';
            Sub_TopicBuf[strlen("app2dev/")+strlen(g_Xpg_GlobalVar.DID)+2] = '\0';        
            break;
        default:
            break;
    }

	return 0;
}

static int Mqtt_IntoRunning(void)
{
    g_MQTTStatus=MQTT_STATUS_RUNNING;
    log_info("MQTT_STATUS_RUNNING\n");
	mqtt_ping(&g_stMQTTBroker); 
    #if (GAGENT_FEATURE_OTA  == 1)    
    {
	    GetAskOtaPayload();
    }
    #endif
    return 0;
}

/***********************************************************
*
*   ���� :  ת��p0 ����
*
*************************************************************/
static int MQTT_DoCloudMCUCmd(mqtt_broker_handle_t *pstMQTTBroker, u8 clientid[32], u8 did[32], u8 *pHiP0Data, int P0DataLen)
{
    int varlen;
    int datalen;
    u8 *pP0Data;
    int pP0DataLen;
	int ret;

    /*���ݱ����еı��ĳ���ȷ�������Ƿ���һ����Ч�ı���*/
    varlen = mqtt_num_rem_len_bytes(pHiP0Data+4);
    /*����ط�+3����ΪMQTT������ʵ�ְ� UDP flag�㵽messagelen���棬����Ϊ�˸�mqtt�Ᵽ��һ�����Լ�3*/
    datalen = mqtt_parse_rem_len(pHiP0Data+3); 
    
    pP0DataLen = datalen-3;//��Ϊ flag(1B)+cmd(2B)=3B
    
    //����payload��ʼ�ĵط�
    pP0Data = &pHiP0Data[7+varlen];
	ret = GAgentV4_CtlDev_with_p0( 0, MCU_CTRL_CMD, pP0Data, pP0DataLen ); 
	if (ret == 0) {
		/* Response device status to all phone and remote app */
		ResponseDevStatus(pstMQTTBroker->socketid);
		log_warn("GAgent Process MQTT cloud CMD success(ret:%d).\n", ret);
		
	} else {
		log_err("GAgent Process MQTT cloud CMD error(ret:%d).\n", ret);
	}
    
    return ret;
}

/*******************************************************
*
*���سɹ�����Topic�ĸ���
*
********************************************************/
static int Mqtt_SubLoginTopic( mqtt_broker_handle_t *LoginBroker)
{
    char Sub_TopicBuf[100];
    char Topic[100];
    
    memset(Sub_TopicBuf,0,100);       
    memset(Topic,0,100);
    
    switch(g_MQTTStatus)
    {
        case MQTT_STATUS_LOGIN:
            init_subTopic(LoginBroker,Sub_TopicBuf,1);
            if(mqtt_subscribe( LoginBroker,Sub_TopicBuf,&g_Msgsub_id )==1)
            {   
                sprintf(Topic,"LOGIN sub topic is:%s\n",Sub_TopicBuf);
                GAgent_Printf(GAGENT_INFO,Topic);
                g_MQTTStatus=MQTT_STATUS_LOGINTOPIC1;
                log_info("MQTT_STATUS_LOGINTOPIC1\n");
            }
            break;            
        case MQTT_STATUS_LOGINTOPIC1:
            init_subTopic(LoginBroker,Sub_TopicBuf,2);
            if(mqtt_subscribe( LoginBroker,Sub_TopicBuf,&g_Msgsub_id )==1)
            {    
                sprintf(Topic,"LOGIN T1 sub topic is:%s\n",Sub_TopicBuf);                                
                log_info(Topic);
                g_MQTTStatus=MQTT_STATUS_LOGINTOPIC2;
                log_info("MQTT_STATUS_LOGINTOPIC2\n");
            }                
            break;
        case MQTT_STATUS_LOGINTOPIC2:
            init_subTopic(LoginBroker,Sub_TopicBuf,3);
            if(mqtt_subscribe( LoginBroker,Sub_TopicBuf,&g_Msgsub_id )==1)
            {
                sprintf(Topic,"LOGIN T2 sub topic is:%s\n",Sub_TopicBuf);
                log_info(Topic);             
                g_MQTTStatus=MQTT_STATUS_LOGINTOPIC3;
                log_info("MQTT_STATUS_LOGINTOPIC3\n");
            }                
            break;
        case MQTT_STATUS_LOGINTOPIC3:
            Mqtt_IntoRunning();                
            break;           
        default:
            break;                
    }

    return 0;
}

/***********************************************************
*
*   return :    0 success ,1 error
*
*************************************************************/
static int Mqtt_SendConnetPacket( mqtt_broker_handle_t *pstBroketHandle, char *pDID )
{       
    char TmpPasscode[12];    
    int ret;
    
    if( pDID==NULL)//������¼
    {
        mqtt_init(pstBroketHandle, GetRegClientid()); 
        log_info("MQTT register with clientid:%s\n", Clientid);
    }
    else //�û���¼      
    {
        memcpy((unsigned char*)TmpPasscode, (unsigned char*)g_stGAgentConfigData.wifipasscode,10);
        TmpPasscode[10]='\0';                  
        mqtt_init(pstBroketHandle,pDID);
        log_info("MQTT login with passcode:%s, DID:%s\n", g_stGAgentConfigData.wifipasscode, pDID);
        mqtt_init_auth(pstBroketHandle,pDID,TmpPasscode);
    }
    ret = Cloud_MQTT_initSocket(pstBroketHandle,0);
    if(ret == 0)
    {
        ret = mqtt_connect(pstBroketHandle);
        if (ret != 0)
        {
            log_info("mqtt connect is failed with:%d\n", ret); 
            return 1;
        }
        return 0;                    
    }
    log_info("Mqtt_SendConnetPacket failed, Cloud_MQTT_initSocket ret:%d\n", ret); 
    return 1;
}

void Cloud_HB_Status_init(void)
{
    Gagent_Cloud_status.pingTime = 0;
    Gagent_Cloud_status.loseTime = 0;
}

/***********************************************************
*
*   ���� : MQTT ����������
*   
*
*************************************************************/
static void Cloud_Mqtt_HB_Timer(void)
{
    /* socketid<0,��δ������mqtt������ */
    if(g_stMQTTBroker.socketid < 0)
    {
        return ;
    }
    
    Gagent_Cloud_status.pingTime++;
    /* û�յ�mqtt�κ����ݣ����������� */
    if(Gagent_Cloud_status.pingTime >= CLOUD_HB_TIMEOUT_ONESHOT)
    {
        mqtt_ping(&g_stMQTTBroker);

#ifdef POWERSAVE_SUPPORT
		InfPowerSaveSet(PS_MODE_DISABLE);
#endif

        Gagent_Cloud_status.loseTime++;
        Gagent_Cloud_status.pingTime = 0;
    }
    /* ��ʱ����û�յ��κ����ݣ����µ�¼mqtt������ */
    if(Gagent_Cloud_status.loseTime >= CLOUD_HB_TIMEOUT_CNT_MAX && 
            Gagent_Cloud_status.pingTime >= CLOUD_HB_TIMEOUT_REDUNDANCE)
    {
        g_ConCloud_Status = CONCLOUD_REQ_LOGIN;
        
        close( g_stMQTTBroker.socketid );
        g_stMQTTBroker.socketid = -1;

        Cloud_HB_Status_init();

        GAgent_Printf(GAGENT_INFO, "[CLOUD]Heaert beat timeout over %d times:%d S,relogin mqtt",
                CLOUD_HB_TIMEOUT_CNT_MAX, nl6621_GetTime_S());
    }
}

/***********************************************************
*
*   ���� : CLOUD ģ��ļ�ʱ�����м�ʱ���������
*
*************************************************************/
void GAgent_Cloud_Timer(void)
{
    Cloud_Mqtt_HB_Timer();
}

#if HEART_BEAT_DEBUG_MACRO
void GAgent_Timer_Init(void)
{
    NST_InitTimer(&HeartBeatTimer, MQTT_HeartbeatTime, NULL, NST_TRUE);
}
#endif

static int Mqtt_DoLogin( mqtt_broker_handle_t *LOG_Sendbroker,u8* packet,int packetLen )
{
    if(packet[3] != 0x00)
    {        
        log_err("Mqtt_DoLogin CONNACK failed![%d]. restart register now\n", packet[3]);
        /*����ע��*/
        g_ConCloud_Status = CONCLOUD_REQ_DID;
        
        log_info("MQTT_STATUS_START\n");
        return 0;
    }
    Mqtt_SubLoginTopic( LOG_Sendbroker );
		
#if HEART_BEAT_DEBUG_MACRO
    NST_SetTimer(HeartBeatTimer,  MQTT_HEARTBEAT * 100);		
#else
	Cloud_HB_Status_init();

#endif

	log_info("  Create MQTT heart beat timer success.\n");
    return 0;
}

/******************************************************************************
 ******************************************************************************
 ******************************************************************************
 ************                              GLOBAL FUNCTIONS                       *******************
 ******************************************************************************
 ******************************************************************************
 *****************************************************************************/

/***********************************************************
*
*   ���� : ���ƶ�ע�ᣬ����passcode/mac addr/product key
*
*************************************************************/
int Mqtt_Register2Server( mqtt_broker_handle_t *Reg_broker )
{
    int ret;               
    ret = Http_InitSocket( 1 );      
    if(ret!=0) return 1;    
    ret = Http_POST( HTTP_SERVER,g_stGAgentConfigData.wifipasscode,g_Xpg_GlobalVar.a_mac,g_Xpg_GlobalVar.Xpg_Mcu.product_key );        
    if( ret!=0 )
    {
        log_info("5 Mqtt_Register2Server\n");
        return 1;
    }    
    log_info("6 Mqtt_Register2Server\n");
    return 0;
}

/***********************************************************
*
*   ���� : ��¼�ƶ˷�����, ����TCP ������
*
*************************************************************/
int Mqtt_Login2Server( mqtt_broker_handle_t *Reg_broker )
{
    log_info("login2ser [%s]%d, len:%d\n", g_Xpg_GlobalVar.DID, strlen(g_Xpg_GlobalVar.DID), DIdLen);
    if( Mqtt_SendConnetPacket(Reg_broker, g_Xpg_GlobalVar.DID) == 0)
    {    
        log_info(" Mqtt_SendConnetPacket OK!\n");
        return 0;
    }   
    log_info(" Mqtt_SendConnetPacket NO!\n");    
    return 1;       
}

/***********************************************************
*
*   ���� : �����ַ��������·�������
*
*************************************************************/
int Mqtt_DispatchPublishPacket( mqtt_broker_handle_t *pstMQTTBroker, u8 *packetBuffer,int packetLen )
{
	int ret;
    u8 topic[128];
    int topiclen;
    u8 *pHiP0Data;
    int HiP0DataLen;
    int i;
    u8  clientid[32];
    int clientidlen = 0;
    u8 *pTemp;
    u8  DIDBuffer[32];
    u8  Cloud_FirmwareId[8]; 
    u16 cmd;
    u16 *pcmd=NULL;
    u8 MQTT_ReqBinBuf[14]=
    {
        0x00,0x00 ,0x00 ,0x03 ,0x02 ,0x07 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x8c
    };            
    topiclen = mqtt_parse_pub_topic(packetBuffer, topic);
    HiP0DataLen = packetLen - topiclen;
    topic[topiclen] = '\0';
    pHiP0Data = mem_malloc(HiP0DataLen);
    if (pHiP0Data == NULL) 
    {
        mem_free(pHiP0Data);
        return -1;
    }

    memset(pHiP0Data, 0x0, HiP0DataLen);
    HiP0DataLen = mqtt_parse_publish_msg(packetBuffer, pHiP0Data); 
    pcmd = (u16*)&pHiP0Data[4];
    cmd = ntohs( *pcmd );
    
    if(strncmp((const char*)topic,"app2dev/",strlen("app2dev/"))==0)
    {
        pTemp = &topic[strlen("app2dev/")];
        i = 0;
        while (*pTemp != '/')
        {
            DIDBuffer[i] = *pTemp;
            i++;
            pTemp++;
        }
        DIDBuffer[i] = '\0';

        if (DIdLen == 0)
        {
            memset(g_Xpg_GlobalVar.DID,0,GAGENT_DID_LEN_MAX);
            memcpy((unsigned char*)g_Xpg_GlobalVar.DID,DIDBuffer,i);
            memset(g_stGAgentConfigData.Cloud_DId, 0x0, sizeof(g_stGAgentConfigData.Cloud_DId));
            memcpy((unsigned char*)g_stGAgentConfigData.Cloud_DId, DIDBuffer,i);
            g_stGAgentConfigData.Cloud_DId[i] = '\0';
            nl6621_SaveConfigData(&g_stGAgentConfigData);
            
            DIdLen = i;
        }
        pTemp ++; ///*����\/*/
        i=0;
        while (*pTemp != '\0')
        {
            clientid[i] = *pTemp;
            i++;
            pTemp++;
        }
        clientid[i]= '\0';          
        strcpy( g_Xpg_GlobalVar.phoneClientId, (const char*)clientid );
        MQTT_DoCloudMCUCmd(pstMQTTBroker, clientid, DIDBuffer, pHiP0Data, HiP0DataLen);

    }
    //�������¹̼���Ӧ
    else if(strncmp((const char*)topic,"ser2cli_res/",strlen("ser2cli_res/"))==0)
    {
        //pHiP0Data��Ϣ���ָ��
        //HiP0DataLen��Ϣ��ĳ��� packetBuffer
        
        #if (GAGENT_FEATURE_OTA == 1)
        {      
            log_info(" MQTT Respond Topic1: %s\n",topic);            
            memcpy( Cloud_FirmwareId,pHiP0Data+6,8);            
            for( i=0;i<8;i++)
            {
                if( Cloud_FirmwareId[i]!=0 ) break;
            }
            
            if(i==8)
            {
                mem_free( pHiP0Data );
                log_info("cloud have no Firmware!\n");
                return 0;
            }
            
            if( (cmd==0x0206))
            {
                if(memcmp(g_stGAgentConfigData.FirmwareId,Cloud_FirmwareId,8)!=0)
                {
                    memcpy(MQTT_ReqBinBuf+6,Cloud_FirmwareId,8);                                 
                    PubMsg( &g_stMQTTBroker,"cli2ser_req",MQTT_ReqBinBuf,14,0);                    
                }
            }            
           mem_free( pHiP0Data );
        }
        #endif
        return -3;
    } 
    else if(strncmp((const char*)topic,"ser2cli_noti/",strlen("ser2cli_noti/"))==0)
    {
        ;
    }
    else
    {
        ;
    }    
    mem_free( pHiP0Data );
    return 0;
}

void MQTT_handlePacket()
{
    int packetLen = 0;
    u8 packettype=0;
    int Recmsg_id;
    int ret;
    
    if( g_stMQTTBroker.socketid == -1)
    {
        return;
    }

    packetLen = MQTT_readPacket(g_MQTTBuffer, MQTT_SOCKET_BUFFER_LEN);	         
	if(packetLen>0)  				
	{
	    packettype = MQTTParseMessageType(g_MQTTBuffer);
        if ( packettype != MQTT_MSG_PINGRESP)
        {
            log_info("MQTT PacketType:%08x[g_MQTTStatus:%08x]\n", 
            		packettype, g_MQTTStatus);
        }
        
		Cloud_HB_Status_init();
        switch(packettype)
        {
            case MQTT_MSG_PINGRESP:
				log_notice("\tReceived MQTT server heart beat ACK.\n");
				sys_status.status = SYS_STATUS_WIFI_STA_LOGIN;
				if(socket_flag == 0 )
				{
					BSP_GPIOSetValue(USER_GPIO_IDX_LED, GPIO_HIGH_LEVEL);
					socket_flag = 1;
				}

#ifdef POWERSAVE_SUPPORT
				InfPowerSaveSet(PS_MODE_SOFT);
#endif

                break;
                
            case MQTT_MSG_CONNACK:                          
                if( g_MQTTStatus==MQTT_STATUS_LOGIN )
                {                        
                    Mqtt_DoLogin( &g_stMQTTBroker,g_MQTTBuffer,packetLen );
                }                             
                break;           
            case MQTT_MSG_SUBACK:
            {
                Recmsg_id = mqtt_parse_msg_id(g_MQTTBuffer);
             
                if(g_Msgsub_id == Recmsg_id)
                {
                    Mqtt_SubLoginTopic(&g_stMQTTBroker);
                }                                  
                break;
            }
            case MQTT_MSG_PUBLISH:
            {                    
                if(g_MQTTStatus==MQTT_STATUS_RUNNING)
                {
                    ret = Mqtt_DispatchPublishPacket(&g_stMQTTBroker,g_MQTTBuffer,packetLen);
                    if (ret != 0)
                    {
                        log_info("Mqtt_DispatchPublishPacket return:%d\n",ret);
                    }
                    break;                               
                }
                else
                {
                    log_info("Receive MQTT_MSG_PUBLISH packet but MQTTStatus is:%d\n",g_MQTTStatus);
                }
                break ;
            }
            default:
                log_info("MQTTStatus is:%d , Invalid packet type:%08x\n",g_MQTTStatus, packettype);
                break;
		}
    }

    #if (GAGENT_FEATURE_OTA == 1)
    {
        if( packetLen==-3&&(g_MQTTStatus==MQTT_STATUS_RUNNING) )
        {
            int topiclen;
            int VARLEN;
            int mqttHeadLen; 

            u8 Mtopic[100]={0};
            
            log_info("OTA MQTTStatus is:%d \n", g_MQTTStatus);     
            
            VARLEN = mqtt_num_rem_len_bytes(g_MQTTBuffer);
            topiclen = mqtt_parse_pub_topic(g_MQTTBuffer, Mtopic);                                 
            
            mqttHeadLen = 1+VARLEN+2+topiclen;
            packettype = MQTTParseMessageType(g_MQTTBuffer);
                        
            log_info("topic:%s\r\n", Mtopic);            
            if ( packettype == MQTT_MSG_PUBLISH )
            {            
                if(strncmp(Mtopic, "ser2cli_res/", strlen("ser2cli_res/"))==0)
                {                
                    //SOCKET_RECBUFFER_LEN Ϊ���յ������ݳ���
                    DRV_OTAPacketHandle(g_MQTTBuffer + mqttHeadLen, MQTT_SOCKET_BUFFER_LEN - mqttHeadLen, g_stMQTTBroker.socketid);
                }
            }               
        }
    
    }
    #endif
    return;
}

#ifdef  __cplusplus
}
#endif /* end of __cplusplus */

