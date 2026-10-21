#include "ta_vs_attack3.h"
TaVsResult ta_vs_attack3_send(TaVsSession*s,uint32_t*sequence){
 /* version, original attack descriptor type, count (big endian) */
 static const uint8_t message[4]={1,2,0,1};
 return ta_vs_enqueue(s,TA_VS_ATTACK,message,sizeof message,sequence);
}
int ta_vs_attack3_apply(uint8_t*w,size_t n,unsigned player,const TaVsEvent*event){
 if(!w||n!=131072||player>1||!event||event->tag!=TA_VS_ATTACK||event->size!=4||event->data[0]!=1||event->data[1]!=2||event->data[2]!=0||event->data[3]!=1)return TA_VS_BAD_ARGUMENT;
 unsigned a=0x0444+player*2;
 unsigned pending=w[a]|((unsigned)w[a+1]<<8);
 if(pending==65535)return TA_VS_ATTACK3_BUSY;
 ++pending;
 /* Matches $89:AEE1/$89:AF68 pending-delivery semantics. Original $82:ABD4
  * transfers this to the ready count; $86:97EB creates the actual block. */
 w[a]=(uint8_t)pending;w[a+1]=(uint8_t)(pending>>8);return TA_VS_OK;
}

int ta_vs_attack3_accept(TaVsSession*s,uint8_t*w,size_t n,unsigned player){
 const TaVsEvent*e=ta_vs_peek(s);if(!e)return TA_VS_NOT_READY;
 uint32_t seq=e->sequence;int result=ta_vs_attack3_apply(w,n,player,e);
 return result==TA_VS_OK?ta_vs_commit(s,seq):result;
}
