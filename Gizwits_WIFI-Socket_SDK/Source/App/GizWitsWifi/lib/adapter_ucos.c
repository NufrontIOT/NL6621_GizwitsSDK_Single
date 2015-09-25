#ifdef  __cplusplus
extern "C"{
#endif

#include "../include/common.h"

OS_EVENT * pdomainSem;
static ip_addr_t domain_ipaddr;
static unsigned int domain_flag = 0;


/* other file viariable */
extern struct sockaddr_in g_stUDPBroadcastAddr;//ÓÃÓÚUDP¹ã²¥


void InitDomainSem(void)
{
    pdomainSem = OSSemCreate(0);
}

void Gagent_ServerFound(const char *name, ip_addr_t *ipaddr, void *arg)
{
    if ((ipaddr) && (ipaddr->addr)) {
        memcpy(&domain_ipaddr, ipaddr, sizeof(ip_addr_t));
        domain_flag = 1;
        GAgent_Printf(GAGENT_LOG, "Get DNS server ip success.\n\r");

    } else {
        domain_flag = 0;
        GAgent_Printf(GAGENT_LOG, "Get DNS server ip error.\n\r");    
    }

    OSSemPost(pdomainSem);
}


int GAgent_GetHostByName(char *domain, char *IPAddress)
{
    unsigned char Err;
    ip_addr_t ip_addr;
    char str[32];

    memset(str, 0x0, sizeof(str));

    switch (dns_gethostbyname(domain, &ip_addr, Gagent_ServerFound, NULL)) {
        case ERR_OK:
			memcpy((unsigned char*)IPAddress, (unsigned char*)inet_ntoa(ip_addr.addr), strlen(inet_ntoa(ip_addr.addr)) + 1);
        	GAgent_Printf(GAGENT_LOG, "ok Server name %s, first address: %s", 
                domain, inet_ntoa(domain_ipaddr.addr));
			return 0;
        case ERR_INPROGRESS:
            GAgent_Printf(GAGENT_LOG, "dns_gethostbyname called back success.\n\r");

			/* Waiting for connect success */
		    OSSemPend(pdomainSem, 1000, &Err);
		
		    if (domain_flag == 1) {
		        memcpy((unsigned char*)IPAddress, (unsigned char*)inet_ntoa(domain_ipaddr.addr), strlen(inet_ntoa(domain_ipaddr.addr)) + 1);
		        GAgent_Printf(GAGENT_LOG, "call back Server name %s, first address: %s", 
		                domain, inet_ntoa(domain_ipaddr.addr));
				return 0;
		    } else {
		        return -1;
		    }
            break;

        default:
            GAgent_Printf(GAGENT_LOG, "dns_gethostbyname called back error.\n\r");
            return -1;
    }
}


int Gagent_setsocketnonblock(int socketfd)
{
	return fcntl(socketfd, F_SETFL, O_NONBLOCK);
}

int Socket_sendto(int sockfd, u8 *data, int len, void *addr, int addr_size)
{
    return sendto(sockfd, data, len, 0, (const struct sockaddr*)addr, addr_size);
}
int Socket_accept(int sockfd, void *addr, socklen_t *addr_size)
{
    return accept(sockfd, (struct sockaddr *)addr, addr_size);
}

int Socket_recvfrom(int sockfd, u8 *buffer, int len, void *addr, socklen_t *addr_size)
{
    return recvfrom(sockfd, buffer, len, 0, (struct sockaddr *)addr, addr_size);
}
int connect_mqtt_socket(int iSocketId, struct sockaddr_in *Msocket_address, unsigned short port, char *MqttServerIpAddr)
{
    memset(Msocket_address, 0x0, sizeof(struct sockaddr_in));
    Msocket_address->sin_family = AF_INET;
    Msocket_address->sin_port = htons(port);// = port;
    Msocket_address->sin_addr.s_addr = inet_addr(MqttServerIpAddr);
	memset(&(Msocket_address->sin_zero), 0, sizeof(Msocket_address->sin_zero)); 

	log_info("befor connect to mqtt server:%s(port:%d, socketfd:%d).\n\n", 
			MqttServerIpAddr, port, iSocketId);

    return connect(iSocketId, (struct sockaddr *)Msocket_address, sizeof(struct sockaddr)); 
}

int Socket_CreateTCPServer(int tcp_port)
{
    struct sockaddr_in addr;

    if (g_TCPServerFd == -1)
    {
        g_TCPServerFd = socket(AF_INET, SOCK_STREAM, 0);

        if (g_TCPServerFd < 0)
        {
            log_info("Create TCPServer failed.errno:%d\n", errno);
            g_TCPServerFd = -1;
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(tcp_port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(g_TCPServerFd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        {
            GAgent_Printf(GAGENT_DEBUG, "TCPSrever socket bind error,errno:%d", errno);
            close(g_TCPServerFd);
            g_TCPServerFd = -1;
            return -1;
        }

        if (listen(g_TCPServerFd, 0) != 0)
        {
            GAgent_Printf(GAGENT_DEBUG, "TCPServer socket listen error, errno:%d", errno);
            close(g_TCPServerFd);
            g_TCPServerFd = -1;
            return -1;
        } 
    } else {
        /**/
        return -1;
    }
    GAgent_Printf(GAGENT_DEBUG,"TCP Server socketid:%d on port:%d", g_TCPServerFd, tcp_port);
    return 0;
}

int Socket_CreateUDPServer(int udp_port)
{
    struct sockaddr_in addr;
    if (g_UDPServerFd == -1) 
    {
        g_UDPServerFd = socket(AF_INET, SOCK_DGRAM, 0);
        if (g_UDPServerFd < 1)
        {
            GAgent_Printf(GAGENT_DEBUG, "UDPServer socket create error,errno:%d", errno);
            g_UDPServerFd = -1;
            return -1;
        }

        memset(&addr, 0x0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(udp_port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(g_UDPServerFd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        {
            GAgent_Printf(GAGENT_DEBUG, "UDPServer socket bind error,errno:%d", errno);
            close(g_UDPServerFd);
            g_UDPServerFd = -1;
            return -1;
        }
    }
    GAgent_Printf(GAGENT_DEBUG, "UDP Server socketid:%d on port:%d", g_UDPServerFd, udp_port);
    return 0;
}


int Socket_CreateUDPBroadCastServer( int udp_port )
{
    int udpbufsize = 2;

    if ( g_UDPBroadcastServerFd == -1 )
    {
        g_UDPBroadcastServerFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (g_UDPBroadcastServerFd < 0)
        {
            GAgent_Printf(GAGENT_DEBUG, "UDP BC socket create error, errno:%d", errno);
            g_UDPBroadcastServerFd = -1;
            return -1;
        }

        g_stUDPBroadcastAddr.sin_family = AF_INET;
        g_stUDPBroadcastAddr.sin_port = htons(udp_port);
        g_stUDPBroadcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

        if (setsockopt(g_UDPBroadcastServerFd, SOL_SOCKET, SO_BROADCAST, &udpbufsize, sizeof(int)) != 0)
        {
            GAgent_Printf(GAGENT_DEBUG, "UDP BC Server setsockopt error,errno:%d", errno);
            //return;
        }

        if (bind(g_UDPBroadcastServerFd, (struct sockaddr *)&g_stUDPBroadcastAddr, sizeof(g_stUDPBroadcastAddr)) != 0)
        {
            GAgent_Printf(GAGENT_DEBUG, "UDP BC Server bind error,errno:%d", errno);
            close(g_UDPBroadcastServerFd);
            g_UDPBroadcastServerFd = -1;
            return -1;
        }
    }
    GAgent_Printf(GAGENT_DEBUG, "UDP BC Server socketid:%d on port:%d", g_UDPBroadcastServerFd, udp_port);
    return 0;
}

#ifdef  __cplusplus
}
#endif /* end of __cplusplus */
