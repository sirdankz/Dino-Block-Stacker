#include "ta_vs_public.h"
#include <string.h>
static unsigned r16(const uint8_t*p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t r32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t r64(const uint8_t*p){return ((uint64_t)r32(p)<<32)|r32(p+4);}
static void w16(uint8_t*p,unsigned x){p[0]=(uint8_t)(x>>8);p[1]=(uint8_t)x;}
static void w32(uint8_t*p,uint32_t x){p[0]=(uint8_t)(x>>24);p[1]=(uint8_t)(x>>16);p[2]=(uint8_t)(x>>8);p[3]=(uint8_t)x;}
static void w64(uint8_t*p,uint64_t x){w32(p,(uint32_t)(x>>32));w32(p+4,(uint32_t)x);}
static uint32_t addr(const uint8_t*p){uint32_t x;memcpy(&x,p,4);return x;}
static int nonzero(const uint8_t*p){unsigned x=0;for(unsigned k=0;k<16;k++)x|=p[k];return x!=0;}
static size_t head(uint8_t*p,size_t cap,unsigned op,uint64_t nonce,const uint8_t*key,unsigned n){
 if(!p||cap<32u+n||n>TA_VS_PACKET_BYTES)return 0;
 memcpy(p,"TAP9",4);p[4]=1;p[5]=(uint8_t)op;w16(p+6,n);w64(p+8,nonce);if(key)memcpy(p+16,key,16);else memset(p+16,0,16);return 32u+n;
}
uint32_t ta_vs_public_ipv4(const char*s){uint8_t a[4];unsigned v=0,k=0,digits=0;if(!s)return 0;
 for(;;s++){if(*s>='0'&&*s<='9'){if(++digits>3)return 0;v=v*10+(unsigned)(*s-'0');if(v>255)return 0;}
 else if(*s=='.'||!*s){if(!digits||k>=4)return 0;a[k++]=(uint8_t)v;v=digits=0;if(!*s)break;}else return 0;}if(k!=4)return 0;uint32_t x=addr(a);return x&&x!=UINT32_MAX?x:0;
}
void ta_vs_public_init(TaVsPublic*p,const TaVsIdentity*i,uint64_t nonce,uint32_t server,uint16_t port,uint16_t localPort,uint32_t now){
 memset(p,0,sizeof *p);p->serverAddress=server;p->serverPort=port;p->localPort=localPort;p->nonce=nonce;p->mode=TA_PUB_BROWSE;p->role=1;p->speed=5;p->phase=1;p->roundEpoch=1;p->started=p->lastServer=now;memcpy(p->identity,i->rom,32);memcpy(p->identity+32,i->rules,32);
}
void ta_vs_public_host(TaVsPublic*p){if(!p->haveRoom){p->mode=TA_PUB_HOST;p->role=0;p->requested=0;}}
int ta_vs_public_join(TaVsPublic*p,uint32_t room){if(!room||p->haveRoom)return -1;p->mode=TA_PUB_JOINING;p->role=1;p->joinRoom=room;p->requested=0;return 0;}
size_t ta_vs_public_packet(TaVsPublic*p,uint32_t now,uint8_t*out,size_t cap,uint32_t*to,uint16_t*port){
 if(!p||!out||!to||!port||(p->error&&!p->leaving))return 0;*to=p->serverAddress;*port=p->serverPort;
 if(p->leaving){if(p->leaveAck)return 0;if(p->haveCookie&&(!p->requested||(uint32_t)(now-p->lastRequest)>=250)){p->requested=1;p->lastRequest=now;return head(out,cap,TA_PUB_LEAVE,p->nonce,p->cookie,0);}return 0;}
 if(!p->haveCookie){if(!p->requested||(uint32_t)(now-p->lastRequest)>=500){p->requested=1;p->lastRequest=now;return head(out,cap,TA_PUB_HELLO,p->nonce,NULL,0);}return 0;}
 if(p->direct&&(uint32_t)(now-p->lastDirect)>1800){p->direct=p->seen=p->peerSeen=p->seenLan=p->peerSeenLan=0;}
 if(p->haveRoom&&(!p->kept||(uint32_t)(now-p->lastKeep)>=1000)){
  unsigned bytes=1;
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
  bytes=6;
#endif
  size_t n=head(out,cap,TA_PUB_KEEP,p->nonce,p->cookie,bytes);if(n){out[32]=p->speed;
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
   out[33]=p->phase;w32(out+34,p->roundEpoch);
#endif
p->kept=1;p->lastKeep=now;}return n;
 }
 if(p->havePeer&&(!p->punched||(uint32_t)(now-p->lastPunch)>=120)){
  uint32_t candidate=p->punchLocal&&p->peerLocalAddress?p->peerLocalAddress:p->peerAddress;
  uint16_t candidatePort=p->punchLocal&&p->peerLocalAddress?p->peerLocalPort:p->peerPort;
  size_t n=head(out,cap,TA_PUB_PUNCH,p->roomNonce,p->key,1);if(n){out[32]=(uint8_t)((p->punchLocal&&p->peerLocalAddress?p->seenLan:p->seen)>=3);*to=candidate;*port=candidatePort;p->punchLocal^=1;p->punched=1;p->lastPunch=now;}return n;
 }
 if(!p->haveRoom&&(!p->requested||(uint32_t)(now-p->lastRequest)>=500)){
  unsigned op=p->mode==TA_PUB_HOST?TA_PUB_CREATE:p->mode==TA_PUB_JOINING?TA_PUB_JOIN:TA_PUB_LIST;
  unsigned bytes=op==TA_PUB_CREATE?71:op==TA_PUB_JOIN?74:64;size_t n=head(out,cap,op,p->nonce,p->cookie,bytes);if(!n)return 0;
  unsigned a=32;if(op==TA_PUB_JOIN){w32(out+a,p->joinRoom);a+=4;}memcpy(out+a,p->identity,64);a+=64;
  if(op==TA_PUB_CREATE)out[a++]=p->speed;
  if(op!=TA_PUB_LIST){memcpy(out+a,&p->localAddress,4);w16(out+a+4,p->localPort);}
  p->requested=1;p->lastRequest=now;return n;
 }
 return 0;
}
int ta_vs_public_receive(TaVsPublic*p,uint32_t now,uint32_t from,uint16_t port,uint8_t*b,size_t n){
 if(!p||!b||n<32||n>TA_PUB_BYTES||memcmp(b,"TAP9",4)||b[4]!=1||r16(b+6)!=n-32)return -1;
 unsigned op=b[5],bytes=(unsigned)n-32;int server=from==p->serverAddress&&port==p->serverPort;
 if(server){if(r64(b+8)!=p->nonce)return -1;
  if(op==TA_PUB_COOKIE){if(bytes||!nonzero(b+16))return -1;if(p->haveCookie&&memcmp(p->cookie,b+16,16))return -1;memcpy(p->cookie,b+16,16);p->haveCookie=1;p->requested=0;p->lastServer=now;return 0;}
  if(!p->haveCookie||memcmp(p->cookie,b+16,16))return -1;
  if(op==TA_PUB_LEAVE_ACK){if(bytes||!p->leaving)return -1;p->leaveAck=1;p->haveRoom=p->havePeer=p->direct=0;p->error=0;return 0;}
  if(p->leaving)return 0;
  if(op==TA_PUB_ROOMS){if(p->haveRoom||bytes<1||b[32]>64||bytes!=1+b[32]*8)return -1;
   for(unsigned k=0;k<b[32];k++){const uint8_t*q=b+33+k*8;if(!r32(q)||q[4]<1||q[4]>10||q[5]<1||q[5]>2||q[6]>4||q[7])return -1;}
   p->roomCount=b[32];for(unsigned k=0;k<p->roomCount;k++){p->rooms[k].id=r32(b+33+k*8);p->rooms[k].speed=b[37+k*8];p->rooms[k].players=b[38+k*8];p->rooms[k].phase=b[39+k*8];}p->lastServer=now;return 0;
  }
  if(op==TA_PUB_STATE){if(bytes!=44||!r32(b+32)||b[36]!=p->role||b[37]<1||b[37]>10||b[38]>1||b[39]||!r64(b+40))return -1;
   if(p->haveRoom&&(p->room!=r32(b+32)||p->roomNonce!=r64(b+40)))return -1;
   if(p->havePeer&&!b[38])return 0; /* Delayed pre-pair STATE cannot unpair a live route. */
   if(b[38]){if(!addr(b+48)||!r16(b+52)||!nonzero(b+60))return -1;
    if(!p->havePeer||p->peerAddress!=addr(b+48)||p->peerPort!=r16(b+52)||memcmp(p->key,b+60,16)){p->direct=p->seen=p->peerSeen=p->seenLan=p->peerSeenLan=p->punched=0;}
    p->peerAddress=addr(b+48);p->peerPort=(uint16_t)r16(b+52);p->peerLocalAddress=addr(b+54);p->peerLocalPort=(uint16_t)r16(b+58);memcpy(p->key,b+60,16);
   }
   p->room=r32(b+32);p->roomNonce=r64(b+40);p->haveRoom=1;p->havePeer=b[38];if(p->role)p->speed=b[37];p->lastServer=now;return 0;
  }
  if(op==TA_PUB_ERROR){if(bytes!=1)return -1;p->error=-(200+(int)b[32]);p->lastServer=now;return 0;}
  if(op!=TA_PUB_DATA||!p->havePeer||bytes<4)return -1;p->lastServer=now;memmove(b,b+32,bytes);return (int)bytes;
 }
 if(!p->havePeer||r64(b+8)!=p->roomNonce||memcmp(b+16,p->key,16))return -1;
 if(!((from==p->peerAddress&&port==p->peerPort)||(p->peerLocalAddress&&from==p->peerLocalAddress&&port==p->peerLocalPort)))return -1;
 if(op==TA_PUB_PUNCH){if(bytes!=1||b[32]>1)return -1;
  int lan=from==p->peerLocalAddress&&port==p->peerLocalPort;uint8_t*seen=lan?&p->seenLan:&p->seen,*confirmed=lan?&p->peerSeenLan:&p->peerSeen;
  if(*seen<3)(*seen)++;if(b[32])*confirmed=1;
  if(*seen>=3&&*confirmed&&(!p->direct||lan||p->directAddress==from)){p->direct=1;p->directAddress=from;p->directPort=port;}
  if(!p->direct||(from==p->directAddress&&port==p->directPort))p->lastDirect=now;return 0;
 }
 if(op!=TA_PUB_DIRECT||bytes<4)return -1;if(!p->direct||(from==p->directAddress&&port==p->directPort))p->lastDirect=now;memmove(b,b+32,bytes);return (int)bytes;
}
size_t ta_vs_public_game(TaVsPublic*p,uint32_t now,const uint8_t*in,size_t n,uint8_t*out,size_t cap,uint32_t*to,uint16_t*port){
 if(!p||!p->havePeer||!in||n<4||n>TA_VS_PACKET_BYTES||!to||!port)return 0;
 if(p->direct&&(uint32_t)(now-p->lastDirect)>1800)p->direct=p->seen=p->peerSeen=p->seenLan=p->peerSeenLan=0;
 unsigned op=p->direct?TA_PUB_DIRECT:TA_PUB_RELAY;size_t bytes=head(out,cap,op,p->direct?p->roomNonce:p->nonce,p->direct?p->key:p->cookie,(unsigned)n);
 if(bytes){memcpy(out+32,in,n);*to=p->direct?p->directAddress:p->serverAddress;*port=p->direct?p->directPort:p->serverPort;}return bytes;
}
size_t ta_vs_public_leave(TaVsPublic*p,uint8_t*out,size_t cap){return p&&p->haveCookie?head(out,cap,TA_PUB_LEAVE,p->nonce,p->cookie,0):0;}

void ta_vs_public_begin_leave(TaVsPublic*p){if(p){p->leaving=1;p->leaveAck=0;p->requested=0;p->error=0;p->phase=0;}}
