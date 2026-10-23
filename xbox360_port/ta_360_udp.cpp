#include "ta_360_platform.h"
#undef XNetGetTitleXnAddr
#if defined(TA_VS_HOST)
#include <winsock2.h>
#include <windows.h>
#else
#include <xtl.h>
#include <winsockx.h>
#endif
#include "ta_vs_udp.h"
#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK && !defined(TA_VS_HOST)

#endif
#include <string.h>
#if !defined(TA_VS_HOST)
static unsigned ta_xnet_users;
#endif
void ta_vs_udp_close(TaVsUdp*u){
 if(!u)return;
 if(u->opened)closesocket((SOCKET)u->socketHandle);
 if(u->winsockStarted)WSACleanup();
#if !defined(TA_VS_HOST)
 if(u->xnetStarted&&ta_xnet_users&&!--ta_xnet_users)XNetCleanup();
#endif
 memset(u,0,sizeof *u);
}
static int ta_vs_udp_open_mode(TaVsUdp*u,uint16_t port,int automatic){
 if(!u)return -WSAEINVAL;
 memset(u,0,sizeof *u);
 int error;
#if !defined(TA_VS_HOST)
 XNetStartupParams xp;memset(&xp,0,sizeof xp);xp.cfgSizeOfStruct=sizeof xp;xp.cfgFlags=XNET_STARTUP_BYPASS_SECURITY;xp.cfgSockDefaultRecvBufsizeInK=128;xp.cfgSockDefaultSendBufsizeInK=128;
 #if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK
 (void)automatic;
#endif
 if(!ta_xnet_users){error=XNetStartup(&xp);if(error)return -error;}
 ta_xnet_users++;u->xnetStarted=1;
#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK

#endif
#endif
 WSADATA wd;error=WSAStartup(MAKEWORD(2,2),&wd);
 if(error){ta_vs_udp_close(u);return -error;}u->winsockStarted=1;
 SOCKET sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
 if(sock==INVALID_SOCKET){error=WSAGetLastError();ta_vs_udp_close(u);return -error;}
 u->socketHandle=(uintptr_t)sock;u->opened=1;
 BOOL plain=TRUE;
 /* Match MKart360's Internet UDP setup. IPv4 values already contain the
    network-order bytes used by inet_addr, sockaddr and TAP9 packet fields. */
 if(setsockopt(sock,SOL_SOCKET,0x5801,(const char*)&plain,sizeof plain)==SOCKET_ERROR){error=WSAGetLastError();ta_vs_udp_close(u);return -error;}
 int bufferBytes=128*1024;setsockopt(sock,SOL_SOCKET,SO_RCVBUF,(const char*)&bufferBytes,sizeof bufferBytes);setsockopt(sock,SOL_SOCKET,SO_SNDBUF,(const char*)&bufferBytes,sizeof bufferBytes);
 u_long nonblocking=1;
 if(ioctlsocket(sock,FIONBIO,&nonblocking)==SOCKET_ERROR){error=WSAGetLastError();ta_vs_udp_close(u);return -error;}
 struct sockaddr_in local;memset(&local,0,sizeof local);local.sin_family=AF_INET;local.sin_addr.s_addr=INADDR_ANY;local.sin_port=htons(port);
 if(bind(sock,(struct sockaddr*)&local,sizeof local)==SOCKET_ERROR){error=WSAGetLastError();ta_vs_udp_close(u);return -error;}
 int length=sizeof local;
 if(getsockname(sock,(struct sockaddr*)&local,&length)==SOCKET_ERROR){error=WSAGetLastError();ta_vs_udp_close(u);return -error;}
 u->localPort=ntohs(local.sin_port);return 0;
}
int ta_vs_udp_open(TaVsUdp*u,uint16_t port){return ta_vs_udp_open_mode(u,port,0);}
#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK
int ta_vs_udp_open_dhcp(TaVsUdp*u,uint16_t port){return ta_vs_udp_open_mode(u,port,1);}
#endif
int ta_vs_udp_pin_peer(TaVsUdp*u,const char*ip,uint16_t port){
 if(!u||!u->opened||!ip||!port)return -WSAEINVAL;
 unsigned long address=inet_addr(ip);
 if(address==INADDR_NONE||address==INADDR_ANY)return -WSAEINVAL;
 /* An active endpoint may not silently switch the peer beneath a session. */
 if(u->peerPinned&&(u->peerAddress!=address||u->peerPort!=port))return -WSAEISCONN;
 u->peerAddress=(uint32_t)address;u->peerPort=port;u->peerPinned=1;return 0;
}
int ta_vs_udp_pump(TaVsUdp*u,TaVsSession*s,uint32_t now,unsigned rxBudget,unsigned txBudget){
 if(!u||!s||!u->opened||!u->peerPinned||rxBudget>32||txBudget>8)return -WSAEINVAL;
#if !defined(TA_VS_HOST)
 XNADDR address;DWORD status=XNetGetTitleXnAddr(&address);
 if(status==XNET_GET_XNADDR_PENDING||!(status&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_PPPOE)))return 0;
