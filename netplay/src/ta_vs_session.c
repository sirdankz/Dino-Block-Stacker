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
static const uint32_t crcTable[256]={
 0x00000000u,0x77073096u,0xee0e612cu,0x990951bau,0x076dc419u,0x706af48fu,0xe963a535u,0x9e6495a3u,
 0x0edb8832u,0x79dcb8a4u,0xe0d5e91eu,0x97d2d988u,0x09b64c2bu,0x7eb17cbdu,0xe7b82d07u,0x90bf1d91u,
 0x1db71064u,0x6ab020f2u,0xf3b97148u,0x84be41deu,0x1adad47du,0x6ddde4ebu,0xf4d4b551u,0x83d385c7u,
 0x136c9856u,0x646ba8c0u,0xfd62f97au,0x8a65c9ecu,0x14015c4fu,0x63066cd9u,0xfa0f3d63u,0x8d080df5u,
 0x3b6e20c8u,0x4c69105eu,0xd56041e4u,0xa2677172u,0x3c03e4d1u,0x4b04d447u,0xd20d85fdu,0xa50ab56bu,
 0x35b5a8fau,0x42b2986cu,0xdbbbc9d6u,0xacbcf940u,0x32d86ce3u,0x45df5c75u,0xdcd60dcfu,0xabd13d59u,
 0x26d930acu,0x51de003au,0xc8d75180u,0xbfd06116u,0x21b4f4b5u,0x56b3c423u,0xcfba9599u,0xb8bda50fu,
 0x2802b89eu,0x5f058808u,0xc60cd9b2u,0xb10be924u,0x2f6f7c87u,0x58684c11u,0xc1611dabu,0xb6662d3du,
 0x76dc4190u,0x01db7106u,0x98d220bcu,0xefd5102au,0x71b18589u,0x06b6b51fu,0x9fbfe4a5u,0xe8b8d433u,
 0x7807c9a2u,0x0f00f934u,0x9609a88eu,0xe10e9818u,0x7f6a0dbbu,0x086d3d2du,0x91646c97u,0xe6635c01u,
 0x6b6b51f4u,0x1c6c6162u,0x856530d8u,0xf262004eu,0x6c0695edu,0x1b01a57bu,0x8208f4c1u,0xf50fc457u,
 0x65b0d9c6u,0x12b7e950u,0x8bbeb8eau,0xfcb9887cu,0x62dd1ddfu,0x15da2d49u,0x8cd37cf3u,0xfbd44c65u,
 0x4db26158u,0x3ab551ceu,0xa3bc0074u,0xd4bb30e2u,0x4adfa541u,0x3dd895d7u,0xa4d1c46du,0xd3d6f4fbu,
 0x4369e96au,0x346ed9fcu,0xad678846u,0xda60b8d0u,0x44042d73u,0x33031de5u,0xaa0a4c5fu,0xdd0d7cc9u,
 0x5005713cu,0x270241aau,0xbe0b1010u,0xc90c2086u,0x5768b525u,0x206f85b3u,0xb966d409u,0xce61e49fu,
 0x5edef90eu,0x29d9c998u,0xb0d09822u,0xc7d7a8b4u,0x59b33d17u,0x2eb40d81u,0xb7bd5c3bu,0xc0ba6cadu,
 0xedb88320u,0x9abfb3b6u,0x03b6e20cu,0x74b1d29au,0xead54739u,0x9dd277afu,0x04db2615u,0x73dc1683u,
 0xe3630b12u,0x94643b84u,0x0d6d6a3eu,0x7a6a5aa8u,0xe40ecf0bu,0x9309ff9du,0x0a00ae27u,0x7d079eb1u,
 0xf00f9344u,0x8708a3d2u,0x1e01f268u,0x6906c2feu,0xf762575du,0x806567cbu,0x196c3671u,0x6e6b06e7u,
 0xfed41b76u,0x89d32be0u,0x10da7a5au,0x67dd4accu,0xf9b9df6fu,0x8ebeeff9u,0x17b7be43u,0x60b08ed5u,
 0xd6d6a3e8u,0xa1d1937eu,0x38d8c2c4u,0x4fdff252u,0xd1bb67f1u,0xa6bc5767u,0x3fb506ddu,0x48b2364bu,
 0xd80d2bdau,0xaf0a1b4cu,0x36034af6u,0x41047a60u,0xdf60efc3u,0xa867df55u,0x316e8eefu,0x4669be79u,
 0xcb61b38cu,0xbc66831au,0x256fd2a0u,0x5268e236u,0xcc0c7795u,0xbb0b4703u,0x220216b9u,0x5505262fu,
 0xc5ba3bbeu,0xb2bd0b28u,0x2bb45a92u,0x5cb36a04u,0xc2d7ffa7u,0xb5d0cf31u,0x2cd99e8bu,0x5bdeae1du,
 0x9b64c2b0u,0xec63f226u,0x756aa39cu,0x026d930au,0x9c0906a9u,0xeb0e363fu,0x72076785u,0x05005713u,
 0x95bf4a82u,0xe2b87a14u,0x7bb12baeu,0x0cb61b38u,0x92d28e9bu,0xe5d5be0du,0x7cdcefb7u,0x0bdbdf21u,
 0x86d3d2d4u,0xf1d4e242u,0x68ddb3f8u,0x1fda836eu,0x81be16cdu,0xf6b9265bu,0x6fb077e1u,0x18b74777u,
 0x88085ae6u,0xff0f6a70u,0x66063bcau,0x11010b5cu,0x8f659effu,0xf862ae69u,0x616bffd3u,0x166ccf45u,
 0xa00ae278u,0xd70dd2eeu,0x4e048354u,0x3903b3c2u,0xa7672661u,0xd06016f7u,0x4969474du,0x3e6e77dbu,
 0xaed16a4au,0xd9d65adcu,0x40df0b66u,0x37d83bf0u,0xa9bcae53u,0xdebb9ec5u,0x47b2cf7fu,0x30b5ffe9u,
 0xbdbdf21cu,0xcabac28au,0x53b39330u,0x24b4a3a6u,0xbad03605u,0xcdd70693u,0x54de5729u,0x23d967bfu,
 0xb3667a2eu,0xc4614ab8u,0x5d681b02u,0x2a6f2b94u,0xb40bbe37u,0xc30c8ea1u,0x5a05df1bu,0x2d02ef8du,
};
static uint32_t checksum(const uint8_t*p,size_t n){
 uint32_t crc=~0u;for(size_t i=0;i<n;i++)if(i<28||i>=32)crc=(crc>>8)^crcTable[(crc^p[i])&255u];return ~crc;
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
