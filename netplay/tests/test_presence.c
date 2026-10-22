/* Real public/account packets through a simulated transport; no live service. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ta_vs_udp.h"
#include "ta_vs_public.h"
#include "ta_vs_account.h"
#include "../../third_party/monocypher/monocypher.h"
#define TA_N17_SERVER "127.0.0.1"
#define TA_N17_PORT 34987u
#define WSAEMSGSIZE 10040
#define XNET_GET_XNADDR_STATIC 1
#define XNET_GET_XNADDR_DHCP 2
#define XNET_GET_XNADDR_AUTO 4
#define XNET_GET_XNADDR_PPPOE 8
#define TA_Log(...) ((void)0)
typedef unsigned long DWORD;
typedef struct {struct {uint32_t s_addr;}ina;} XNADDR;
static uint8_t ta_n17_account_key[32],serverSecret[32]={11};
static struct{char name[16];uint8_t token[32];}savedProfiles[8];
static unsigned profileCount,profileActive,taSocialAccounts,taSocialInGame,taSocialOnline,taSocialCountKnown;
static uint32_t now;static bool silent,reject;static unsigned opens,closes,lists,logins,queries,leaves;
static void (*callback)(void);
static uint8_t queued[8][TA_PUB_BYTES];static unsigned qsize[8],qread,qwrite;
static uint32_t GetTickCount(void){return now;}
static void accountProfiles(bool write){(void)write;}
static void TA_Background_Set(void(*tick)(void)){callback=tick;}
static int XNetRandom(void*p,unsigned n){memset(p,17,n);return 0;}
static DWORD XNetGetTitleXnAddr(XNADDR*a){a->ina.s_addr=ta_vs_public_ipv4("10.0.0.2");return XNET_GET_XNADDR_DHCP;}
static void ta_rom_sha256(const void*p,unsigned n,uint8_t*out){(void)p;(void)n;memset(out,7,32);}
static uint32_t taSocialU32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void w32(uint8_t*p,uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static void enqueue(const uint8_t*p,unsigned n){assert(qwrite-qread<8);memcpy(queued[qwrite%8],p,n);qsize[qwrite++%8]=n;}
int ta_vs_udp_open(TaVsUdp*u,uint16_t port){memset(u,0,sizeof *u);u->opened=1;u->localPort=(uint16_t)(41000+opens++);(void)port;return 0;}
void ta_vs_udp_close(TaVsUdp*u){if(u->opened)closes++;memset(u,0,sizeof *u);qread=qwrite;}
int ta_vs_udp_receive_any(TaVsUdp*u,void*b,size_t cap,uint32_t*a,uint16_t*p){(void)u;if(qread==qwrite)return 0;unsigned k=qread++%8;assert(qsize[k]<=cap);memcpy(b,queued[k],qsize[k]);*a=ta_vs_public_ipv4(TA_N17_SERVER);*p=TA_N17_PORT;return (int)qsize[k];}
int ta_vs_udp_send_to(TaVsUdp*u,uint32_t address,uint16_t port,const void*data,size_t n){
 (void)u;assert(address==ta_vs_public_ipv4(TA_N17_SERVER)&&port==TA_N17_PORT);const uint8_t*b=data;unsigned op=b[5];
 assert(op==TA_PUB_HELLO||op==TA_PUB_LIST||op==TA_PUB_LEAVE||op==16);
 if(silent)return (int)n;
 uint8_t reply[TA_PUB_BYTES];memcpy(reply,b,n);
 if(op==TA_PUB_HELLO){reply[5]=TA_PUB_COOKIE;reply[6]=reply[7]=0;memset(reply+16,91,16);enqueue(reply,32);}
 else if(op==TA_PUB_LIST){lists++;reply[5]=TA_PUB_ROOMS;reply[6]=0;reply[7]=1;reply[32]=0;enqueue(reply,33);}
 else if(op==TA_PUB_LEAVE)leaves++;
 else{
  uint8_t key[32],plain[TA_ACCOUNT_PACKET],body[78]={0};uint64_t seq=0;for(unsigned i=0;i<8;i++)seq=(seq<<8)|b[64+i];
  assert(!ta_account_key(key,serverSecret,b+32,b+32,ta_n17_account_key));
  assert(!ta_account_open(plain,b+72,key,seq,0,b,72,b+88,(unsigned)n-88));
  unsigned count;body[0]=plain[0];
  if(plain[0]==TA_ACCOUNT_RESUME){logins++;assert(n==137);assert(!memcmp(plain+1,savedProfiles[profileActive].name,16));assert(!memcmp(plain+17,savedProfiles[profileActive].token,32));count=78;body[1]=reject?3:0;memcpy(body+2,plain+17,32);memcpy(body+34,plain+1,16);w32(body+50,1000);}
  else{assert(plain[0]==TA_ACCOUNT_COUNTS);queries++;count=14;w32(body+2,12);w32(body+6,2);w32(body+10,3);}
  unsigned bytes=56+count;reply[6]=bytes>>8;reply[7]=bytes;
  ta_account_seal(reply+88,reply+72,key,seq,1,reply,72,body,count);enqueue(reply,88+count);
 }
 return (int)n;
}
static void TA_Video_SetPopulation(unsigned a,unsigned g,unsigned o,unsigned k){assert(a==taSocialAccounts&&g==taSocialInGame&&o==taSocialOnline&&k);}
#include "../../src/xbox/ta_n22_presence.h"
static void advance(unsigned ms){for(unsigned end=now+ms;now<end;){now+=20;if(callback)callback();}}
int main(void){
 crypto_x25519_public_key(ta_n17_account_key,serverSecret);taPresenceStart((const uint8_t*)"rom",3);
 advance(1000);assert(!opens); /* No anonymous or manufactured accounts. */
 strcpy(savedProfiles[0].name,"PLAYERONE");memset(savedProfiles[0].token,23,32);profileCount=1;
 advance(1000);assert(taPresenceAuthenticated&&opens==1&&logins==1&&queries==1);assert(taSocialOnline==3&&taSocialInGame==2&&taSocialAccounts==12);
 /* The same frame hook remains active in menus, LAN, party and offline play.
    Switching screens never reauthenticates or recreates the presence socket. */
 unsigned beforeLists=lists,beforeQueries=queries;
 for(unsigned screen=0;screen<4;screen++)advance(20000);
 assert(opens==1&&logins==1);assert(lists-beforeLists>=15&&lists-beforeLists<=17);assert(queries-beforeQueries>=7&&queries-beforeQueries<=9);assert(!taPresenceRoom.haveRoom&&!taPresenceRoom.havePeer);
 silent=true;advance(22000);assert(!taPresenceAuthenticated&&closes>=1);silent=false;advance(7000);assert(taPresenceAuthenticated&&opens==2&&logins==2);
 strcpy(savedProfiles[1].name,"PLAYERTWO");memset(savedProfiles[1].token,29,32);profileCount=2;profileActive=1;advance(1000);assert(taPresenceAuthenticated&&opens==3&&logins==3&&leaves>=1);
 profileCount=0;advance(1000);assert(!taPresenceUdp.opened&&!taPresenceAuthenticated);assert(!taPresenceToken[0]);
 profileCount=1;profileActive=0;reject=true;advance(1000);unsigned failed=logins;assert(!taPresenceAuthenticated);advance(30000);assert(logins==failed);reject=false;advance(33000);assert(taPresenceAuthenticated);
 /* Reopening a ROM/session replaces its hook/socket cleanly. */
 taPresenceStart((const uint8_t*)"next",4);advance(1000);assert(taPresenceAuthenticated);assert(opens==closes+1);
 taPresenceEnabled=false;taPresenceDrop(now,0);assert(opens==closes);
 puts("Presence tests passed: saved-profile auto-resume, 80s outside ONLINE, bounded cadence, reconnect, profile switch/removal, invalid-token backoff, session restart.");return 0;
}
