#ifndef TA_VS_LAN_H
#define TA_VS_LAN_H
#include "ta_vs_session.h"
#define TA_VS_LAN_PORT 34986u
/* LAN connection only. Gameplay activation is intentionally gated until the
 * board bridge is verified. No remote inputs are applied to a guest machine. */
typedef struct {
 TaVsSession session;
 TaVsIdentity identity;
 uint64_t nonce,clientNonce;
 uint32_t peerAddress,lastOffer,started;
 uint16_t peerPort;
 uint8_t role,pinned,sessionLive,sentOffer;
 unsigned rejected;
} TaVsLan;
int ta_vs_lan_init(TaVsLan*,const TaVsIdentity*,unsigned role,uint64_t nonce,uint32_t now);
int ta_vs_lan_receive(TaVsLan*,uint32_t now,uint32_t address,uint16_t port,const uint8_t*,size_t);
size_t ta_vs_lan_packet(TaVsLan*,uint32_t now,uint8_t*,size_t,uint32_t*address,uint16_t*port);
int ta_vs_lan_ready(const TaVsLan*);
int ta_vs_lan_timed_out(const TaVsLan*,uint32_t now);
#endif
