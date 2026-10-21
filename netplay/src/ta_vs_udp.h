#ifndef TA_VS_UDP_H
#define TA_VS_UDP_H
#include "ta_vs_session.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One frontend-owned endpoint. Raw IPv4 direct peer transport, not an Xbox
 * Live backend, matchmaking service, NAT traversal service, or authentication. */
typedef struct {
 uintptr_t socketHandle;
 uint32_t peerAddress;
 uint16_t localPort,peerPort;
 unsigned opened,winsockStarted,xnetStarted,peerPinned;
 uint32_t sent,received,rejected;
 int lastProtocolError;
} TaVsUdp;
/* Open only a fresh or previously closed instance. Close before reopening. */
int ta_vs_udp_open(TaVsUdp*,uint16_t localPort);
#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK
/* Close the old endpoint first. Starts fresh title-only DHCP, without saving IP configuration. */
int ta_vs_udp_open_dhcp(TaVsUdp*,uint16_t localPort);
#endif
int ta_vs_udp_pin_peer(TaVsUdp*,const char*ipv4,uint16_t port);
/* Bounded LAN discovery uses these raw nonblocking datagrams before pinning.
 * Address is IPv4 in network byte order. Broadcast requires explicit enable. */
int ta_vs_udp_enable_broadcast(TaVsUdp*);
int ta_vs_udp_receive_any(TaVsUdp*,void*,size_t,uint32_t*address,uint16_t*port);
int ta_vs_udp_send_to(TaVsUdp*,uint32_t address,uint16_t port,const void*,size_t);
void ta_vs_udp_close(TaVsUdp*);
/* Bounded nonblocking pump. Returns work count or negative socket error.
 * Game-side ta_vs_peek/commit runs separately on the local update boundary. */
int ta_vs_udp_pump(TaVsUdp*,TaVsSession*,uint32_t nowMs,unsigned receiveBudget,unsigned sendBudget);
#ifdef __cplusplus
}
#endif
#endif
