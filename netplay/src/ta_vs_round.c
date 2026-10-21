#include "ta_vs_round.h"
#include <string.h>
void ta_vs_round_init(TaVsRound*r){memset(r,0,sizeof *r);r->epoch=1;r->phase=TA_ROUND_SELECT;}
/*0 accepted;1 stale/ignored;2 future/defer;-1 illegal transition. */
int ta_vs_round_control(TaVsRound*r,unsigned role,unsigned kind,uint32_t epoch){
 if(!r||role>1||kind>9||!epoch)return -1;
 if(kind==6||kind==9){
  if(epoch<=r->epoch)return 1;
  if(!role||r->epoch==UINT32_MAX||epoch!=r->epoch+1||r->phase!=TA_ROUND_RESULT||!r->localVote||!r->peerVote)return -1;
  if((kind==9)!=(r->localOptions||r->peerOptions))return -1;
  if(r->next&&r->next!=epoch)return -1;r->next=epoch;r->nextOptions=(uint8_t)(kind==9);return 0;
 }
 if(epoch<r->epoch)return 1;if(epoch>r->epoch)return 2;
 if(kind==5||kind==8){if(r->phase!=TA_ROUND_RESULT)return -1;r->peerVote=1;r->peerOptions=(uint8_t)(kind==8);}
 if(kind==2||kind==3)r->phase=TA_ROUND_RESULT;
 if(kind==7)r->peerLeft=1;
 return 0;
}
unsigned ta_vs_round_offer(const TaVsRound*r,unsigned role){return r&&!role&&r->phase==TA_ROUND_RESULT&&r->localVote&&r->peerVote&&!r->next&&r->epoch!=UINT32_MAX?(r->localOptions||r->peerOptions?9:6):0;}
void ta_vs_round_local_vote(TaVsRound*r,unsigned options){if(r&&r->phase==TA_ROUND_RESULT){r->localVote=1;r->localOptions=(uint8_t)(options!=0);}}
int ta_vs_round_advance(TaVsRound*r){if(!r||r->epoch==UINT32_MAX||r->next!=r->epoch+1)return -1;uint32_t epoch=r->next;unsigned options=r->nextOptions;memset(r,0,sizeof *r);r->epoch=epoch;r->phase=options?TA_ROUND_SELECT:TA_ROUND_LOADING;return 0;}
uint32_t ta_vs_round_seed(uint32_t base,uint32_t epoch){
#if defined(TA_VS_FAST_START) && TA_VS_FAST_START
 /* The ROM consumes 11 table-index bits and 8 bias bits. An odd stride visits
    all 524288 effective states before reuse within a connection. */
 base^=base>>16;base*=0x7feb352du;base^=base>>15;
 return (base+(epoch-1u)*0x1f3du)&0x7ffffu;
#else
if(epoch==1)return base;uint32_t x=base^(epoch*0x9e3779b9u);x^=x<<13;x^=x>>17;x^=x<<5;return x;
#endif
}
