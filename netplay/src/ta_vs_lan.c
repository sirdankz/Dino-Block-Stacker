#include "ta_vs_lan.h"
#include <string.h>
#define DISC 96u
static uint32_t r32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t r64(const uint8_t*p){return ((uint64_t)r32(p)<<32)|r32(p+4);}
static void w32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void w64(uint8_t*p,uint64_t v){w32(p,(uint32_t)(v>>32));w32(p+4,(uint32_t)v);}
static uint32_t crc(const uint8_t*p,size_t n){uint32_t v=~0u;while(n--){v^=*p++;for(unsigned k=0;k<8;k++)v=(v>>1)^((0u-(v&1u))&0xedb88320u);}return ~v;}
int ta_vs_lan_init(TaVsLan*l,const TaVsIdentity*i,unsigned role,uint64_t nonce,uint32_t now){
 if(!l||!i||role>1||!nonce)return TA_VS_BAD_ARGUMENT;
 TaVsSession check;TaVsIdentity copy=*i;copy.match=nonce;if(ta_vs_init(&check,&copy,role))return TA_VS_BAD_ARGUMENT;
 memset(l,0,sizeof *l);l->identity=copy;l->role=(uint8_t)role;l->nonce=nonce;l->clientNonce=role?nonce:0;l->started=now;return 0;
}
int ta_vs_lan_ready(const TaVsLan*l){return l&&l->sessionLive&&ta_vs_ready(&l->session);}
int ta_vs_lan_timed_out(const TaVsLan*l,uint32_t now){return l&&l->sessionLive&&l->session.everReceived&&(uint32_t)(now-l->session.lastReceive)>5000u;}
int ta_vs_lan_receive(TaVsLan*l,uint32_t now,uint32_t address,uint16_t port,const uint8_t*p,size_t n){
 if(!l||!p||!address||address==UINT32_MAX||!port)return TA_VS_BAD_ARGUMENT;
 if(n>=4&&!memcmp(p,"TAVS",4)){
  if(!l->pinned||!l->sessionLive||address!=l->peerAddress||port!=l->peerPort)return TA_VS_WRONG_SESSION;
  return ta_vs_receive(&l->session,now,p,n);
 }
 if(n!=DISC||memcmp(p,"TALN",4)||p[4]!=1||p[7]||p[6]>1||p[6]==l->role||r32(p+92)!=crc(p,92))return TA_VS_BAD_PACKET;
 if(memcmp(p+28,l->identity.rom,32)||memcmp(p+60,l->identity.rules,32))return TA_VS_IDENTITY_MISMATCH;
 unsigned kind=p[5];uint64_t client=r64(p+8),match=r64(p+16);uint32_t seed=r32(p+24);
 if(!client)return TA_VS_BAD_PACKET;
 if(l->pinned&&(address!=l->peerAddress||port!=l->peerPort||client!=l->clientNonce))return TA_VS_WRONG_SESSION;
 if(!l->role){
  if(kind==1){if(match||seed)return TA_VS_BAD_PACKET;if(!l->pinned){l->peerAddress=address;l->peerPort=port;l->clientNonce=client;l->pinned=1;l->sentOffer=0;}return 0;}
  if(kind!=3||!l->pinned||match!=l->identity.match||seed!=l->identity.seed)return TA_VS_BAD_PACKET;
  if(!l->sessionLive){int result=ta_vs_init(&l->session,&l->identity,0);if(result)return result;l->sessionLive=1;}
  return 0;
 }
 if(kind!=2||client!=l->nonce||!match)return TA_VS_BAD_PACKET;
 if(l->pinned&&(match!=l->identity.match||seed!=l->identity.seed))return TA_VS_WRONG_SESSION;
 if(!l->pinned){l->identity.match=match;l->identity.seed=seed;l->peerAddress=address;l->peerPort=port;l->pinned=1;l->sentOffer=0;int result=ta_vs_init(&l->session,&l->identity,1);if(result)return result;l->sessionLive=1;}
 return 0;
}
size_t ta_vs_lan_packet(TaVsLan*l,uint32_t now,uint8_t*p,size_t cap,uint32_t*address,uint16_t*port){
 if(!l||!p||!address||!port||cap<TA_VS_PACKET_BYTES)return 0;
 if(!ta_vs_lan_ready(l)&&(!l->sentOffer||(uint32_t)(now-l->lastOffer)>=250u)){
  if(!l->role&&!l->pinned)return 0;
  unsigned kind=l->role?(l->pinned?3:1):2;
  memset(p,0,DISC);memcpy(p,"TALN",4);p[4]=1;p[5]=(uint8_t)kind;p[6]=l->role;
  w64(p+8,l->clientNonce);if(kind!=1){w64(p+16,l->identity.match);w32(p+24,l->identity.seed);}
  memcpy(p+28,l->identity.rom,32);memcpy(p+60,l->identity.rules,32);w32(p+92,crc(p,92));
  *address=l->pinned?l->peerAddress:UINT32_MAX;*port=l->pinned?l->peerPort:TA_VS_LAN_PORT;l->lastOffer=now;l->sentOffer=1;return DISC;
 }
 if(!l->sessionLive)return 0;
 size_t n=ta_vs_packet(&l->session,now,p,cap);if(n){*address=l->peerAddress;*port=l->peerPort;}return n;
}
