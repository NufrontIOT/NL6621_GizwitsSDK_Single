#ifdef  __cplusplus
extern "C"{
#endif

#include "../include/common.h"

/******************************************************************************
 ******************************************************************************
 ******************************************************************************
 *************                              LOCAL FUNCTIONS                       *******************
 ******************************************************************************
 ******************************************************************************
 *****************************************************************************/
static int send_packet(int socketid, const void* buf, unsigned int count)
{
    int ret;
	ret = send(socketid, buf, count, 0);
    return ret;
}

static int close_socket(mqtt_broker_handle_t* broker)
{
    int fd = broker->socketid;
    close(fd);
    broker->socketid = -1;

    return 0;
}

/******************************************************************************
 ******************************************************************************
 ******************************************************************************
 ************                              GLOBAL FUNCTIONS                       *******************
 ******************************************************************************
 ******************************************************************************
 *****************************************************************************/


/******************************************************************
*       function    :   Cloud_MQTT_initSocket
*       broker      :   mqtt struct    
*       flag        :   0 wifi module to register socketid.
*                       1 wifi module to login socketid                       
*
*
********************************************************************/
int Cloud_MQTT_initSocket( mqtt_broker_handle_t* broker,int flag )
{
    int ret;
//    int MTflag;
    struct sockaddr_in Msocket_address;
    char MqttServerIpAddr[32];
    int iSocketId;
//    int packetLen;
    char *domain = g_Xpg_GlobalVar.m2m_SERVER;
    unsigned short port = g_Xpg_GlobalVar.m2m_Port;
	
    memset(MqttServerIpAddr, 0x0, sizeof(MqttServerIpAddr));

    ret = GAgent_GetHostByName( domain, MqttServerIpAddr );
    if(ret !=0 )
    {
        GAgent_Printf(GAGENT_ERROR,"Gethostbyname(%s) fail ret:%d", domain, ret);
        return 1; //域名解析失败
    }
    GAgent_Printf(GAGENT_INFO,"MQTT Connect Domain:%s IP:%s \r\n", domain, MqttServerIpAddr);

    if( broker->socketid!=-1 )
    {
        GAgent_Printf(GAGENT_INFO,"Cloud_MQTT_initSocket close[%04x] mqtt socket", broker->socketid);
        close_socket(broker);
    }
    
    if( (iSocketId = socket(AF_INET, SOCK_STREAM, 0))<0)
    {
        GAgent_Printf(GAGENT_ERROR," MQTT socket init fail");
        return 1;
    }

    ret = connect_mqtt_socket(iSocketId, &Msocket_address, port, MqttServerIpAddr);
    if (ret < 0)
    {
        GAgent_Printf(GAGENT_WARNING,"mqtt socket connect fail with:%d", ret);
        close(iSocketId);
        return 3;
    }

    /*For Test MQTT Read And Write Func*/
    MQTT_handlePacket();
    
    GAgent_Printf(GAGENT_LOG, "Created mqtt socketid[%08x]", iSocketId);

    // MQTT stuffs
    mqtt_set_alive(broker, MQTT_HEARTBEAT);
    broker->socketid = iSocketId;
    broker->mqttsend = send_packet; 
    
    return 0;
}

int MQTT_readPacket(uint8_t *packetBuffer, int bufferLen)
{
	int bytes_rcvd;
	fd_set readfds;
	struct timeval tmv;

    uint8_t *pData;
    int messageLen;
    int varLen;
    int packet_length;
    tmv.tv_sec = 0;
	tmv.tv_usec = 0;
    
	memset(packetBuffer, 0, bufferLen);

	FD_ZERO (&readfds);
	FD_SET (g_stMQTTBroker.socketid, &readfds);
	if(select((g_stMQTTBroker.socketid+1), &readfds, NULL, NULL, &tmv)==-1)
    {
	    return -2;
    }

    if(FD_ISSET(g_stMQTTBroker.socketid, &readfds))
    {
        bytes_rcvd = recv(g_stMQTTBroker.socketid, packetBuffer, bufferLen, 0);
		if((bytes_rcvd) <= 0)
		{
			return -1;
		}        
        pData = packetBuffer + 0; /*去掉头*/
		messageLen = mqtt_parse_rem_len(pData);
		varLen = mqtt_num_rem_len_bytes(pData);
		packet_length = varLen + messageLen + 1;

        if (bytes_rcvd < packet_length)
        {
            /*目前这种情况不可能发生，增加调试信息*/
            GAgent_Printf(GAGENT_INFO, "goto %s:%d ", __FUNCTION__, __LINE__);
            return -3;
        }
       
        return bytes_rcvd;
    }
    
	return 0;
}

/**********************************************************
*
*			Function 	: PubMsg() 
*			broker	 	: mqtt_broker_handle_t
*			topic			:	sub topic
*			Payload		:	msg payload
*			PayLen		: payload length
*			flag			: 0 qos 0 1 qos 1
*			return 		: 0 pub topic success 1 pub topic fail.
*			Add by Alex lin		2014-04-03
*
***********************************************************/
int PubMsg( mqtt_broker_handle_t* broker, const char* topic, char* Payload, int PayLen, int flag )
{
	uint16_t msg_id;
	int pubFlag=0;

	switch(	flag )
	{
		case 0:	
            pubFlag = XPGmqtt_publish( broker,topic,Payload,PayLen,0 );
            break;
		case 1:	
            pubFlag = XPGmqtt_publish_with_qos( broker,topic,Payload,PayLen,0,1,&msg_id);
			break;
		default:
            pubFlag=1;
			break;
	}
    
	return pubFlag;
}

#ifdef  __cplusplus
}
#endif /* end of __cplusplus */