#endif
 uint8_t packet[TA_VS_PACKET_BYTES];int work=0;
 for(unsigned k=0;k<rxBudget;k++){
  struct sockaddr_in from;int length=sizeof from;
  int n=recvfrom((SOCKET)u->socketHandle,(char*)packet,sizeof packet,0,(struct sockaddr*)&from,&length);
  if(n==SOCKET_ERROR){int error=WSAGetLastError();if(error==WSAEWOULDBLOCK)break;if(error==WSAEMSGSIZE){u->rejected++;continue;}if(error==WSAECONNRESET)continue;return -error;}
  work++;u->received++;
  if(length!=sizeof from||from.sin_family!=AF_INET||from.sin_addr.s_addr!=u->peerAddress||ntohs(from.sin_port)!=u->peerPort){u->rejected++;continue;}
  TaVsResult r=ta_vs_receive(s,now,packet,(size_t)n);
  if(r!=TA_VS_OK){u->rejected++;u->lastProtocolError=r;}
 }
 struct sockaddr_in to;memset(&to,0,sizeof to);to.sin_family=AF_INET;to.sin_addr.s_addr=u->peerAddress;to.sin_port=htons(u->peerPort);
 for(unsigned k=0;k<txBudget;k++){
  size_t n=ta_vs_packet(s,now,packet,sizeof packet);if(!n)break;
  int sent=sendto((SOCKET)u->socketHandle,(const char*)packet,(int)n,0,(struct sockaddr*)&to,sizeof to);
  if(sent==SOCKET_ERROR){int error=WSAGetLastError();if(error==WSAEWOULDBLOCK)break;return -error;}
  if(sent!=(int)n)return -WSAEMSGSIZE;
  u->sent++;work++;
 }
 return work;
}

int ta_vs_udp_enable_broadcast(TaVsUdp*u){
 if(!u||!u->opened)return -WSAEINVAL;
 int yes=1;if(setsockopt((SOCKET)u->socketHandle,SOL_SOCKET,SO_BROADCAST,(const char*)&yes,sizeof yes)==SOCKET_ERROR)return -WSAGetLastError();return 0;
}
int ta_vs_udp_receive_any(TaVsUdp*u,void*p,size_t cap,uint32_t*address,uint16_t*port){
 if(!u||!u->opened||!p||cap>65507||!cap||!address||!port)return -WSAEINVAL;
 struct sockaddr_in from;int length=sizeof from;
 int n=recvfrom((SOCKET)u->socketHandle,(char*)p,(int)cap,0,(struct sockaddr*)&from,&length);
 if(n==SOCKET_ERROR){int error=WSAGetLastError();if(error==WSAEWOULDBLOCK||error==WSAECONNRESET)return 0;return -error;}
 if(length!=sizeof from||from.sin_family!=AF_INET)return -WSAEINVAL;
 *address=from.sin_addr.s_addr;*port=ntohs(from.sin_port);u->received++;return n;
}
int ta_vs_udp_send_to(TaVsUdp*u,uint32_t address,uint16_t port,const void*p,size_t n){
 if(!u||!u->opened||!p||!n||n>65507||!address||!port)return -WSAEINVAL;
 struct sockaddr_in to;memset(&to,0,sizeof to);to.sin_family=AF_INET;to.sin_addr.s_addr=address;to.sin_port=htons(port);
 int sent=sendto((SOCKET)u->socketHandle,(const char*)p,(int)n,0,(struct sockaddr*)&to,sizeof to);
 if(sent==SOCKET_ERROR){int error=WSAGetLastError();if(error==WSAEWOULDBLOCK)return 0;return -error;}
 if(sent!=(int)n)return -WSAEMSGSIZE;u->sent++;return sent;
}

extern "C" DWORD TA_360_TitleAddress(XNADDR*address){return XNetGetTitleXnAddr(address);}
extern "C" const uint8_t*TA_360_IPBytes(uint32_t address){static uint8_t bytes[4];memcpy(bytes,&address,sizeof bytes);return bytes;}
