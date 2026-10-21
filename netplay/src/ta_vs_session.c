#include "ta_vs_session.h"
#include <string.h>
#define HELLO 1u
#define EVENT 2u
#define ACK 3u
#define SNAPSHOT 4u
#define HEAD 32u
static uint16_t r16(const uint8_t*p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint32_t r32(const uint8_t*p){return ((uint32_t)r16(p)<<16)|r16(p+2);}
static uint64_t r64(const uint8_t*p){return ((uint64_t)r32(p)<<32)|r32(p+4);}
static void w16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void w32(uint8_t*p,uint32_t v){w16(p,(uint16_t)(v>>16));w16(p+2,(uint16_t)v);}
static void w64(uint8_t*p,uint64_t v){w32(p,(uint32_t)(v>>32));w32(p+4,(uint32_t)v);}
static uint32_t checksum(const uint8_t*p,size_t n){
 uint32_t crc=~0u;
 for(size_t i=0;i<n;i++){if(i>=28&&i<32)continue;crc^=p[i];for(unsigned k=0;k<8;k++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}
 return ~crc;
}
static int nonzero(const uint8_t*p,size_t n){unsigned v=0;while(n--)v|=*p++;return v!=0;}
static int tag_ok(unsigned t){return t==TA_VS_ATTACK||t==TA_VS_RESULT;}
int ta_vs_ready(const TaVsSession*s){return s&&s->peerHello&&s->helloAcked;}
TaVsResult ta_vs_init(TaVsSession*s,const TaVsIdentity*i,unsigned role){
 if(!s||!i||role>1||!i->match||!nonzero(i->rom,32)||!nonzero(i->rules,32))return TA_VS_BAD_ARGUMENT;
 TaVsIdentity copy=*i;
 memset(s,0,sizeof *s);s->identity=copy;s->role=(uint8_t)role;return TA_VS_OK;
}
unsigned ta_vs_pending(const TaVsSession*s){return s?(unsigned)(s->txLast-s->txAck):0;}
TaVsResult ta_vs_enqueue(TaVsSession*s,unsigned tag,const void*data,size_t n,uint32_t*sequence){
 if(!s||!tag_ok(tag)||n>TA_VS_EVENT_BYTES||(n&&!data))return TA_VS_BAD_ARGUMENT;
 if(!ta_vs_ready(s))return TA_VS_NOT_READY;
 if(s->txLast==UINT32_MAX)return TA_VS_SEQUENCE_EXHAUSTED;
 if(ta_vs_pending(s)>=TA_VS_WINDOW)return TA_VS_QUEUE_FULL;
 uint32_t seq=s->txLast+1;TaVsSlot*t=s->tx+(seq%TA_VS_WINDOW);
 memset(t,0,sizeof *t);t->valid=1;t->event.sequence=seq;t->event.tag=(uint16_t)tag;t->event.size=(uint16_t)n;
 if(n)memcpy(t->event.data,data,n);
 s->txLast=seq;
 if(sequence)*sequence=seq;
 return TA_VS_OK;
}
TaVsResult ta_vs_set_snapshot(TaVsSession*s,const void*data,size_t n){
 if(!s||!data||!n||n>TA_VS_SNAPSHOT_BYTES)return TA_VS_BAD_ARGUMENT;
 if(s->snapshotRevision==UINT32_MAX)return TA_VS_SEQUENCE_EXHAUSTED;
 memcpy(s->snapshot,data,n);s->snapshotSize=(uint16_t)n;++s->snapshotRevision;return TA_VS_OK;
}
static size_t header(TaVsSession*s,uint32_t now,uint8_t*p,unsigned kind,uint32_t seq,size_t n){
 memcpy(p,"TAVS",4);p[4]=1;p[5]=(uint8_t)kind;p[6]=s->role;p[7]=s->peerHello?1:0;
 w64(p+8,s->identity.match);w32(p+16,seq);w32(p+20,s->rxApplied);w16(p+24,(uint16_t)n);w16(p+26,0);w32(p+28,0);
 w32(p+28,checksum(p,HEAD+n));s->lastPacket=now;s->ackPending=0;return HEAD+n;
}
size_t ta_vs_packet(TaVsSession*s,uint32_t now,void*out,size_t cap){
 if(!s||!out||cap<HEAD+68)return 0;
 uint8_t*p=out;
 if(s->helloReply||!s->helloSent||(!ta_vs_ready(s)&&(uint32_t)(now-s->lastHello)>=TA_VS_HELLO_MS)){
  memcpy(p+HEAD,s->identity.rom,32);memcpy(p+HEAD+32,s->identity.rules,32);w32(p+HEAD+64,s->identity.seed);
  s->helloReply=0;s->helloSent=1;s->lastHello=now;return header(s,now,p,HELLO,0,68);
 }
 if(!ta_vs_ready(s))return 0;
 /* Prefer unsent events, then the oldest retry due. All queues are bounded. */
 for(unsigned pass=0;pass<2;pass++)for(uint32_t d=1;d<=ta_vs_pending(s);d++){
  uint32_t seq=s->txAck+d;TaVsSlot*t=s->tx+(seq%TA_VS_WINDOW);
  if(!t->valid||t->event.sequence!=seq)continue;
  if((!pass&&!t->sent)||(pass&&t->sent&&(uint32_t)(now-t->lastSent)>=TA_VS_RETRY_MS)){
   size_t n=4u+t->event.size;if(cap<HEAD+n)return 0;
   w16(p+HEAD,t->event.tag);w16(p+HEAD+2,t->event.size);memcpy(p+HEAD+4,t->event.data,t->event.size);
   t->sent=1;t->lastSent=now;if(seq>s->txHighestSent)s->txHighestSent=seq;
   return header(s,now,p,EVENT,seq,n);
  }
 }
 if(s->ackPending)return header(s,now,p,ACK,0,0);
 if(s->snapshotSize&&s->snapshotRevision!=s->snapshotSent){
  if(cap<HEAD+s->snapshotSize)return 0;
  memcpy(p+HEAD,s->snapshot,s->snapshotSize);
  s->snapshotSent=s->snapshotRevision;return header(s,now,p,SNAPSHOT,s->snapshotRevision,s->snapshotSize);
 }
 if((uint32_t)(now-s->lastPacket)>=TA_VS_KEEPALIVE_MS)return header(s,now,p,ACK,0,0);
 return 0;
}
static void acknowledge(TaVsSession*s,uint32_t ack){
 if(ack<=s->txAck)return;
 for(uint32_t d=1;d<=ack-s->txAck;d++){TaVsSlot*t=s->tx+((s->txAck+d)%TA_VS_WINDOW);memset(t,0,sizeof *t);}
 s->txAck=ack;
}
TaVsResult ta_vs_receive(TaVsSession*s,uint32_t now,const void*data,size_t n){
 if(!s||!data)return TA_VS_BAD_ARGUMENT;
 if(n<HEAD||n>TA_VS_PACKET_BYTES)return TA_VS_BAD_PACKET;
 const uint8_t*p=data;unsigned kind=p[5];uint32_t seq=r32(p+16),ack=r32(p+20);size_t payload=r16(p+24);
 if(memcmp(p,"TAVS",4)||p[4]!=1||p[6]>1||(p[7]&~1u)||r16(p+26)||payload!=n-HEAD||r32(p+28)!=checksum(p,n))return TA_VS_BAD_PACKET;
 if(p[6]==s->role||r64(p+8)!=s->identity.match)return TA_VS_WRONG_SESSION;
 if(kind==HELLO){
  if(seq||payload!=68)return TA_VS_BAD_PACKET;
  if(memcmp(p+HEAD,s->identity.rom,32)||memcmp(p+HEAD+32,s->identity.rules,32)||r32(p+HEAD+64)!=s->identity.seed)return TA_VS_IDENTITY_MISMATCH;
 }else{
  if(kind==EVENT){if(!seq||payload<4||payload>4+TA_VS_EVENT_BYTES||r16(p+HEAD+2)!=payload-4||!tag_ok(r16(p+HEAD)))return TA_VS_BAD_PACKET;}
  else if(kind==ACK){if(seq||payload)return TA_VS_BAD_PACKET;}
  else if(kind==SNAPSHOT){if(!seq||!payload||payload>TA_VS_SNAPSHOT_BYTES)return TA_VS_BAD_PACKET;}
  else return TA_VS_BAD_PACKET;
  if(!s->peerHello||(!s->helloAcked&&!(p[7]&1)))return TA_VS_NOT_READY;
 }
 if(ack>s->txHighestSent)return TA_VS_BAD_ACK;
 if(kind==EVENT&&seq>s->rxApplied){
  if(seq-s->rxApplied>TA_VS_WINDOW)return TA_VS_OUTSIDE_WINDOW;
  const TaVsSlot*r=s->rx+(seq%TA_VS_WINDOW);
  if(r->valid&&(r->event.sequence!=seq||r->event.tag!=r16(p+HEAD)||r->event.size!=payload-4||memcmp(r->event.data,p+HEAD+4,payload-4)))return TA_VS_CONFLICT;
 }
 /* No mutations above this point: malformed or incompatible input cannot
  * acknowledge unsent attacks, alter readiness, or poison a receive slot. */
 acknowledge(s,ack);s->lastReceive=now;s->everReceived=1;
 if(kind==HELLO){if(!s->peerHello||!(p[7]&1))s->helloReply=1;s->peerHello=1;}
 if(s->peerHello&&(p[7]&1))s->helloAcked=1;
 if(kind==EVENT){
  if(seq>s->rxApplied){TaVsSlot*r=s->rx+(seq%TA_VS_WINDOW);if(!r->valid){memset(r,0,sizeof *r);r->valid=1;r->event.sequence=seq;r->event.tag=r16(p+HEAD);r->event.size=(uint16_t)(payload-4);memcpy(r->event.data,p+HEAD+4,r->event.size);}}
  s->ackPending=1;
 }else if(kind==SNAPSHOT&&seq>s->remoteRevision){memcpy(s->remoteSnapshot,p+HEAD,payload);s->remoteSize=(uint16_t)payload;s->remoteRevision=seq;}
 return TA_VS_OK;
}
const TaVsEvent*ta_vs_peek(const TaVsSession*s){
 if(!ta_vs_ready(s)||s->rxApplied==UINT32_MAX)return NULL;
 const TaVsSlot*r=s->rx+((s->rxApplied+1)%TA_VS_WINDOW);
 return r->valid&&r->event.sequence==s->rxApplied+1?&r->event:NULL;
}
TaVsResult ta_vs_commit(TaVsSession*s,uint32_t seq){
 if(!s)return TA_VS_BAD_ARGUMENT;
 const TaVsEvent*e=ta_vs_peek(s);
 if(!e||e->sequence!=seq)return TA_VS_BAD_ARGUMENT;
 memset(s->rx+(seq%TA_VS_WINDOW),0,sizeof(TaVsSlot));s->rxApplied=seq;s->ackPending=1;return TA_VS_OK;
}
const uint8_t*ta_vs_remote_snapshot(const TaVsSession*s,size_t*n,uint32_t*revision){
 if(!s)return NULL;
 if(n)*n=s->remoteSize;
 if(revision)*revision=s->remoteRevision;
 return s->remoteRevision?s->remoteSnapshot:NULL;
}
