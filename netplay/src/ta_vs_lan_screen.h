#ifndef TA_VS_LAN_SCREEN_H
#define TA_VS_LAN_SCREEN_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Screen-only frontend. Never owns or modifies SNES state. */
enum {TA_LAN_SELECT,TA_LAN_ADDRESS,TA_LAN_HOST_WAIT,TA_LAN_JOIN_WAIT,TA_LAN_HANDSHAKE,TA_LAN_CONNECTED,TA_LAN_LOST,TA_LAN_ERROR};
void ta_vs_lan_text(uint32_t*,int,int,const char*,uint32_t,int);
void ta_vs_lan_render(uint32_t*pixels,unsigned state,uint32_t localIp,uint32_t peerIp,unsigned remoteButtons,int error);
#ifdef __cplusplus
}
#endif
#endif
