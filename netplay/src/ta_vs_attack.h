#ifndef TA_VS_ATTACK_H
#define TA_VS_ATTACK_H
#include "ta_vs_session.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Original deliverable descriptor classes. Type1 is an internal linked actor,
 * not a deliverable attack: its ROM delivery case deliberately loops. */
enum {TA_VS_CHAIN=0,TA_VS_NORMAL3=2,TA_VS_NORMAL4=3,TA_VS_NORMAL5=4,TA_VS_NORMAL6=5,TA_VS_METAL=6};
enum {TA_VS_ATTACK_BUSY=1};
typedef struct {uint32_t sequence,sourceFrame,round;uint16_t type,value;} TaVsAttack;
typedef struct {uint8_t used[TA_VS_WINDOW];TaVsAttack attack[TA_VS_WINDOW];} TaVsAttackInbox;
/* value=chain rows1..12 or1 for one ordinary/metal actor. sourceFrame is
 * metadata; caller still owns original travel/arrival scheduling. */
TaVsResult ta_vs_attack_send(TaVsSession*,unsigned type,unsigned value,uint32_t sourceFrame,uint32_t*sequence);
TaVsResult ta_vs_attack_decode(const TaVsEvent*,TaVsAttack*);
int ta_vs_attack_apply(uint8_t*wram,size_t bytes,unsigned localPlayer,const TaVsAttack*);
void ta_vs_attack_inbox_init(TaVsAttackInbox*);
/* ACK only after copying into an owned bounded game queue. Incoming packet
 * order must not replace the original actor engine's delivery priorities. */
TaVsResult ta_vs_attack_stage(TaVsSession*,TaVsAttackInbox*);
const TaVsAttack*ta_vs_attack_find(const TaVsAttackInbox*,uint32_t sequence);
int ta_vs_attack_deliver(TaVsAttackInbox*,uint32_t sequence,uint8_t*wram,size_t bytes,unsigned localPlayer);
unsigned ta_vs_attack_waiting(const TaVsAttackInbox*);
#ifdef __cplusplus
}
#endif
#endif
