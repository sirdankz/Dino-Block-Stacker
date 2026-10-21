#ifndef TA_VS_ROUND_H
#define TA_VS_ROUND_H
#include <stdint.h>
enum {TA_ROUND_SELECT=1,TA_ROUND_LOADING=2,TA_ROUND_PLAY=3,TA_ROUND_RESULT=4};
typedef struct{uint32_t epoch,next;uint8_t phase,localVote,peerVote,localOptions,peerOptions,nextOptions,peerLeft;}TaVsRound;
void ta_vs_round_init(TaVsRound*);
int ta_vs_round_control(TaVsRound*,unsigned localRole,unsigned kind,uint32_t epoch);
unsigned ta_vs_round_offer(const TaVsRound*,unsigned localRole);
void ta_vs_round_local_vote(TaVsRound*,unsigned options);
int ta_vs_round_advance(TaVsRound*);
uint32_t ta_vs_round_seed(uint32_t base,uint32_t epoch);
#endif
