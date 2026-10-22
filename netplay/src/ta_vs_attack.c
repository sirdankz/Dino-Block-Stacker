#include "ta_vs_attack.h"
#include <string.h>
static int valid(unsigned type,unsigned value){
 return type==TA_VS_CHAIN?(value>=1&&value<=12):(type>=2&&type<=6&&value==1);
}
TaVsResult ta_vs_attack_send(TaVsSession*s,unsigned type,unsigned value,uint32_t frame,uint32_t*sequence){
 if(!valid(type,value))return TA_VS_BAD_ARGUMENT;
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
 if(!s||!s->applicationRound)return TA_VS_BAD_ARGUMENT;
 uint32_t r=s->applicationRound;
 uint8_t data[12]={3,(uint8_t)type,(uint8_t)(value>>8),(uint8_t)value,(uint8_t)(frame>>24),(uint8_t)(frame>>16),(uint8_t)(frame>>8),(uint8_t)frame,(uint8_t)(r>>24),(uint8_t)(r>>16),(uint8_t)(r>>8),(uint8_t)r};
#else
 uint8_t data[8]={2,(uint8_t)type,(uint8_t)(value>>8),(uint8_t)value,(uint8_t)(frame>>24),(uint8_t)(frame>>16),(uint8_t)(frame>>8),(uint8_t)frame};
#endif
 return ta_vs_enqueue(s,TA_VS_ATTACK,data,sizeof data,sequence);
}
TaVsResult ta_vs_attack_decode(const TaVsEvent*e,TaVsAttack*out){
 if(!e||!out||e->tag!=TA_VS_ATTACK)return TA_VS_BAD_ARGUMENT;
 TaVsAttack a;memset(&a,0,sizeof a);a.sequence=e->sequence;
 if(e->size==4&&e->data[0]==1&&e->data[1]==2&&e->data[2]==0&&e->data[3]==1){a.type=2;a.value=1;}
 else if((e->size==8&&e->data[0]==2)||(e->size==12&&e->data[0]==3)){
  if(e->size==12){a.round=((uint32_t)e->data[8]<<24)|((uint32_t)e->data[9]<<16)|((uint32_t)e->data[10]<<8)|e->data[11];if(!a.round)return TA_VS_BAD_ARGUMENT;}
  a.type=e->data[1];a.value=((uint16_t)e->data[2]<<8)|e->data[3];
  a.sourceFrame=((uint32_t)e->data[4]<<24)|((uint32_t)e->data[5]<<16)|((uint32_t)e->data[6]<<8)|e->data[7];
 }else return TA_VS_BAD_ARGUMENT;
 if(!valid(a.type,a.value))return TA_VS_BAD_ARGUMENT;
 *out=a;return TA_VS_OK;
}
int ta_vs_attack_apply(uint8_t*w,size_t n,unsigned player,const TaVsAttack*a){
 if(!w||n!=131072||player>1||!a||!valid(a->type,a->value))return TA_VS_BAD_ARGUMENT;
 static const unsigned pending[7]={0x42c,0,0x444,0x44c,0x454,0x45c,0x470};
 unsigned address=pending[a->type]+player*2,value=a->value;
 if(a->type==TA_VS_CHAIN && (w[address]||w[address+1]))return TA_VS_ATTACK_BUSY;
 if(a->type!=TA_VS_CHAIN){
  value=w[address]|((unsigned)w[address+1]<<8);
  /* Retain a network event when no counter capacity remains. This guarded
   * transport boundary never silently wraps and loses an unacknowledged attack. */
  if(value==65535)return TA_VS_ATTACK_BUSY;
  ++value;
 }
 /* Chain delivery REPLACES the pending height, as the original STA does.
  * Ordinary/metal delivery increments its separate pending counter. */
 w[address]=(uint8_t)value;w[address+1]=(uint8_t)(value>>8);return TA_VS_OK;
}
void ta_vs_attack_inbox_init(TaVsAttackInbox*i){if(i)memset(i,0,sizeof *i);}
unsigned ta_vs_attack_waiting(const TaVsAttackInbox*i){
 if(!i)return 0;
 unsigned n=0;for(unsigned k=0;k<TA_VS_WINDOW;k++)n+=i->used[k]!=0;return n;
}
const TaVsAttack*ta_vs_attack_find(const TaVsAttackInbox*i,uint32_t seq){
 if(!i||!seq)return NULL;
 for(unsigned k=0;k<TA_VS_WINDOW;k++)if(i->used[k]&&i->attack[k].sequence==seq)return i->attack+k;
 return NULL;
}
TaVsResult ta_vs_attack_stage(TaVsSession*s,TaVsAttackInbox*i){
 if(!s||!i)return TA_VS_BAD_ARGUMENT;
 const TaVsEvent*e=ta_vs_peek(s);if(!e)return TA_VS_NOT_READY;
 TaVsAttack a;TaVsResult result=ta_vs_attack_decode(e,&a);if(result)return result;
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
 if(a.round!=s->applicationRound)return TA_VS_WRONG_SESSION;
#endif
 unsigned slot=0;while(slot<TA_VS_WINDOW&&i->used[slot])slot++;
 if(slot==TA_VS_WINDOW)return TA_VS_QUEUE_FULL;
 /* Single-owner game thread: no callbacks or allocation between copy/commit. */
 i->attack[slot]=a;i->used[slot]=1;result=ta_vs_commit(s,a.sequence);
 if(result){memset(i->attack+slot,0,sizeof a);i->used[slot]=0;}
 return result;
}
int ta_vs_attack_deliver(TaVsAttackInbox*i,uint32_t seq,uint8_t*w,size_t n,unsigned player){
 const TaVsAttack*a=ta_vs_attack_find(i,seq);if(!a)return TA_VS_NOT_READY;
 int result=ta_vs_attack_apply(w,n,player,a);
 if(!result){unsigned slot=(unsigned)(a-i->attack);i->used[slot]=0;memset(i->attack+slot,0,sizeof *a);}
 return result;
}
