#include "ta_vs_party.h"
#include <string.h>
static uint32_t r32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t r64(const uint8_t*p){return ((uint64_t)r32(p)<<32)|r32(p+4);}
static void w32(uint8_t*p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static void w64(uint8_t*p,uint64_t n){w32(p,(uint32_t)(n>>32));w32(p+4,(uint32_t)n);}
unsigned ta_vs_party_count(unsigned m){unsigned n=0;for(;m;m>>=1)n+=m&1;return n;}
static void namecopy(char*out,const char*in){unsigned k=0;for(;k<8&&in[k];k++){unsigned c=(unsigned char)in[k];out[k]=(char)(c>=32&&c<=126?c:'?');}for(;k<9;k++)out[k]=0;}
void ta_vs_party_init(TaVsParty*p,const TaVsIdentity*i,uint64_t nonce,const char*name,uint32_t now){memset(p,0,sizeof *p);p->identity=*i;p->nonce=nonce;p->self=p->host=p->target=255;p->speed=5;p->lastServer=now;namecopy(p->name,name);}
void ta_vs_party_host(TaVsParty*p,unsigned speed,unsigned mode){if(!p->room&&speed>=1&&speed<=10&&mode<=1){p->request=TA_PARTY_CREATE;p->speed=(uint8_t)speed;p->mode=(uint8_t)mode;p->requestSent=0;}}
void ta_vs_party_join(TaVsParty*p,uint32_t room){if(!p->room&&room){p->request=TA_PARTY_JOIN;p->requestRoom=room;p->requestSent=0;}}
void ta_vs_party_return(TaVsParty*p){if(p&&p->room&&p->phase==3){p->returning=1;p->requestSent=0;}}
void ta_vs_party_speed(TaVsParty*p,unsigned speed){if(p&&p->room&&(p->phase==0||p->phase==3)&&speed>=1&&speed<=10){p->speedRequest=(uint8_t)speed;p->start=0;p->requestSent=0;}}
static size_t head(TaVsParty*p,uint8_t*b,size_t cap,unsigned op,unsigned n){if(cap<32u+n)return 0;memcpy(b,"TAP4",4);b[4]=1;b[5]=(uint8_t)op;b[6]=(uint8_t)(n>>8);b[7]=(uint8_t)n;w64(b+8,p->nonce);memcpy(b+16,p->cookie,16);return 32u+n;}
static unsigned opponent(const TaVsParty*p,unsigned after){for(unsigned k=1;k<=4;k++){unsigned s=(after+k)%4;if(s!=p->self&&(p->alive&(1u<<s)))return s;}return 255;}
void ta_vs_party_focus(TaVsParty*p,int x,int y){
 unsigned neutral=x<9000&&x> -9000&&y<9000&&y> -9000;if(neutral){p->stickHeld=0;return;}
 if(p->mode!=TA_PARTY_FOCUS||p->stickHeld)return;
 if(x<16000&&x> -16000&&y<16000&&y> -16000)return;p->stickHeld=1;
 unsigned current=p->target<4?p->target:p->self;
 int forward=(y>16000||x>16000)?1:-1;
 for(unsigned k=1;k<=4;k++){unsigned s=(unsigned)((int)current+forward*(int)k+8)%4;if(s!=p->self&&(p->alive&(1u<<s))){p->target=(uint8_t)s;break;}}
}
size_t ta_vs_party_packet(TaVsParty*p,uint32_t now,uint8_t*b,size_t cap){
 if(!p||!b||p->error||cap<TA_PARTY_BYTES)return 0;
 if(p->leaving)return head(p,b,cap,TA_PARTY_LEAVE,0);
 if(p->haveCookie&&p->authState==3)return 0; /* Wait for endpoint-bound account ticket. */
 if(!p->requestSent||(uint32_t)(now-p->lastSend)>=250u){
  p->requestSent=1;p->lastSend=now;unsigned op=!p->haveCookie?TA_PARTY_HELLO:p->room?(p->returning&&p->phase==3?TA_PARTY_RETURN:p->start&&p->phase==0?TA_PARTY_START:TA_PARTY_KEEP):p->request?p->request:TA_PARTY_LIST;
  if(p->haveCookie&&p->capability>=2&&p->authState==1)op=TA_PARTY_AUTH;
  else if(p->room&&p->speedRequest&&(p->phase==0||p->phase==3))op=TA_PARTY_SPEED;
  unsigned n=op==TA_PARTY_AUTH?76:op==TA_PARTY_SPEED?5:op==TA_PARTY_CREATE?74:op==TA_PARTY_JOIN?76:op==TA_PARTY_LIST?64:op==TA_PARTY_KEEP?(p->capability>=2?6:1):op==TA_PARTY_RETURN||op==TA_PARTY_START?(p->capability>=2?5:op==TA_PARTY_RETURN?4:0):0;
  size_t bytes=head(p,b,cap,op,n);if(!bytes)return 0;
  if(op==TA_PARTY_CREATE){memcpy(b+32,p->identity.rom,32);memcpy(b+64,p->identity.rules,32);b[96]=p->speed;b[97]=p->mode;memcpy(b+98,p->name,8);}
  if(op==TA_PARTY_JOIN){w32(b+32,p->requestRoom);memcpy(b+36,p->identity.rom,32);memcpy(b+68,p->identity.rules,32);memcpy(b+100,p->name,8);}
  if(op==TA_PARTY_LIST){memcpy(b+32,p->identity.rom,32);memcpy(b+64,p->identity.rules,32);}
  if(op==TA_PARTY_AUTH)memcpy(b+32,p->authTicket,76);if(op==TA_PARTY_SPEED){w32(b+32,p->epoch);b[36]=p->speedRequest;}
  if(op==TA_PARTY_RETURN||op==TA_PARTY_START){if(n>=4)w32(b+32,p->epoch);if(n==5)b[36]=p->speed;}if(op==TA_PARTY_KEEP){b[32]=p->flags;if(n==6){w32(b+33,p->epoch);b[37]=p->speed;}}return bytes;
 }
 if(p->linked)for(unsigned k=0;k<4;k++){unsigned s=p->txRotation++%4;if(s==p->self||!(p->members&(1u<<s)))continue;
  size_t n=ta_vs_packet(p->peer+s,now,b+33,cap-33);if(n){head(p,b,cap,TA_PARTY_DATA,(unsigned)n+1);b[32]=(uint8_t)s;return 33+n;}}
 return 0;
}
static int linkpeers(TaVsParty*p){
 for(unsigned s=0;s<4;s++)if(s!=p->self&&(p->members&(1u<<s))){TaVsIdentity id=p->identity;unsigned lo=s<p->self?s:p->self,hi=s>p->self?s:p->self;
  id.match=p->roomNonce^((uint64_t)(1+lo*4+hi)*UINT64_C(0x9e3779b97f4a7c15))^((uint64_t)p->epoch*UINT64_C(0xd1b54a32d192ed03));if(!id.match)id.match=1;id.seed=p->seed;
  int r=ta_vs_init(p->peer+s,&id,p->self>s);if(r)return r;p->peer[s].applicationRound=p->epoch;}
 p->linked=1;p->target=(uint8_t)opponent(p,p->self);return 0;
}
int ta_vs_party_receive(TaVsParty*p,uint32_t now,const uint8_t*b,size_t n){
 if(!p||!b||n<32||n>TA_PARTY_BYTES||memcmp(b,"TAP4",4)||b[4]!=1||(((unsigned)b[6]<<8)|b[7])!=n-32||r64(b+8)!=p->nonce)return TA_VS_BAD_PACKET;
 unsigned op=b[5];const uint8_t*d=b+32;unsigned len=(unsigned)n-32;
 if(op==TA_PARTY_COOKIE){unsigned nz=0;for(unsigned k=0;k<16;k++)nz|=b[16+k];if((!len||(len==1&&d[0]==1))&&nz){p->error=-304;return p->error;}if(len!=1||(d[0]!=1&&d[0]!=2)||!nz||(p->haveCookie&&memcmp(b+16,p->cookie,16)))return TA_VS_BAD_PACKET;memcpy(p->cookie,b+16,16);p->haveCookie=1;p->capability=d[0];p->requestSent=0;p->lastServer=now;return 0;}
 if(!p->haveCookie||memcmp(b+16,p->cookie,16))return TA_VS_BAD_PACKET;
 if(op==TA_PARTY_AUTH_OK){if(len!=1||d[0]>1)return TA_VS_BAD_PACKET;p->authenticated=d[0];p->authState=2;memset(p->authTicket,0,76);p->requestSent=0;p->lastServer=now;return 0;}
 if(op==TA_PARTY_ERROR){if(len!=1)return TA_VS_BAD_PACKET;p->error=-(300+(int)d[0]);return p->error;}
 if(op==TA_PARTY_ROOMS){if(p->room)return 0;if(len<1||d[0]>8||len!=1u+d[0]*16u)return TA_VS_BAD_PACKET;
  for(unsigned k=0;k<d[0];k++){const uint8_t*q=d+1+k*16;if(!r32(q)||q[4]<1||q[4]>3||q[5]<1||q[5]>10||q[6]>1||q[7])return TA_VS_BAD_PACKET;}
  p->roomCount=d[0];for(unsigned k=0;k<p->roomCount;k++){const uint8_t*q=d+1+k*16;TaPartyRoom*r=p->rooms+k;r->id=r32(q);r->count=q[4];r->speed=q[5];r->mode=q[6];memcpy(r->name,q+8,8);r->name[8]=0;}}
 else if(op==TA_PARTY_STATE){
  if((len!=65&&len!=72)||!r32(d)||!r64(d+4)||!r32(d+20)||d[24]>3||d[25]>3||d[26]<1||d[26]>10||d[27]>1||d[28]>15||d[29]>15||d[30]>15||d[31]>15||d[32]>3||!(d[28]&(1u<<d[24]))||!(d[28]&(1u<<d[25]))||((d[29]|d[30]|d[31])&~d[28]))return TA_VS_BAD_PACKET;
  if(len==72&&(d[65]>15||!(d[65]&(1u<<d[24]))||!(d[65]&(1u<<d[25]))||(d[65]&~d[28])||d[70]>1||d[71]>1))return TA_VS_BAD_PACKET;
  if(len==72)for(unsigned i=0;i<4;i++)if(d[66+i]>10)return TA_VS_BAD_PACKET;
  if(p->room&&(p->room!=r32(d)||p->roomNonce!=r64(d+4)||p->self!=d[24]))return TA_VS_WRONG_SESSION;
  /* KEEP replies deliberately retain the room revision while everyone waits.
     A validated reply still proves the relay is alive; do not expire an idle
     lobby just because nobody has changed ready state. */
  if(p->revision&&r32(d+20)<=p->revision){p->lastServer=now;return 0;}
  /* Only a confirmed result vote can move this fixed room to a new round.
     A new epoch is also part of every peer session match ID. */
  unsigned reset=p->phase==3&&p->returning&&((d[32]==0&&r32(d+16)==p->epoch)||(d[32]==1&&r32(d+16)==p->epoch+1u));
  if(reset){memset(p->peer,0,sizeof p->peer);memset(p->inbox,0,sizeof p->inbox);p->linked=p->flags=p->start=p->returning=p->stickHeld=p->rotation=p->txRotation=p->rxRotation=0;p->requestSent=0;}
  if(p->linked&&(p->epoch!=r32(d+16)||p->seed!=r32(d+12)||p->members!=d[28]||d[32]<p->phase))return TA_VS_CONFLICT;
  if(d[32]&&(!r32(d+16)||ta_vs_party_count(d[28])<2))return TA_VS_BAD_PACKET;
  if(p->room&&p->speed!=d[26]){if(p->phase==3)p->returning=0;else if(p->phase==0){p->flags&=~1u;p->start=0;p->requestSent=0;}}
  p->room=r32(d);p->roomNonce=r64(d+4);p->seed=r32(d+12);p->epoch=r32(d+16);p->revision=r32(d+20);
  p->self=d[24];p->host=d[25];p->speed=d[26];p->mode=d[27];p->members=d[28];p->alive=d[29];p->ready=d[30];p->loaded=d[31];p->phase=d[32];
  p->present=len==72?d[65]:p->members;p->ranked=len==72?d[70]:0;p->authenticated=len==72?d[71]:p->authenticated;
  for(unsigned i=0;i<4;i++)p->speedRequests[i]=len==72?d[66+i]:0;
  if(p->speedRequest&&((p->self==p->host&&p->speed==p->speedRequest)||(p->self!=p->host&&p->speedRequests[p->self]==p->speedRequest)))p->speedRequest=0;
  for(unsigned k=0;k<4;k++){memcpy(p->names[k],d+33+k*8,8);p->names[k][8]=0;}
  if((p->phase==1||p->phase==2)&&!p->linked){int r=linkpeers(p);if(r)return r;}if(p->target>=4||!(p->alive&(1u<<p->target)))p->target=(uint8_t)opponent(p,p->self);
 }
 else if(op==TA_PARTY_DATA){if(len<33||d[0]>3||d[0]==p->self||!(p->members&(1u<<d[0])))return TA_VS_BAD_PACKET;
  if(!p->linked)return TA_VS_NOT_READY;
  int r=ta_vs_receive(p->peer+d[0],now,d+1,len-1);if(r)return r;
 }else return TA_VS_BAD_PACKET;
 p->lastServer=now;return 0;
}
unsigned ta_vs_party_next_target(const TaVsParty*p){
 if(!p)return 4;unsigned target=p->mode==TA_PARTY_FOCUS?p->target:opponent(p,p->rotation);
 if(target>=4||target==p->self||!(p->alive&(1u<<target)))target=opponent(p,p->self);
 return target<4?target:4;
}
unsigned ta_vs_party_target(void*context){
 TaVsParty*p=(TaVsParty*)context;unsigned target=ta_vs_party_next_target(p);
 if(target<4){p->rotation=(uint8_t)target;p->target=(uint8_t)target;}return target;
}

TaVsResult ta_vs_party_attack_to(void*context,unsigned type,unsigned value,uint32_t frame,unsigned target){
 TaVsParty*p=(TaVsParty*)context;if(!p||p->phase!=2)return TA_VS_NOT_READY;
 /* An eliminated target consumes its already-addressed attacks. It does not
    redistribute them and change the winner's pending garbage unexpectedly. */
 if(target>=4||target==p->self||!(p->alive&(1u<<target)))return TA_VS_OK;
 return ta_vs_attack_send(p->peer+target,type,value,frame,0);
}
TaVsResult ta_vs_party_attack(void*context,unsigned type,unsigned value,uint32_t frame){
 if(!context)return TA_VS_BAD_ARGUMENT;return ta_vs_party_attack_to(context,type,value,frame,ta_vs_party_target(context));
}
int ta_vs_party_deliver(TaVsParty*p,uint8_t*w,size_t n){
 if(!p||!w||n!=131072)return TA_VS_BAD_ARGUMENT;
 /* One fair visit per source per local game tick. A busy chain stays queued,
    while another source's ordinary garbage may still reach its own mailbox. */
 unsigned first=p->rxRotation++%4;
 for(unsigned k=0;k<4;k++){unsigned s=(first+k)%4;if(s==p->self||!(p->members&(1u<<s)))continue;
  TaVsSession*session=p->peer+s;const TaVsEvent*e;
  while((e=ta_vs_peek(session))!=0){if(e->tag!=TA_VS_ATTACK)return TA_VS_BAD_PACKET;TaVsAttack a;int r=ta_vs_attack_decode(e,&a);if(r)return r;
   if(a.round!=p->epoch)return TA_VS_WRONG_SESSION;r=ta_vs_attack_stage(session,p->inbox+s);if(r==TA_VS_QUEUE_FULL)break;if(r)return r;}
  for(unsigned work=0;work<TA_VS_WINDOW;work++){uint32_t seq=0;for(unsigned j=0;j<TA_VS_WINDOW;j++)if(p->inbox[s].used[j]&&(!seq||p->inbox[s].attack[j].sequence<seq))seq=p->inbox[s].attack[j].sequence;
   if(!seq)break;int r=ta_vs_attack_deliver(p->inbox+s,seq,w,n,0);if(r==TA_VS_ATTACK_BUSY)break;if(r)return r;}
 }return 0;
}
