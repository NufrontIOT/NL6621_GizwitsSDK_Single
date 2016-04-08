#ifdef  __cplusplus
extern "C"{
#endif


#include "../include/common.h"
    
 
u8 g_productKey[35]={0};
int g_productKeyLen = 0;


u8 *g_busiProtocolVer; //业务逻辑协议版本号
u16 g_busiProtocolVerLen;


void MCU_PasscodeTimer()
{
    g_passcodeEnable=0;
    GAgent_Printf(GAGENT_INFO,"Passcode Disable.");
    
}



/*********************************************************
*
*		buf			:	uart receive data pointer
*		prodKey	    :	productKey	pointer
*		busiProtocolVer: business logic protocolVer
*		return	    :	productKey length
*		Add by Alex lin 		2014-03-20
*
***********************************************************/
int MUC_DoProductKeyPacket(const u8* buf,u8* prodKey )/*与协议不一致, 
no busiProtocolVer,  watson*/
{
	int productKeyLen=0;
	unsigned short busiProtocolVerLen=0;
    
	productKeyLen = buf[9];
	busiProtocolVerLen=buf[9+productKeyLen+1]*256+buf[9+productKeyLen+2];
    g_busiProtocolVerLen = busiProtocolVerLen;
    
	memcpy( prodKey,buf+10,productKeyLen);

    if (g_busiProtocolVer != NULL)
    {
        mem_free( g_busiProtocolVer );
    }
    
	g_busiProtocolVer = (u8*)mem_malloc(busiProtocolVerLen);
    if (g_busiProtocolVer == NULL)
    {
        return 0;
    }

	memcpy(g_busiProtocolVer,buf+(9+productKeyLen+2+1),busiProtocolVerLen);    	  
	
	return productKeyLen;
}

/*************************************************
*
*			FUNCTION : transtion u16 to 2 byte for mqtt remainlen
*			remainLen: src data to transtion
*			return	 : varc
*			Add by alex 2014-04-20
***************************************************/
varc Tran2varc(short remainLen)
{
	varc Tmp;
	
    if (remainLen <= 127) 
    {
        //fixed_header[0] = remainLen;	   	   
        Tmp.var[0] = remainLen;
        Tmp.varcbty = 1;
    } 
    else 
    {
        // first byte is remainder (mod) of 128, then set the MSB to indicate more bytes		 
        Tmp.var[0] = remainLen % 128;
        Tmp.var[0]=Tmp.var[0] | 0x80;
        // second byte is number of 128s          
        Tmp.var[1] = remainLen / 128;	 
        Tmp.varcbty=2;
    }
    return Tmp;
}


/*************************************************
*       FUNCTION :	send mcu cmd to cloud 
*       uartbuf	 :	uart receive buf 
*
*************************************************/
void MCU_SendPacket2Cloud( u8* uartbuf,int uartbufLen )
{
    u8 *SendPack=NULL;
    int SendPackLen=0;
    u8 *ClientId=NULL;
    int ClientIdLen=0;
    int UartP0Len=0;    
    u8 McuSendTopic[200]={0};
    
    short VarLen=0;
    varc SendVarc;
    int i;
    
    ClientIdLen = uartbuf[8]*256+uartbuf[9];
    ClientId = (u8*)mem_malloc( ClientIdLen );
    if( ClientId==NULL )
    {
        return;
    }
    
    //protocolVer(4B)+len(2B)+cmd(2B)+phoneClientIdLen(2B)+phoneClientId+p0
    UartP0Len = uartbufLen-10-ClientIdLen;
    
    
    memset(ClientId,0,ClientIdLen);
    memcpy(ClientId,uartbuf+10,ClientIdLen);    
    
    memcpy( McuSendTopic,"dev2app/",strlen("dev2app/"));
    memcpy( (unsigned char*)(McuSendTopic+strlen("dev2app/")), (unsigned char*)g_Xpg_GlobalVar.DID,strlen(g_Xpg_GlobalVar.DID));
    McuSendTopic[strlen("dev2app/")+strlen(g_Xpg_GlobalVar.DID)]='/';
    memcpy( McuSendTopic+strlen("dev2app/")+strlen(g_Xpg_GlobalVar.DID)+1,ClientId,ClientIdLen);
    
    //protocolVer(4B)+varLen(1~4B)+flag(1B)+cmd(2B)+P0
    VarLen=1+2+UartP0Len;
    SendVarc = Tran2varc(VarLen);
    SendPackLen = 4+SendVarc.varcbty+1+2+UartP0Len;
    
    SendPack = (u8*)mem_malloc(SendPackLen);
    if( SendPack==NULL ) 
    {
        mem_free(ClientId);
        return ;
    }
    
    //protocolVer
    SendPack[0] = 0x00;
    SendPack[1] = 0x00;
    SendPack[2] = 0x00;
    SendPack[3] = 0x03;
    //varLen
    for(i=0;i<SendVarc.varcbty;i++)
	{
		SendPack[4+i] = SendVarc.var[i];
	}
     //flag   
    SendPack[4+SendVarc.varcbty] = 0x00;
    //CMD
    SendPack[4+SendVarc.varcbty+1]=0x00;
    SendPack[4+SendVarc.varcbty+2]=0x91;
    //p0
    memcpy( (SendPack+4+SendVarc.varcbty+3),uartbuf+10+ClientIdLen,UartP0Len);
    GAgent_Printf(GAGENT_INFO,"MCU data to Topic: %s",McuSendTopic);  
        
    PubMsg( &g_stMQTTBroker, (char *)McuSendTopic, (char *)SendPack, SendPackLen, 0);
        
    mem_free( SendPack );
    mem_free( ClientId );
    
    return;
}

/***************************************
*
*   重置WiFi的 wifipasscode，wifi_ssid
*              wifi_key     Cloud_DId
*
****************************************/
void WiFiReset()
{
    memset( g_stGAgentConfigData.wifipasscode,0,16 );
    memset( g_stGAgentConfigData.wifi_ssid,0,32 );  
    memset( g_stGAgentConfigData.wifi_key,0,32 );
    memset( g_stGAgentConfigData.Cloud_DId,0,GAGENT_DID_LEN_MAX );
    
    make_rand(g_stGAgentConfigData.wifipasscode);
    //清楚标志位
    g_stGAgentConfigData.flag &=~XPG_CFG_FLAG_CONNECTED; 
    nl6621_SaveConfigData(&g_stGAgentConfigData);
    DIdLen = 0;    
    nl6621_Reset();
    return;
}

void GAgent_SaveSSIDAndKey(char *pSSID, char *pKey)
{
	//memcpy(&wNetConfig, nwkpara, sizeof(network_InitTypeDef_st));
    memset(g_stGAgentConfigData.wifi_ssid,0,32);
    memset(g_stGAgentConfigData.wifi_key,0,32);
    
    memcpy((unsigned char*)g_stGAgentConfigData.wifi_ssid, (unsigned char*)pSSID, 32);
    memcpy((unsigned char*)g_stGAgentConfigData.wifi_key, (unsigned char*)pKey, 32);
	
	g_stGAgentConfigData.flag = g_stGAgentConfigData.flag | XPG_CFG_FLAG_CONNECTED;
    nl6621_SaveConfigData(&g_stGAgentConfigData);	
      	
    GAgent_Printf(GAGENT_INFO,"Configuration is successful.");
    return;
}


void Xpg_Send2AllClients( unsigned char Cmd )
{   
    
    
}


#ifdef  __cplusplus
}
#endif /* end of __cplusplus */

