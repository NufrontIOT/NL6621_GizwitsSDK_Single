
#include "../include/common.h"


int g_ConCloud_Status = CONCLOUD_INVALID;


extern int Http_Response_Code( char *Http_recevieBuf );
extern int Http_Sent_Provision(void);

int Http_Recive_Did(char *DID)
{
    int ret;
    int response_code = 0;
    char httpReceiveBuf[1024] = {0};
    
    ret = Http_ReadSocket( g_Xpg_GlobalVar.http_socketid, httpReceiveBuf, 1024 );    
    if (ret <= 0 ) 
    {
        return -1;
    }

    response_code = Http_Response_Code( httpReceiveBuf );       
    if( response_code == 201 )
    {
        return Http_Response_DID( httpReceiveBuf, DID);
    }
    else
    {
        GAgent_Printf(GAGENT_WARNING,"HTTP response_code:%d",response_code);
        return -1;
    }
}

int Http_Recive_M2minfo(char *server, int *port)
{
    
    int ret;
    int response_code = 0;
    char httpReceiveBuf[1024] = {0};

    ret = Http_ReadSocket( g_Xpg_GlobalVar.http_socketid, httpReceiveBuf, 1024 );    
    if(ret <=0 ) 
    {
        return -1;
    }
    
    response_code = Http_Response_Code( httpReceiveBuf );  
    if( response_code != 200 ) 
    {
        return -1;
    }
    
    ret = Http_getdomain_port( httpReceiveBuf, server, port );

    return ret;
}

/**************************************************
 **************************************************
 �������ƶ˷�����������
 MQTTҵ����(����/��½/���ݵ�)��MQTT handle
 �ڴ��ڼ佨��socket������
 WIFI           CLOUD(HTTP)          CLOUD(MQTT)
 =================================================
 REQ_DID        ------->    handle
 REQ_DID_ACK    <----    DID_INFO
 REQ_M2MINFO    ------>    handle
 REQ_M2MINFO_ACK  <--  M2M_INFO
 MQTT_LOGIN   --------     (END)            ----------->     mqtt handle(login)
 
 **************************************************/
void GAgent_Login_Cloud(void)
{
    int ret;
	char *p_Did = NULL;

    /* WIFI STATIONģʽ�²��ܽ���cloud */
    if( (wifiStatus & WIFI_MODE_STATION) != WIFI_MODE_STATION ||
        (wifiStatus & WIFI_STATION_STATUS) != WIFI_STATION_STATUS)
    {
        return ;
    }

    switch(g_ConCloud_Status)
    {
        case CONCLOUD_REQ_DID:
            if( nl6621_GetTime_S() - g_Xpg_GlobalVar.connect2CloudLastTime < 1 )
            {
                return ;
            }
            g_Xpg_GlobalVar.connect2CloudLastTime = nl6621_GetTime_S();
            if((Mqtt_Register2Server(&g_stMQTTBroker)==0))
            {
                g_ConCloud_Status = CONCLOUD_REQ_DID_ACK;
                g_Xpg_GlobalVar.send2HttpLastTime = nl6621_GetTime_S();
                log_info("MQTT_Register2Server OK!\n");
            }
            else
            {
                log_info("Mqtt_Register2Server Fail!\n");
            }
            break;

        case CONCLOUD_REQ_DID_ACK:
            /* time out, or socket isn't created , resent  CONCLOUD_REQ_DID */
            if( nl6621_GetTime_S()-g_Xpg_GlobalVar.send2HttpLastTime>=HTTP_TIMEOUT || 
                g_Xpg_GlobalVar.http_socketid<=0 )
            {
                g_ConCloud_Status = CONCLOUD_REQ_DID_ACK;
                break;
            }

            ret = Http_Recive_Did(g_Xpg_GlobalVar.DID);
            if (ret == 0)
            {
                p_Did = g_Xpg_GlobalVar.DID;
                DIdLen = GAGENT_DID_LEN;
                memcpy((unsigned char*)g_stGAgentConfigData.Cloud_DId, (unsigned char*)p_Did, DIdLen);
                g_stGAgentConfigData.Cloud_DId[DIdLen] = '\0';
                nl6621_SaveConfigData(&g_stGAgentConfigData);
                
                g_ConCloud_Status = CONCLOUD_REQ_M2MINFO;
                log_info( "Http Receive DID is:%s[len:%d]\n", p_Did, DIdLen); 
                log_info( "ConCloud Status[%04x]\n", g_ConCloud_Status);
            }
            break;

        case CONCLOUD_REQ_M2MINFO:
            if( nl6621_GetTime_S()-g_Xpg_GlobalVar.connect2CloudLastTime < 1 )
            {
                return ;
            }
            g_Xpg_GlobalVar.connect2CloudLastTime = nl6621_GetTime_S();
            if( Http_Sent_Provision()==0 )
            {
                g_Xpg_GlobalVar.send2HttpLastTime = nl6621_GetTime_S();
                g_ConCloud_Status = CONCLOUD_REQ_M2MINFO_ACK;
                GAgent_Printf( GAGENT_INFO,"Http_Sent_Provision OK! ");
            }                
            else
            {
                GAgent_Printf( GAGENT_INFO,"Http_Sent_Provision Fail!");
            }
            break;
        case CONCLOUD_REQ_M2MINFO_ACK:
            /* time out, or socket isn't created ,resent  CONCLOUD_REQ_M2MINFO */
            if( nl6621_GetTime_S()-g_Xpg_GlobalVar.send2HttpLastTime>=HTTP_TIMEOUT || 
                g_Xpg_GlobalVar.http_socketid<=0 )
            {
                g_ConCloud_Status = CONCLOUD_REQ_M2MINFO;
                break;
            }

            ret = Http_Recive_M2minfo(g_Xpg_GlobalVar.m2m_SERVER, &g_Xpg_GlobalVar.m2m_Port);
            if( ret==0 ) 
            {
    			g_ConCloud_Status = CONCLOUD_REQ_LOGIN;
                GAgent_Printf(GAGENT_WARNING, "--HTTP response OK. Goto MQTT LOGIN");

            }
            break;
        case CONCLOUD_REQ_LOGIN:
            if( nl6621_GetTime_S() - g_Xpg_GlobalVar.connect2CloudLastTime < 1 )
            {
                return ;
            }
            g_Xpg_GlobalVar.connect2CloudLastTime = nl6621_GetTime_S();
            if((Mqtt_Login2Server(&g_stMQTTBroker)==0))
            {
                g_ConCloud_Status = CONCLOUD_RUNNING;
                g_MQTTStatus = MQTT_STATUS_LOGIN;
                log_info( "Mqtt_Login2Server OK!\n");
            }
            else
            {
                log_info("Mqtt_Login2Server Fail!\n");
            }
            break;
        case CONCLOUD_RUNNING:
            /* mqtt handle */
            if(g_MQTTStatus == MQTT_STATUS_LOGIN)
            {
                if( nl6621_GetTime_S() - g_Xpg_GlobalVar.connect2CloudLastTime >= MQTT_CONNECT_TIMEOUT )
                {
					log_info("MQTT connect timeout.\n");
                    g_ConCloud_Status = CONCLOUD_REQ_LOGIN;
                    break;
                }
            }
            MQTT_handlePacket();
            break;
        case CONCLOUD_INVALID:
        default:
            break;
    }
}

