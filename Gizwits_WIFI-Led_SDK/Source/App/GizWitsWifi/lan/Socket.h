#ifndef __GAGENT_SOCKET_H_
#define __GAGENT_SOCKET_H_

#define SOCKET_RECBUFFER_LEN (1*1024)
#define SOCKET_TCPSOCKET_BUFFERSIZE    1*1024
extern u8 *g_stSocketRecBuffer;
int  Status_Socket(void);

extern void Socket_DoTCPServer(void);
extern void Socket_DoUDPServer(void);
extern int Socket_SendData2Client(u8* pdata, int datalength,unsigned char Cmd );

void Init_Service(void);
int Socket_CheckNewTCPClient(void);
void Socket_SendBroadCastPacket( int UdpFlag );

extern int g_TCPServerFd;
extern int g_UDPServerFd;
extern struct sockaddr_in g_stUDPBroadcastAddr;//¨®?¨®¨²UDP1?2£¤
extern int g_UDPBroadcastServerFd;
//implate in lib/adapter*
extern int connect_mqtt_socket(int iSocketId, struct sockaddr_in *Msocket_address, unsigned short port, char *MqttServerIpAddr);
extern int Socket_CreateTCPServer(int tcp_port);
extern int Socket_CreateUDPServer(int udp_port);
extern int Socket_CreateUDPBroadCastServer( int udp_port );
#endif
