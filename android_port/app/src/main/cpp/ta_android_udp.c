#include "ta_vs_udp.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
static int socket_error(void){switch(errno){case EINVAL:return -10022;case EISCONN:return -10056;case EMSGSIZE:return -10040;case ECONNRESET:return -10054;case ENETUNREACH:return -10051;case ETIMEDOUT:return -10060;default:return -(10000+errno);}}
void ta_vs_udp_close(TaVsUdp*u){if(!u)return;if(u->opened)close((int)u->socketHandle);memset(u,0,sizeof *u);}
int ta_vs_udp_open(TaVsUdp*u,uint16_t port){if(!u)return -10022;memset(u,0,sizeof *u);int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(fd<0)return socket_error();u->socketHandle=(uintptr_t)fd;u->opened=1;
 if(fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0)|O_NONBLOCK)<0){int e=socket_error();ta_vs_udp_close(u);return e;}fcntl(fd,F_SETFD,FD_CLOEXEC);
 struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=INADDR_ANY;a.sin_port=htons(port);if(bind(fd,(struct sockaddr*)&a,sizeof a)<0){int e=socket_error();ta_vs_udp_close(u);return e;}socklen_t n=sizeof a;if(getsockname(fd,(struct sockaddr*)&a,&n)<0){int e=socket_error();ta_vs_udp_close(u);return e;}u->localPort=ntohs(a.sin_port);return 0;}
int ta_vs_udp_enable_broadcast(TaVsUdp*u){if(!u||!u->opened)return -10022;int yes=1;return setsockopt((int)u->socketHandle,SOL_SOCKET,SO_BROADCAST,&yes,sizeof yes)?socket_error():0;}
int ta_vs_udp_pin_peer(TaVsUdp*u,const char*ip,uint16_t port){if(!u||!u->opened||!ip||!port)return -10022;struct in_addr a;if(inet_pton(AF_INET,ip,&a)!=1||!a.s_addr||a.s_addr==UINT32_MAX)return -10022;if(u->peerPinned&&(u->peerAddress!=a.s_addr||u->peerPort!=port))return -10056;u->peerAddress=a.s_addr;u->peerPort=port;u->peerPinned=1;return 0;}
int ta_vs_udp_receive_any(TaVsUdp*u,void*p,size_t cap,uint32_t*address,uint16_t*port){if(!u||!u->opened||!p||!cap||cap>65507||!address||!port)return -10022;struct sockaddr_in from={0};socklen_t length=sizeof from;
 ssize_t n=recvfrom((int)u->socketHandle,p,cap,MSG_TRUNC,(struct sockaddr*)&from,&length);if(n<0){if(errno==EAGAIN||errno==EWOULDBLOCK||errno==ECONNRESET||errno==EINTR)return 0;return socket_error();}if((size_t)n>cap){u->rejected++;return 0;}if(length!=sizeof from||from.sin_family!=AF_INET)return -10022;*address=from.sin_addr.s_addr;*port=ntohs(from.sin_port);u->received++;return (int)n;}
int ta_vs_udp_send_to(TaVsUdp*u,uint32_t address,uint16_t port,const void*p,size_t n){if(!u||!u->opened||!p||!n||n>65507||!address||!port)return -10022;struct sockaddr_in to={0};to.sin_family=AF_INET;to.sin_addr.s_addr=address;to.sin_port=htons(port);ssize_t sent=sendto((int)u->socketHandle,p,n,0,(struct sockaddr*)&to,sizeof to);if(sent<0){if(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)return 0;return socket_error();}if(sent!=(ssize_t)n)return -10040;u->sent++;return (int)sent;}
int ta_vs_udp_pump(TaVsUdp*u,TaVsSession*s,uint32_t now,unsigned rx,unsigned tx){if(!u||!s||!u->opened||!u->peerPinned||rx>32||tx>8)return -10022;uint8_t packet[TA_VS_PACKET_BYTES];int work=0;for(unsigned i=0;i<rx;i++){uint32_t a;uint16_t p;int n=ta_vs_udp_receive_any(u,packet,sizeof packet,&a,&p);if(n<0)return n;if(!n)break;work++;if(a!=u->peerAddress||p!=u->peerPort){u->rejected++;continue;}TaVsResult result=ta_vs_receive(s,now,packet,(size_t)n);if(result!=TA_VS_OK){u->rejected++;u->lastProtocolError=result;}}for(unsigned i=0;i<tx;i++){size_t n=ta_vs_packet(s,now,packet,sizeof packet);if(!n)break;int sent=ta_vs_udp_send_to(u,u->peerAddress,u->peerPort,packet,n);if(sent<0)return sent;if(!sent)break;work++;}return work;}

#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK
int ta_vs_udp_open_dhcp(TaVsUdp*u,uint16_t port){return ta_vs_udp_open(u,port);}
#endif
