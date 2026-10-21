#ifndef TA_VS_ATTACK3_H
#define TA_VS_ATTACK3_H
#include "ta_vs_session.h"
/* Narrow verified boundary: original descriptor type2 (one width3 garbage
 * block, produced by the four-panel combo replay). Other attack types remain
 * unsupported. Not attached to the Xbox frontend. Game thread/frame-boundary
 * caller must enforce exact ROM identity and active versus state. */
enum {TA_VS_ATTACK3_BUSY=1};
#ifdef __cplusplus
extern "C" {
#endif
TaVsResult ta_vs_attack3_send(TaVsSession*,uint32_t*sequence);
/* Returns OK only after the original pending mailbox accepted the attack.
 * BUSY means the 16-bit pending count is full: retry without committing.
 * Caller commits the network event once, after OK; no implicit ACK here. */
int ta_vs_attack3_accept(TaVsSession*,uint8_t*wram,size_t bytes,unsigned localPlayer);
int ta_vs_attack3_apply(uint8_t*wram,size_t bytes,unsigned localPlayer,const TaVsEvent*);
#ifdef __cplusplus
}
#endif
#endif
