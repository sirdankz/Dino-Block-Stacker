#include "ta_platform.h"
#include <winsockx.h>
#if defined(TA_DINO_BRAND) && TA_DINO_BRAND
#define TA_MENU_TITLE "DINO BLOCK STACKER"
#else
#define TA_MENU_TITLE "TETRIS ATTACK"
#endif
extern "C" {
#include "ta_vs_lan.h"
#include "ta_vs_udp.h"
#include "ta_vs_lan_screen.h"
#include "ta_vs_local_game.h"
#include "ta_vs_picture.h"
#include "ta_vs_public.h"
#include "ta_vs_party.h"
#if defined(TA_VS_ACCOUNTS) && TA_VS_ACCOUNTS
#include "ta_vs_account.h"
#endif
#include "ta_vs_match.h"
#include "ta_vs_round.h"
#include "ta_vs_preview.h"
#include "ta_vs_start.h"
#include "ta_rom_sha256.h"
#include "ta_native.h"
}
#if defined(TA_VS_NETWORK_READY) && TA_VS_NETWORK_READY
#include "../../out/N17Assets/ta_n17_assets.h"
#include "ta_vs_connect_clock.h"
#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK
#include "ta_vs_network_recovery.h"
#if defined(TA_VS_PERSIST_PROFILES) && TA_VS_PERSIST_PROFILES
#if defined(TA_VS_TITLE_SAVES) && TA_VS_TITLE_SAVES
#if defined(TA_DINO_BRAND) && TA_DINO_BRAND
#define TA_LAN_BUILD "N22"
#else
#define TA_LAN_BUILD "N21"
#endif
#else
#define TA_LAN_BUILD "N20"
#endif
#else
#define TA_LAN_BUILD "N19"
#endif
#else
#define TA_LAN_BUILD "N18"
#endif
#elif defined(TA_VS_COMPACT_UI) && TA_VS_COMPACT_UI
#include "../../out/N17Assets/ta_n17_assets.h"
#define TA_LAN_BUILD "N17"
#elif defined(TA_VS_MENU_NAV) && TA_VS_MENU_NAV
#define TA_LAN_BUILD "N16"
#elif defined(TA_VS_ACCOUNT_UX) && TA_VS_ACCOUNT_UX
#define TA_LAN_BUILD "N15"
#elif defined(TA_VS_ACCOUNTS) && TA_VS_ACCOUNTS
#define TA_LAN_BUILD "N14"
#elif defined(TA_VS_NATIVE_VIEW) && TA_VS_NATIVE_VIEW
#define TA_LAN_BUILD "N13"
#elif defined(TA_VS_FIRST_LAUNCH) && TA_VS_FIRST_LAUNCH
#define TA_LAN_BUILD "N12"
#elif defined(TA_VS_FAST_START) && TA_VS_FAST_START
#define TA_LAN_BUILD "N11"
#elif defined(TA_VS_REMATCH) && TA_VS_REMATCH
#define TA_LAN_BUILD "N10"
#elif defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
#define TA_LAN_BUILD "N9"
#elif defined(TA_VS_ACTIVE_CAPTURE) && TA_VS_ACTIVE_CAPTURE
#define TA_LAN_BUILD "N8"
#elif defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
#define TA_LAN_BUILD "N7"
#else
#define TA_LAN_BUILD "N6"
#endif
static TaVsUdp transport;
static TaVsLan lobby;
static TaVsLocalGame game;
static TaVsPictureTx pictureTx;
static TaVsPictureRx pictureRx;
static uint32_t screen[512*480],capture[512*224],overlay[512*224];
static uint8_t legacy[512*480*4];
static int16_t audio[1600];
static unsigned peerReady,peerLoaded,peerLost,peerQuit,remoteRevision,streamTick,overlayRevision;
static bool sourceReady;
static uint32_t captureRevision;
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
static TaVsPublic publicRoom;
/* Both room directories share one socket; opening a second XNet stack while
   the original is active can reset Xbox network configuration. */
static TaVsParty publicPartyDirectory;
static uint32_t publicPartyAddress;
static uint16_t publicPartyPort;
static bool publicPartyBrowsing;
static uint32_t publicPartyJoinRoom;
static int publicPartyHostMode=-1;
#if defined(TA_VS_NETWORK_READY) && TA_VS_NETWORK_READY
static TaVsConnectClock publicClock;
static uint32_t publicNetStatus,publicWaitLogAt;
static unsigned publicRxLogs,publicTxLogs;
#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK
static unsigned publicAutoAttempted;
#endif
#endif

static bool publicMode,publicRoomClosed;
static bool publicRehostRequested;
#if defined(TA_VS_ACCOUNTS) && TA_VS_ACCOUNTS
static TaAccountLink accountLink;
static bool accountAuthenticated;
#endif
static unsigned ownCharacter=0,peerCharacter=0,sharedSpeed=5,peerSettings=0,lastRoute=~0u;
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
static TaVsRound roundState;
static TaVsPreview previewAtlas;
static uint8_t*menuCheckpoint;static int menuCheckpointSize;
#if defined(XBOX_PORT)
static uint32_t menuApuMasterPending;static int64_t menuApuCycleDebtNumerator;
#endif
static void leavePublic(void);
#else
static void leavePublic(void){if(publicMode&&transport.opened){uint8_t packet[32];size_t n=ta_vs_public_leave(&publicRoom,packet,sizeof packet);if(n)for(unsigned k=0;k<3;k++)ta_vs_udp_send_to(&transport,publicRoom.serverAddress,publicRoom.serverPort,packet,n);}publicMode=false;}
#endif
#endif
static unsigned word(const uint8_t*w,unsigned a){return w[a]|((unsigned)w[a+1]<<8);}
static int sendControl(unsigned kind){
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
 uint32_t epoch=roundState.epoch+((kind==6||kind==9)?1u:0u);
 uint8_t data[8]={10,(uint8_t)kind,(uint8_t)sharedSpeed,(uint8_t)ownCharacter,(uint8_t)(epoch>>24),(uint8_t)(epoch>>16),(uint8_t)(epoch>>8),(uint8_t)epoch};
#elif defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
 uint8_t data[5]={9,(uint8_t)kind,(uint8_t)sharedSpeed,(uint8_t)ownCharacter,0};
#else
 uint8_t data[2]={6,(uint8_t)kind};
#endif
 return ta_vs_enqueue(&lobby.session,TA_VS_RESULT,data,sizeof data,NULL);
}
static int consumeEvents(void){
 const TaVsEvent*e;
 while((e=ta_vs_peek(&lobby.session))!=NULL){
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
  if(roundState.next)return 0;
  if(e->tag==TA_VS_ATTACK){
   TaVsAttack a;int r=ta_vs_attack_decode(e,&a);if(r||!a.round)return TA_VS_BAD_PACKET;
   if(a.round>roundState.epoch)return 0;
   if(a.round<roundState.epoch||roundState.phase==TA_ROUND_RESULT){r=ta_vs_commit(&lobby.session,e->sequence);if(r)return r;continue;}
   if(!game.s)return TA_VS_CONFLICT;r=ta_vs_attack_stage(&lobby.session,&game.inbox);if(r==TA_VS_QUEUE_FULL)return 0;if(r)return r;continue;
  }
#else
  if(e->tag==TA_VS_ATTACK){if(!game.s)return TA_VS_CONFLICT;int r=ta_vs_attack_stage(&lobby.session,&game.inbox);if(r==TA_VS_QUEUE_FULL)return 0;if(r)return r;continue;}
#endif
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
  if(e->tag!=TA_VS_RESULT||e->size!=8||e->data[0]!=10||e->data[1]>9||e->data[2]<1||e->data[2]>10||e->data[3]>=TA_VS_CHARACTERS)return TA_VS_BAD_PACKET;
  unsigned kind=e->data[1];uint32_t epoch=((uint32_t)e->data[4]<<24)|((uint32_t)e->data[5]<<16)|((uint32_t)e->data[6]<<8)|e->data[7];
  int transition=ta_vs_round_control(&roundState,lobby.role,kind,epoch);if(transition<0)return TA_VS_CONFLICT;if(transition==2)return 0;
  if(transition==1){int r=ta_vs_commit(&lobby.session,e->sequence);if(r)return r;continue;}
  if(kind==0||kind==4){peerCharacter=e->data[3];peerSettings=1;if(lobby.role)sharedSpeed=e->data[2];}
#elif defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
  if(e->tag!=TA_VS_RESULT||e->size!=5||e->data[0]!=9||e->data[1]>4||e->data[2]<1||e->data[2]>10||e->data[3]>=TA_VS_CHARACTERS||e->data[4])return TA_VS_BAD_PACKET;
  unsigned kind=e->data[1];if(kind==0||kind==4){peerCharacter=e->data[3];peerSettings=1;if(lobby.role)sharedSpeed=e->data[2];}
#else
  if(e->tag!=TA_VS_RESULT||e->size!=2||e->data[0]!=6||e->data[1]>3)return TA_VS_BAD_PACKET;
  unsigned kind=e->data[1];
#endif
  if(kind==0)peerReady=1;else if(kind==1)peerLoaded=1;else if(kind==2)peerLost=1;else if(kind==3)peerQuit=1;
  int r=ta_vs_commit(&lobby.session,e->sequence);if(r)return r;
 }
 return 0;
}
/* All networking is nonblocking and bounded. It never requests a remote
   controller state, runs a remote input, or gates a live local game frame. */
#if (!defined(TA_NO_LOG) || !TA_NO_LOG) && defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
static unsigned long coarse[5];
#define N7_TICK(name) unsigned long name=GetTickCount()
#define N7_ADD(name,index) (coarse[index]+=(unsigned long)(GetTickCount()-name))
#else
#define N7_TICK(name) ((void)0)
#define N7_ADD(name,index) ((void)0)
#endif
static int pumpWork(bool display){
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
 if(publicMode&&publicRoomClosed){
  /* The game route is closed, but the encrypted result channel still drains. */
  uint32_t now=GetTickCount();for(unsigned i=0;i<8;i++){uint8_t b[TA_PUB_BYTES];uint32_t from;uint16_t port;int n=ta_vs_udp_receive_any(&transport,b,sizeof b,&from,&port);if(n<=0)break;if(from==publicRoom.serverAddress&&port==publicRoom.serverPort)ta_account_receive(&accountLink,&publicRoom,b,(size_t)n);}
  const uint8_t*packet;size_t bytes=ta_account_packet(&accountLink,now,&packet);if(bytes)ta_vs_udp_send_to(&transport,publicRoom.serverAddress,publicRoom.serverPort,packet,bytes);return 0;
 }
#endif
 uint32_t now=GetTickCount();
#if defined(TA_VS_NETWORK_READY) && TA_VS_NETWORK_READY
 if(publicMode&&!publicClock.armed){
  XNADDR address;memset(&address,0,sizeof address);DWORD status=XNetGetTitleXnAddr(&address);publicNetStatus=(uint32_t)status;
  if(!(status&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_AUTO|XNET_GET_XNADDR_PPPOE))||!address.ina.s_addr){
#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK
   if(publicAutoAttempted&&(uint32_t)(now-publicClock.opened)>30000u){TA_Log("[TA " TA_LAN_BUILD " NET] stage=dhcp_timeout saved_settings_unchanged=1\n");return -10060;}
#endif
   if(!publicWaitLogAt||(uint32_t)(now-publicWaitLogAt)>=2000){publicWaitLogAt=now;TA_Log("[TA " TA_LAN_BUILD " NET] stage=waiting_address status=%08lX elapsed_ms=%lu server_timer=not_started\n",(unsigned long)status,(unsigned long)(now-publicClock.opened));}return 0;
  }
  publicRoom.localAddress=address.ina.s_addr;ta_vs_connect_ready(&publicClock,now,&publicRoom.lastServer);
  const uint8_t*ip=(const uint8_t*)&publicRoom.localAddress;
  TA_Log("[TA " TA_LAN_BUILD " NET] stage=address_ready ip=%u.%u.%u.%u status=%08lX gateway_flag=%u wait_ms=%lu server_budget_ms=10000\n",ip[0],ip[1],ip[2],ip[3],(unsigned long)status,(unsigned)((status&0x20u)!=0),(unsigned long)(now-publicClock.opened));
 }
#endif
 for(unsigned k=0;k<32;k++){
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
  uint8_t packet[TA_PUB_BYTES];
#else
  uint8_t packet[TA_VS_PACKET_BYTES];
#endif
  uint32_t from;uint16_t port;
  int n=ta_vs_udp_receive_any(&transport,packet,sizeof packet,&from,&port);if(!n)break;
  if(n<0){if(n==-WSAEMSGSIZE){lobby.rejected++;continue;}return n;}
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
  if(publicMode&&publicPartyBrowsing&&from==publicPartyAddress&&port==publicPartyPort&&n>=4&&!memcmp(packet,"TAP4",4)){
   ta_vs_party_receive(&publicPartyDirectory,now,packet,(size_t)n);continue;
  }
  if(publicMode){
#if defined(TA_VS_NETWORK_READY) && TA_VS_NETWORK_READY
   if(!publicRoom.havePeer&&publicRxLogs<4){publicRxLogs++;TA_Log("[TA " TA_LAN_BUILD " NET] stage=datagram_received bytes=%d op=%u from_server=%u rx=%lu\n",n,n>=6?(unsigned)packet[5]:0u,(unsigned)(from==publicRoom.serverAddress&&port==publicRoom.serverPort),(unsigned long)transport.received);}
#endif
#if defined(TA_VS_ACCOUNTS) && TA_VS_ACCOUNTS
   if(n>=6&&packet[5]==16){if(from==publicRoom.serverAddress&&port==publicRoom.serverPort&&!ta_account_receive(&accountLink,&publicRoom,packet,(size_t)n))publicRoom.lastServer=now;continue;}
#endif
   n=ta_vs_public_receive(&publicRoom,now,from,port,packet,(size_t)n);if(n<0){lobby.rejected++;continue;}if(!n)continue;from=publicRoom.serverAddress;port=publicRoom.serverPort;}
#endif
  int r=ta_vs_lan_receive(&lobby,now,from,port,packet,(size_t)n);
  if(r){lobby.rejected++;if(r==TA_VS_IDENTITY_MISMATCH)return r;continue;}
  if(ta_vs_lan_ready(&lobby)){
   r=consumeEvents();if(r)return r;
   size_t bytes;uint32_t rev;const uint8_t*p=ta_vs_remote_snapshot(&lobby.session,&bytes,&rev);
   if(p&&rev!=remoteRevision){remoteRevision=rev;
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
    if(bytes<20||memcmp(p,"RV10",4))return TA_VS_BAD_PACKET;
    uint32_t epoch=((uint32_t)p[4]<<24)|((uint32_t)p[5]<<16)|((uint32_t)p[6]<<8)|p[7];if(epoch!=roundState.epoch)continue;p+=8;bytes-=8;
#endif
    if(bytes>=12&&!memcmp(p,"PVS6",4)){if(ta_vs_picture_receive(&pictureRx,p,bytes))return TA_VS_BAD_PACKET;}
   }
  }
 }
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
 if(publicMode){
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
  if(publicRoom.phase!=roundState.phase||publicRoom.roundEpoch!=roundState.epoch)publicRoom.kept=0;
  publicRoom.phase=roundState.phase;publicRoom.roundEpoch=roundState.epoch;
#endif
  if(publicRoom.error)return publicRoom.error;
  if(publicPartyBrowsing&&!publicRoom.haveRoom&&!publicPartyDirectory.error&&
     (!publicPartyDirectory.requestSent||(uint32_t)(now-publicPartyDirectory.lastSend)>=(publicPartyDirectory.haveCookie?1000u:250u))){
   uint8_t packet[TA_PARTY_BYTES];size_t n=ta_vs_party_packet(&publicPartyDirectory,now,packet,sizeof packet);
   if(n)ta_vs_udp_send_to(&transport,publicPartyAddress,publicPartyPort,packet,n);
  }
#if defined(TA_VS_ACCOUNTS) && TA_VS_ACCOUNTS
  const uint8_t*accountPacket;size_t accountBytes=ta_account_packet(&accountLink,now,&accountPacket);
  if(accountBytes){int sent=ta_vs_udp_send_to(&transport,publicRoom.serverAddress,publicRoom.serverPort,accountPacket,accountBytes);if(sent<0)return sent;}
#endif
  for(unsigned k=0;k<4;k++){uint8_t packet[TA_PUB_BYTES];uint32_t to;uint16_t port;size_t n=ta_vs_public_packet(&publicRoom,now,packet,sizeof packet,&to,&port);if(!n)break;int sent=ta_vs_udp_send_to(&transport,to,port,packet,n);
#if defined(TA_VS_NETWORK_READY) && TA_VS_NETWORK_READY
   if(!publicRoom.havePeer&&publicTxLogs<4){publicTxLogs++;TA_Log("[TA " TA_LAN_BUILD " NET] stage=datagram_send op=%u bytes=%u result=%d tx=%lu cookie=%u elapsed_ms=%lu\n",(unsigned)packet[5],(unsigned)n,sent,(unsigned long)transport.sent,(unsigned)publicRoom.haveCookie,(unsigned long)(now-publicClock.ready));}
#endif
   if(sent<0){if(packet[5]==TA_PUB_PUNCH){if(publicRoom.direct&&to==publicRoom.directAddress&&port==publicRoom.directPort)publicRoom.direct=publicRoom.seen=publicRoom.peerSeen=publicRoom.seenLan=publicRoom.peerSeenLan=0;continue;}return sent;}}
  if(publicRoom.havePeer&&lastRoute!=(unsigned)publicRoom.direct){lastRoute=publicRoom.direct;TA_Log("[TA " TA_LAN_BUILD " ROUTE] room=%lu route=%s local_controls=immediate\n",(unsigned long)publicRoom.room,publicRoom.direct?"DIRECT":"RELAY");}
#if defined(TA_VS_NETWORK_READY) && TA_VS_NETWORK_READY
  if(!publicRoom.havePeer&&ta_vs_connect_expired(&publicClock,now,publicRoom.lastServer)){
#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK
   if(ta_vs_network_recover(publicMode,publicRoom.haveCookie,publicRoom.havePeer,publicAutoAttempted,publicNetStatus&XNET_GET_XNADDR_STATIC)){
    publicAutoAttempted=1;ta_vs_udp_close(&transport);int opened=ta_vs_udp_open_dhcp(&transport,0);
    TA_Log("[TA " TA_LAN_BUILD " NET] stage=automatic_retry result=%d shared_stack=1 saved_settings_unchanged=1\n",opened);if(opened)return opened;
    publicRoom.localAddress=0;publicRoom.localPort=transport.localPort;publicRoom.requested=0;publicRoom.lastRequest=0;publicRoom.started=publicRoom.lastServer=now;
    ta_vs_connect_reset(&publicClock,now);publicNetStatus=publicWaitLogAt=publicRxLogs=publicTxLogs=0;return 0;
   }
#endif
   TA_Log("[TA " TA_LAN_BUILD " NET] stage=server_timeout cookie=%u room=%lu tx=%lu rx=%lu rejected=%u last_reply_ms=%lu address_status=%08lX gateway_flag=%u\n",(unsigned)publicRoom.haveCookie,(unsigned long)publicRoom.room,(unsigned long)transport.sent,(unsigned long)transport.received,lobby.rejected,(unsigned long)(now-publicRoom.lastServer),(unsigned long)publicNetStatus,(unsigned)((publicNetStatus&0x20u)!=0));return -10060;
  }
#else
  if(!publicRoom.havePeer&&(uint32_t)(now-publicRoom.lastServer)>10000)return -10060;
#endif
 }
#endif
 if(ta_vs_lan_timed_out(&lobby,now)&&(!publicMode||(uint32_t)(now-publicRoom.lastServer)>5000u))return -10060;
 for(unsigned k=0;k<4;k++){
  if(display&&sourceReady&&ta_vs_lan_ready(&lobby)&&lobby.session.snapshotRevision==lobby.session.snapshotSent){
   uint8_t payload[TA_VS_SNAPSHOT_BYTES];size_t n;unsigned offset=0;
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
   offset=8;memcpy(payload,"RV10",4);uint32_t epoch=roundState.epoch;payload[4]=(uint8_t)(epoch>>24);payload[5]=(uint8_t)(epoch>>16);payload[6]=(uint8_t)(epoch>>8);payload[7]=(uint8_t)epoch;
#endif
#if defined(TA_VS_PREPARED_PICTURE) && TA_VS_PREPARED_PICTURE
   n=ta_vs_picture_next(&pictureTx,streamTick,payload+offset,sizeof payload-offset);
#else
   n=ta_vs_picture_packet(&pictureTx,capture,512,streamTick,payload+offset,sizeof payload-offset);
#endif
   if(n){int r=ta_vs_set_snapshot(&lobby.session,payload,n+offset);if(r)return r;}
  }
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
  if(publicMode&&!publicRoom.havePeer)break;
#endif
  uint8_t packet[TA_VS_PACKET_BYTES];uint32_t to;uint16_t port;
  size_t n=ta_vs_lan_packet(&lobby,now,packet,sizeof packet,&to,&port);if(!n)break;
  int sent;
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
  if(publicMode){uint8_t wrapped[TA_PUB_BYTES];size_t bytes=ta_vs_public_game(&publicRoom,now,packet,n,wrapped,sizeof wrapped,&to,&port);if(!bytes)break;sent=ta_vs_udp_send_to(&transport,to,port,wrapped,bytes);
   if(sent<0&&wrapped[5]==TA_PUB_DIRECT){publicRoom.direct=publicRoom.seen=publicRoom.peerSeen=publicRoom.seenLan=publicRoom.peerSeenLan=0;bytes=ta_vs_public_game(&publicRoom,now,packet,n,wrapped,sizeof wrapped,&to,&port);if(bytes)sent=ta_vs_udp_send_to(&transport,to,port,wrapped,bytes);}
  }
  else
#endif
  sent=ta_vs_udp_send_to(&transport,to,port,packet,n);if(sent<0)return sent;if(!sent)break;
 }
 return 0;
}
static int pump(bool display){N7_TICK(netStart);int r=pumpWork(display);
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
 /* A peer may leave its room after the reliable result was acknowledged.
    Preserve the completed result screen; do not label this a live disconnect. */
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
 if(publicMode&&r==-207){roundState.peerLeft=1;publicRoomClosed=true;publicRoom.error=0;r=0;}
#else
 if(publicMode&&r==-207&&game.s&&(peerLost||peerQuit||word(game.s->ram,0x2a0)==5)){publicRoomClosed=true;publicRoom.error=0;r=0;}
#endif
#endif
 N7_ADD(netStart,0);return r;}
#if defined(TA_VS_COMPACT_UI) && TA_VS_COMPACT_UI
#include "ta_n17_theme.h"
#endif
static void caption(const char*title,const char*detail,bool preMatch=false){
#if defined(TA_VS_COMPACT_UI) && TA_VS_COMPACT_UI
 ta_n17_backdrop();ta_vs_lan_text(screen,42,51,TA_MENU_TITLE,0x007dd9e8,3);ta_vs_lan_text(screen,42,126,title,0x00f2f4f8,4);ta_vs_lan_text(screen,42,208,detail,0x00c1c9d5,2);ta_n17_panel(30,395,452,47);ta_vs_lan_text(screen,44,412,preMatch?"B CANCEL":"A CONTINUE   B BACK",0x0096e3b2,2);TA_Video_Submit((const uint8_t*)screen);return;
#endif
 for(unsigned i=0;i<512*480;i++)screen[i]=0x000b1524;
 ta_vs_lan_text(screen,28,48,"TETRIS ATTACK VERSUS " TA_LAN_BUILD,0x006fc9ff,4);
 ta_vs_lan_text(screen,28,148,title,0x00ffffff,3);
 ta_vs_lan_text(screen,28,208,detail,0x009fb9ce,2);
#if defined(TA_VS_MENU_NAV) && TA_VS_MENU_NAV
 if(!preMatch)for(unsigned y=396;y<439;y++)for(unsigned x=26;x<486;x++)screen[y*512+x]=0x00f1cb72;
 ta_vs_lan_text(screen,38,414,preMatch?"B CANCEL":"A CONTINUE   B BACK",preMatch?0x00f1f3f8:0x00101928,2);
#else
 ta_vs_lan_text(screen,28,414,preMatch?"BACK AND START TO CANCEL":"B RETURN TO LAN",0x009fb9ce,2);
#endif
 TA_Video_Submit((const uint8_t*)screen);
}
#if defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
static bool updateOpponent(void){return TA_Video_SetLanOverlay(pictureRx.pixels,pictureRx.updates);}
#else
static bool updateOverlay(const char*state,unsigned seconds){
 memset(overlay,0,sizeof overlay);
 for(unsigned y=0;y<224;y++){
  for(unsigned x=224;x<288;x++)overlay[y*512+x]=0x000b1524;
  memcpy(overlay+y*512+288,pictureRx.pixels+y*TA_VS_PIC_W,TA_VS_PIC_W*4);
 }
 /* Font uses a512-wide canvas; all positions stay within the224 active rows. */
 ta_vs_lan_text(overlay,232,12,"VS",0x006fc9ff,3);
 ta_vs_lan_text(overlay,232,46,"LV 5",0x00ffffff,2);
 char time[20];snprintf(time,sizeof time,"%02u:%02u",seconds/60,seconds%60);
 ta_vs_lan_text(overlay,228,88,time,0x00ffffff,2);
 ta_vs_lan_text(overlay,230,128,state,0x006affa8,2);
 ta_vs_lan_text(overlay,230,180,"LAN",0x009fb9ce,2);
 return TA_Video_SetLanOverlay(overlay,++overlayRevision);
}
#endif
/* Preserve the established480-line/swizzled upload fallback if the small
   active texture could not be allocated. */
static void presentCapture(void){
 if(TA_Video_SubmitActive224((const uint8_t*)capture))return;
 memset(legacy,0,sizeof legacy);
 for(unsigned y=0;y<224;y++){memcpy(legacy+(16+y*2)*2048,capture+y*512,2048);memcpy(legacy+(17+y*2)*2048,capture+y*512,2048);}
 TA_Video_Submit(legacy);
}
static void releaseGame(void){ta_vs_local_bind(NULL);TA_Video_SetLanOverlay(NULL,0);if(game.s){TA_Video_ReleaseLanPpu(game.s->ppu);snes_free(game.s);}memset(&game,0,sizeof game);sourceReady=false;}

#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
#if defined(TA_VS_ACCOUNTS) && TA_VS_ACCOUNTS
#if defined(TA_VS_NETWORK_READY) && TA_VS_NETWORK_READY
#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK
#if defined(TA_VS_PERSIST_PROFILES) && TA_VS_PERSIST_PROFILES
#if defined(TA_VS_TITLE_SAVES) && TA_VS_TITLE_SAVES
#if defined(TA_DINO_BRAND) && TA_DINO_BRAND
static int beginLobby(const uint8_t*rom,unsigned bytes,unsigned role,bool online);
#include "ta_n22_accounts.h"
#else
#include "ta_n21_accounts.h"
#endif
#else
#include "ta_n20_accounts.h"
#endif
#else
#include "ta_n19_accounts.h"
#endif
#else
#include "ta_n18_accounts.h"
#endif
#elif defined(TA_VS_COMPACT_UI) && TA_VS_COMPACT_UI
#include "ta_n17_accounts.h"
#elif defined(TA_VS_MENU_NAV) && TA_VS_MENU_NAV
#include "ta_n16_accounts.h"
#elif defined(TA_VS_ACCOUNT_UX) && TA_VS_ACCOUNT_UX
#include "ta_n15_accounts.h"
#else
#include "ta_n14_accounts.h"
#endif
#endif
static int beginLobby(const uint8_t*rom,unsigned bytes,unsigned role,bool online){
 int error=ta_vs_udp_open(&transport,online||role?0:TA_VS_LAN_PORT);if(!error&&!online)error=ta_vs_udp_enable_broadcast(&transport);if(error)return error;
 TaVsIdentity id;memset(&id,0,sizeof id);ta_rom_sha256(rom,bytes,id.rom);
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
 #if defined(TA_VS_FAST_START) && TA_VS_FAST_START
 static const uint8_t rules[]="TA_NATIVE_VS_N11_PREBOARD_SEED_FAST_START_V1";
#else
 static const uint8_t rules[]="TA_NATIVE_VS_N10_ROUND_TAGS_REMATCH_PREVIEWS_V1";
#endif
#else
 static const uint8_t rules[]="TA_NATIVE_VS_N9_P0_AUTHORITY_ACTIVE_CAPTURE_OPTIONS_PUBLIC_V1";
#endif
 ta_rom_sha256(rules,sizeof rules-1,id.rules);
 uint32_t now=GetTickCount();uint64_t nonce=__builtin_readcyclecounter()^((uint64_t)now<<32);if(!nonce)nonce=1;id.seed=(uint32_t)nonce;
#if defined(TA_VS_FAST_START) && TA_VS_FAST_START
 FILETIME utc;GetSystemTimeAsFileTime(&utc);static uint32_t sessions;
 id.seed^=(uint32_t)(nonce>>32)^utc.dwLowDateTime^utc.dwHighDateTime^(++sessions*0x9e3779b9u)^now;
#endif
 error=ta_vs_lan_init(&lobby,&id,role,nonce,now);if(error)return error;
 publicMode=online;publicRoomClosed=false;
 if(online){
#if defined(TA_VS_COMPACT_UI) && TA_VS_COMPACT_UI
 char ip[64]=TA_N17_SERVER;unsigned port=TA_N17_PORT;
#else
 char ip[64]="172.233.145.244";unsigned port=TA_PUB_PORT;
#endif
FILE*f=fopen("D:\\ta-server.txt","rb");if(f){char line[96];if(fgets(line,sizeof line,f)){char value[64];unsigned p=TA_PUB_PORT;if(sscanf(line,"%63s %u",value,&p)>=1&&ta_vs_public_ipv4(value)&&p&&p<65536){strcpy(ip,value);port=p;}}fclose(f);}
  ta_vs_public_init(&publicRoom,&id,nonce,ta_vs_public_ipv4(ip),(uint16_t)port,transport.localPort,now);publicRoom.speed=(uint8_t)sharedSpeed;lastRoute=~0u;
#if defined(TA_VS_NETWORK_READY) && TA_VS_NETWORK_READY
  
#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK
  publicAutoAttempted=0;
#endif
  ta_vs_connect_reset(&publicClock,now);publicNetStatus=publicWaitLogAt=publicRxLogs=publicTxLogs=0;
  TA_Log("[TA " TA_LAN_BUILD " NET] stage=open server=%s port=%u local_port=%u response_timeout_starts_after_address=1\n",ip,port,(unsigned)transport.localPort);
#endif
#if defined(TA_VS_ACCOUNTS) && TA_VS_ACCOUNTS
  accountAuthenticated=false;accountOpen();
#endif
 }
 return 0;
}
static int browsePublic(const uint8_t*rom,unsigned bytes,uint32_t*localIp){
 int error=beginLobby(rom,bytes,1,true);if(error)return error;uint16_t previous=8;
#if defined(TA_VS_FIRST_LAUNCH) && TA_VS_FIRST_LAUNCH
 previous=1u<<9;
#endif
#if defined(TA_VS_ACCOUNTS) && TA_VS_ACCOUNTS
 return accountBrowser(localIp,previous,rom,bytes);
#else
 unsigned selected=0;
 for(;;){uint16_t pads[2];TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];if(pressed&(1u<<8)){leavePublic();ta_vs_udp_close(&transport);return 1;}
  XNADDR address;DWORD status=XNetGetTitleXnAddr(&address);if(status&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_AUTO|XNET_GET_XNADDR_PPPOE)){*localIp=address.ina.s_addr;publicRoom.localAddress=*localIp;if((error=pump(false))!=0)return error;}
  if(publicRoom.haveRoom)return 0;
  if((pressed&0x10)&&selected)selected--;if((pressed&0x20)&&selected+1<publicRoom.roomCount)selected++;
  if(publicRoom.mode==TA_PUB_BROWSE&&(pressed&1)){lobby.role=0;ta_vs_public_host(&publicRoom);}
  if(publicRoom.mode==TA_PUB_BROWSE&&(pressed&2)&&selected<publicRoom.roomCount){ta_vs_public_join(&publicRoom,publicRoom.rooms[selected].id);}
  memset(screen,0,sizeof screen);ta_vs_lan_text(screen,26,28,"PUBLIC ROOMS " TA_LAN_BUILD,0x006fc9ff,4);
  ta_vs_lan_text(screen,26,70,publicRoom.haveCookie?"DIRECT NAT + RELAY FALLBACK":"CONNECTING TO ROOM SERVER",0x009fb9ce,2);
  if(!publicRoom.roomCount)ta_vs_lan_text(screen,26,150,"NO OPEN ROOMS - A TO HOST",0x00ffffff,2);
  for(unsigned k=0;k<publicRoom.roomCount;k++){char text[64];snprintf(text,sizeof text,"%s ROOM %06lu  SPEED %u",k==selected?">":" ",(unsigned long)publicRoom.rooms[k].id,publicRoom.rooms[k].speed);ta_vs_lan_text(screen,26,112+k*29,text,k==selected?0x006affa8:0x00ffffff,2);}
  ta_vs_lan_text(screen,26,370,publicRoom.mode==TA_PUB_HOST?"CREATING ROOM":publicRoom.mode==TA_PUB_JOINING?"JOINING ROOM":"A HOST   X JOIN SELECTED",0x006affa8,2);ta_vs_lan_text(screen,26,410,"UP DOWN SELECT   B BACK",0x009fb9ce,2);TA_Video_Submit((const uint8_t*)screen);
 }
#endif
}
static void renderOptions(bool ready){
#if defined(TA_VS_COMPACT_UI) && TA_VS_COMPACT_UI
 accountRenderOptions(ready);return;
#elif defined(TA_VS_MENU_NAV) && TA_VS_MENU_NAV
 accountFrame(publicMode?"ONLINE VERSUS":"LAN VERSUS","CHOOSE CHARACTER SPEED OR READY");char text[96];
 accountRect(36,145,440,99,optionChoice==0?uiGold:0x00213046);if(optionChoice==0)accountRect(36,145,4,99,uiText);uint32_t localColor=optionChoice==0?uiInk:uiText;
 ta_vs_lan_text(screen,58,156,publicMode&&accountAuthenticated?ownProfile.name:"YOUR CHARACTER",localColor,2);ta_vs_lan_text(screen,58,181,ta_vs_character_name(ownCharacter),localColor,2);ta_vs_lan_text(screen,58,216,"A CHOOSE CHARACTER",localColor,2);ta_vs_preview_draw(&previewAtlas,ownCharacter,screen,380,146);
 accountRect(36,249,440,43,optionChoice==1?uiGold:0x003c3423);uint32_t speedColor=optionChoice==1?uiInk:uiText;ta_vs_lan_text(screen,58,255,"MATCH SPEED",speedColor,2);snprintf(text,sizeof text,"%u",sharedSpeed);ta_vs_lan_text(screen,360,253,text,speedColor,5);ta_vs_lan_text(screen,58,278,lobby.role?"HOST CHOOSES MATCH SPEED":"A CHOOSE MATCH SPEED",speedColor,2);
 accountRect(36,298,440,97,0x00213046);ta_vs_lan_text(screen,46,307,publicMode&&otherProfile.name[0]?otherProfile.name:"OPPONENT",uiMuted,2);ta_vs_lan_text(screen,46,330,peerSettings?ta_vs_character_name(peerCharacter):"CHOOSING CHARACTER",uiText,2);ta_vs_lan_text(screen,46,367,peerReady?"READY":"CHOOSING",peerReady?0x005cdb8b:uiMuted,2);if(peerSettings)ta_vs_preview_draw(&previewAtlas,peerCharacter,screen,380,299);
 const char*items[]={ready?"READY - WAITING":"READY","BACK"};menuActions(items,2,optionChoice>=2?optionChoice-2:~0u,false);return;
#elif defined(TA_VS_ACCOUNT_UX) && TA_VS_ACCOUNT_UX

 accountFrame(publicMode?"ONLINE VERSUS":"LAN VERSUS","CHOOSE YOUR CHARACTER - SAME SPEED FOR BOTH");
 char line[96];accountRect(36,145,440,99,0x00213046);ta_vs_lan_text(screen,46,160,publicMode&&accountAuthenticated?ownProfile.name:"YOUR CHARACTER",uiGold,2);
 ta_vs_lan_text(screen,46,181,ta_vs_character_name(ownCharacter),uiText,2);ta_vs_lan_text(screen,46,214,"LEFT / RIGHT TO CHANGE",uiMuted,2);ta_vs_preview_draw(&previewAtlas,ownCharacter,screen,380,146);
 accountRect(36,249,440,43,0x003c3423);ta_vs_lan_text(screen,46,255,"MATCH SPEED",uiGold,2);snprintf(line,sizeof line,"%u",sharedSpeed);ta_vs_lan_text(screen,360,253,line,uiText,5);ta_vs_lan_text(screen,46,278,lobby.role?"HOST CHOOSES THE SPEED":"UP / DOWN - APPLIES TO BOTH",uiMuted,2);
 accountRect(36,298,440,97,0x00213046);ta_vs_lan_text(screen,46,307,publicMode&&otherProfile.name[0]?otherProfile.name:"OPPONENT",uiMuted,2);ta_vs_lan_text(screen,46,330,peerSettings?ta_vs_character_name(peerCharacter):"CHOOSING CHARACTER",uiText,2);ta_vs_lan_text(screen,46,367,peerReady?"READY":"CHOOSING",peerReady?0x005cdb8b:uiMuted,2);if(peerSettings)ta_vs_preview_draw(&previewAtlas,peerCharacter,screen,380,299);
 if(publicMode)snprintf(line,sizeof line,"ROOM %06lu   %s",(unsigned long)publicRoom.room,publicRoom.direct?"DIRECT":"RELAY");else strcpy(line,"LOCAL NETWORK VERSUS");
 accountButtonLine(30,416,ready?"READY - WAITING FOR OPPONENT":"A READY   B BACK");accountButtonLine(30,444,line);return;
#else

 memset(screen,0,sizeof screen);ta_vs_lan_text(screen,26,34,publicMode?"ONLINE VERSUS " TA_LAN_BUILD:"LAN VERSUS " TA_LAN_BUILD,0x006fc9ff,4);
#if defined(TA_VS_ACCOUNTS) && TA_VS_ACCOUNTS
 if(publicMode&&accountAuthenticated){ta_vs_lan_text(screen,26,86,ownProfile.name,0x006affa8,2);if(otherProfile.name[0])ta_vs_lan_text(screen,26,269,otherProfile.name,0x006affa8,2);}
#endif
 char text[96];snprintf(text,sizeof text,"YOU: %s",ta_vs_character_name(ownCharacter));ta_vs_lan_text(screen,26,110,text,0x00ffffff,2);
 ta_vs_lan_text(screen,26,144,"LEFT RIGHT CHOOSE CHARACTER",0x009fb9ce,2);
 snprintf(text,sizeof text,"BOTH PLAYERS SPEED: %u",sharedSpeed);ta_vs_lan_text(screen,26,205,text,0x006affa8,3);
 ta_vs_lan_text(screen,26,246,lobby.role?"HOST CHOOSES MATCH SPEED":"UP DOWN CHOOSE MATCH SPEED",0x009fb9ce,2);
 snprintf(text,sizeof text,"PEER: %s",peerSettings?ta_vs_character_name(peerCharacter):"CHOOSING");ta_vs_lan_text(screen,26,292,text,0x00ffffff,2);
 ta_vs_lan_text(screen,26,325,peerReady?"PEER READY":"WAITING FOR PEER READY",0x009fb9ce,2);
 ta_vs_lan_text(screen,26,365,ready?"READY - WAITING FOR PEER":"A READY   B BACK",0x006affa8,3);
 if(publicMode){snprintf(text,sizeof text,"ROOM %06lu   %s",(unsigned long)publicRoom.room,publicRoom.direct?"DIRECT":"RELAY");ta_vs_lan_text(screen,26,416,text,0x009fb9ce,2);}
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
 ta_vs_preview_draw(&previewAtlas,ownCharacter,screen,382,96);
 if(peerSettings)ta_vs_preview_draw(&previewAtlas,peerCharacter,screen,382,268);
#endif
#endif /* N15 options / earlier options */
}
#endif
#if defined(TA_VS_NETWORK_READY) && TA_VS_NETWORK_READY
#if defined(TA_VS_AUTO_NETWORK) && TA_VS_AUTO_NETWORK
#if defined(TA_VS_PERSIST_PROFILES) && TA_VS_PERSIST_PROFILES
#if defined(TA_VS_TITLE_SAVES) && TA_VS_TITLE_SAVES
#if defined(TA_DINO_BRAND) && TA_DINO_BRAND
#include "ta_n22_rounds.h"
#else
#include "ta_n21_rounds.h"
#endif
#else
#include "ta_n20_rounds.h"
#endif
#else
#include "ta_n19_rounds.h"
#endif
#else
#include "ta_n18_rounds.h"
#endif
#elif defined(TA_VS_COMPACT_UI) && TA_VS_COMPACT_UI
#include "ta_n17_rounds.h"
#elif defined(TA_VS_MENU_NAV) && TA_VS_MENU_NAV
#include "ta_n16_rounds.h"
#elif defined(TA_VS_ACCOUNT_UX) && TA_VS_ACCOUNT_UX
#include "ta_n15_rounds.h"
#elif defined(TA_VS_ACCOUNTS) && TA_VS_ACCOUNTS
#include "ta_n14_rounds.h"
#elif defined(TA_VS_NATIVE_VIEW) && TA_VS_NATIVE_VIEW
#include "ta_n13_rounds.h"
#elif defined(TA_VS_FIRST_LAUNCH) && TA_VS_FIRST_LAUNCH
#include "ta_n12_rounds.h"
#elif defined(TA_VS_FAST_START) && TA_VS_FAST_START
#include "ta_n11_rounds.h"
#elif defined(TA_VS_REMATCH) && TA_VS_REMATCH
#include "ta_n10_rounds.h"
#else
extern "C" void TA_LanGame_Run(const uint8_t*rom,unsigned bytes){
 TA_Log("[TA " TA_LAN_BUILD " LAN] boot identity=" TA_LAN_BUILD "_INDEPENDENT_LAN_VERSUS local_controls=immediate rollback=0 guest_timing_guards=original profiling=off build_candidate=1\n");
 for(;;){
  releaseGame();
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
  leavePublic();peerSettings=0;
#endif
  ta_vs_udp_close(&transport);memset(&lobby,0,sizeof lobby);
  memset(&pictureTx,0,sizeof pictureTx);pictureTx.fieldOnly=1;memset(&pictureRx,0,sizeof pictureRx);
  peerReady=peerLoaded=peerLost=peerQuit=remoteRevision=streamTick=captureRevision=0;overlayRevision=0;
  unsigned state=TA_LAN_SELECT,previousState=~0u;uint16_t previous=0;uint32_t localIp=0;int error=0;bool localReady=false;
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
  bool settingsSent=false;
#endif
  /* Discovery/ready UI. A and X select host/join; A confirms ready on each
     device after Connected. Y returns to the unchanged offline game. */
  for(;;){
   uint16_t pads[2];TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];
   if(pressed&(1u<<9)){
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
    leavePublic();
#endif
    ta_vs_udp_close(&transport);return;}
   if(state!=TA_LAN_SELECT&&(pressed&(1u<<8)))break;
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
   if(state==TA_LAN_SELECT&&(pressed&8)){error=browsePublic(rom,bytes,&localIp);if(error==1){error=0;break;}state=error?TA_LAN_ERROR:TA_LAN_ADDRESS;}
#endif
   if(state==TA_LAN_SELECT&&(pressed&3)){
    unsigned role=(pressed&1)?0:1;
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
    error=beginLobby(rom,bytes,role,false);state=error?TA_LAN_ERROR:TA_LAN_ADDRESS;
#else
    error=ta_vs_udp_open(&transport,role?0:TA_VS_LAN_PORT);
    if(!error)error=ta_vs_udp_enable_broadcast(&transport);
    if(error)state=TA_LAN_ERROR;
    else{
     TaVsIdentity id;memset(&id,0,sizeof id);ta_rom_sha256(rom,bytes,id.rom);
     static const uint8_t rules[]="TA_NATIVE_VS_" TA_LAN_BUILD "_P0_AUTHORITY_DELIVERY_MAILBOX_STATIC_P1_RGB_TILES_LEVEL5_V1";
     ta_rom_sha256(rules,sizeof rules-1,id.rules);
     uint32_t now=GetTickCount();uint64_t nonce=__builtin_readcyclecounter()^((uint64_t)now<<32);if(!nonce)nonce=1;id.seed=(uint32_t)nonce;
     error=ta_vs_lan_init(&lobby,&id,role,nonce,now);state=error?TA_LAN_ERROR:TA_LAN_ADDRESS;
    }
#endif
   }
   if(state!=TA_LAN_SELECT&&state!=TA_LAN_ERROR){
    XNADDR address;DWORD flags=XNetGetTitleXnAddr(&address);
    if(flags&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_AUTO|XNET_GET_XNADDR_PPPOE)){
     localIp=address.ina.s_addr;
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
     if(publicMode)publicRoom.localAddress=localIp;
#endif
     error=pump(false);
     state=error?TA_LAN_ERROR:ta_vs_lan_ready(&lobby)?TA_LAN_CONNECTED:lobby.pinned?TA_LAN_HANDSHAKE:lobby.role?TA_LAN_JOIN_WAIT:TA_LAN_HOST_WAIT;
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
     if(state==TA_LAN_CONNECTED&&!localReady){bool changed=false;
      if(pressed&0x40){ownCharacter=(ownCharacter+TA_VS_CHARACTERS-1)%TA_VS_CHARACTERS;changed=true;}
      if(pressed&0x80){ownCharacter=(ownCharacter+1)%TA_VS_CHARACTERS;changed=true;}
      if(!lobby.role){if((pressed&0x10)&&sharedSpeed<10){sharedSpeed++;changed=true;}if((pressed&0x20)&&sharedSpeed>1){sharedSpeed--;changed=true;}}
      if(publicMode&&!lobby.role){publicRoom.speed=(uint8_t)sharedSpeed;if(changed)publicRoom.kept=0;}
      if(!settingsSent||changed){error=sendControl(4);settingsSent=true;if(error)state=TA_LAN_ERROR;}
     }
#endif
     if(state==TA_LAN_CONNECTED&&(pressed&1)&&!localReady){error=sendControl(0);if(!error)localReady=true;else state=TA_LAN_ERROR;}
     if(localReady&&peerReady&&!error)break;
    }
   }
   if(state!=previousState){TA_Log("[TA " TA_LAN_BUILD " LAN] state=%u role=%u rejected=%u error=%d\n",state,(unsigned)lobby.role,lobby.rejected,error);previousState=state;}
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
   if(state==TA_LAN_CONNECTED)renderOptions(localReady);else {
#endif
   ta_vs_lan_render(screen,state,localIp,lobby.pinned?lobby.peerAddress:0,peerReady,error);
   if(localReady)ta_vs_lan_text(screen,30,320,"READY - WAITING FOR PEER",0x006affa8,2);
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
   if(state==TA_LAN_SELECT)ta_vs_lan_text(screen,30,290,"START PUBLIC ROOMS",0x006affa8,3);
   }
#endif
   TA_Video_Submit((const uint8_t*)screen);
  }
  if(!localReady||!peerReady||error)continue;
  /* Genuine-ROM menu route, not a bundled ROM/save state. Loading and the
     ready barrier happen before players receive control of a match. */
  Snes*s=snes_init();if(!s||!snes_loadRom(s,rom,bytes)||!ta_native_init(rom,bytes)){if(s)snes_free(s);caption("LOAD FAILED","B RETURN TO LAN");continue;}
  snes_setPixelFormat(s,pixelFormatXRGB);bool leave=false;
  const unsigned setupFrames=
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
   TA_VS_OPTIONS_SETUP_FRAMES;
#else
   TA_VS_SETUP_FRAMES;
#endif
  for(unsigned frame=0;frame<setupFrames;frame++){
   uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12)==12){leave=true;break;}
   if((error=pump(false))!=0)break;
   for(unsigned p=0;p<2;p++){unsigned buttons=
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
    ta_vs_setup_options(frame,p,sharedSpeed,ownCharacter);
#else
    ta_vs_setup_buttons(frame,p);
#endif
    for(unsigned b=0;b<12;b++)snes_setButtonState(s,p+1,b,(buttons>>b)&1);}
   snes_runFrame(s);snes_setSamples(s,audio,800);
   if(frame%120==0){char progress[80];snprintf(progress,sizeof progress,"PREPARING ORIGINAL GAME %u/100",frame*100/setupFrames);caption("LOADING VERSUS",progress,true);}
  }
  if(leave||error){snes_free(s);continue;}
  if(word(s->ram,0x2a8)!=2||s->ppu->mode!=2){snes_free(s);caption("SETUP CHECK FAILED","B RETURN TO LAN");continue;}
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
  if(word(s->ram,0x2ac)!=sharedSpeed||word(s->ram,0x2ae)!=sharedSpeed||word(s->ram,0x2ba)!=ta_vs_character_stage(ownCharacter)){snes_free(s);caption("SETUP OPTIONS FAILED","B RETURN TO VERSUS");continue;}
#endif
  ta_vs_local_init(&game,s,&lobby.session,lobby.identity.seed);ta_vs_local_bind(&game);
  if((error=sendControl(1))!=0)continue;
  uint32_t startAt=0;
  while(!peerLoaded||!startAt||(int32_t)(GetTickCount()-startAt)<0){
   uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12)==12){leave=true;break;}
   if((error=pump(false))!=0)break;
   if(peerLoaded&&!startAt)startAt=GetTickCount()+500;
   caption(peerLoaded?"GET READY":"WAITING FOR PEER","LOCAL CONTROLS START WITH MATCH",true);
  }
  if(leave||error)continue;
  const unsigned matchLevel=
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
   sharedSpeed;
#else
   5;
#endif
  TA_Log("[TA " TA_LAN_BUILD " MATCH] running role=%u level=%u local=P0 opponent=display_only seed=%lu setup_frames=%u\n",(unsigned)lobby.role,matchLevel,(unsigned long)lobby.identity.seed,setupFrames);
#if (!defined(TA_NO_LOG) || !TA_NO_LOG) && defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
  memset(coarse,0,sizeof coarse);
#endif
#if defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
  TA_Log("[TA N9 OPTIONS] speed=%u character=%u peer_character=%u transport=%s\n",sharedSpeed,ownCharacter,peerCharacter,publicMode?"PUBLIC":"LAN");
#endif
  unsigned gpuFrames=0,rgbFrames=0;
  unsigned frames=0;bool lost=false,quit=false,ended=false,backRequested=false;uint32_t backAt=0;uint32_t begun=GetTickCount(),lastLog=begun;unsigned lastFrames=0;unsigned lastUpdates=~0u,lastSeconds=~0u;
  previous=0;
  for(;;){
   uint16_t pads[2];TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];++streamTick;
   if((error=pump(true))!=0)break;
   if(ended&&(pressed&(1u<<8))){backRequested=true;if(!backAt)backAt=GetTickCount();}
   if(backRequested&&(!ta_vs_pending(&lobby.session)||(uint32_t)(GetTickCount()-backAt)>=3000))break;
   if(!ended&&(pads[0]&12)==12){quit=true;lost=true;if((error=sendControl(3))!=0)break;ended=true;}
   if(peerLost||peerQuit)ended=true;
   if(!ended){
    /* Network mode has no unilateral pause; Back+Start forfeits. Original
       movement, swaps and raise buttons are otherwise passed unchanged. */
    uint16_t buttons=pads[0]&~(1u<<3);
    for(unsigned b=0;b<12;b++){snes_setButtonState(s,1,b,(buttons>>b)&1);snes_setButtonState(s,2,b,false);}
    bool captureFrame=(frames%4)==0;
    if(!captureFrame)TA_Video_BeginR126(s->ppu);
    N7_TICK(coreStart);snes_runFrame(s);N7_ADD(coreStart,1);++frames;
    if(game.error){error=game.error;break;}
    if(game.ticks&&word(s->ram,0x2a0)==5){lost=true;ended=true;if((error=sendControl(2))!=0)break;}
    if(captureFrame){
     N7_TICK(captureStart);
#if !defined(TA_VS_ACTIVE_CAPTURE) || !TA_VS_ACTIVE_CAPTURE
     ppu_r126ResolveAll(s->ppu);
#endif
     if(!ppu_taCopyActive224(s->ppu,(uint8_t*)capture,sizeof capture)){
      error=TA_VS_CONFLICT;break;
     }
     sourceReady=true;
#if defined(TA_VS_PREPARED_PICTURE) && TA_VS_PREPARED_PICTURE
     if(ta_vs_picture_prepare(&pictureTx,capture,512,++captureRevision)){error=TA_VS_BAD_ARGUMENT;break;}
#endif
     N7_ADD(captureStart,2);
    }
    N7_TICK(videoStart);
#if defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
    /* During live play only the two original board images are drawn. */
    if(!updateOpponent()){error=TA_VS_CONFLICT;break;}
#else
    unsigned seconds=(GetTickCount()-begun)/1000;
    const char*label=ended?(lost?(peerLost?"DRAW":"LOST"):"WIN"):"PLAY";
    if(lastUpdates!=pictureRx.updates||lastSeconds!=seconds||ended){if(!updateOverlay(label,seconds)){error=TA_VS_CONFLICT;break;}lastUpdates=pictureRx.updates;lastSeconds=seconds;}
#endif
    bool submitted=!captureFrame&&TA_Video_EndR126(s->ppu);if(submitted)gpuFrames++;else rgbFrames++;
    if(!submitted&&!TA_Video_SubmitNative224Direct(s->ppu)&&!TA_Video_SubmitActive224Direct(s->ppu)){
     if(!captureFrame){
#if !defined(TA_VS_ACTIVE_CAPTURE) || !TA_VS_ACTIVE_CAPTURE
      ppu_r126ResolveAll(s->ppu);
#endif
      if(!ppu_taCopyActive224(s->ppu,(uint8_t*)capture,sizeof capture)){error=TA_VS_CONFLICT;break;}sourceReady=true;
#if defined(TA_VS_PREPARED_PICTURE) && TA_VS_PREPARED_PICTURE
      if(ta_vs_picture_prepare(&pictureTx,capture,512,++captureRevision)){error=TA_VS_BAD_ARGUMENT;break;}
#endif
     }
     presentCapture();
    }
    N7_ADD(videoStart,3);N7_TICK(audioStart);snes_setSamples(s,audio,800);TA_Audio_Submit(audio);N7_ADD(audioStart,4);
   }else{
    game.active=0;ta_vs_local_bind(NULL);
#if defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
    TA_Video_SetLanOverlay(NULL,0);
    caption(lost?(peerLost?"DRAW":"YOU LOST"):"YOU WIN","B RETURN TO LAN");
#else
    unsigned seconds=(GetTickCount()-begun)/1000;if(!updateOverlay(lost?(peerLost?"DRAW":"LOST"):"WIN",seconds)){error=TA_VS_CONFLICT;break;}
    if(sourceReady)presentCapture();else caption("MATCH ENDED","B RETURN TO LAN");
#endif
    memset(audio,0,sizeof audio);TA_Audio_Submit(audio);
   }
   if((error=pump(true))!=0)break;
   uint32_t now=GetTickCount();
   if((uint32_t)(now-lastLog)>=5000){TA_Log("[TA " TA_LAN_BUILD " MATCH] frames=%u interval_frames=%u wall_ms=%lu fps_x100=%lu active=%u sent_attacks=%lu received_attacks=%lu picture_tiles=%u packets_tx=%lu packets_rx=%lu rejected=%u local_loss=%u peer_loss=%u quit=%u\n",frames,frames-lastFrames,(unsigned long)(now-lastLog),(unsigned long)((uint64_t)(frames-lastFrames)*100000u/(now-lastLog)),(unsigned)!ended,(unsigned long)game.sent,(unsigned long)game.received,pictureRx.updates,(unsigned long)transport.sent,(unsigned long)transport.received,lobby.rejected,(unsigned)lost,peerLost,(unsigned)quit);
#if (!defined(TA_NO_LOG) || !TA_NO_LOG) && defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
    TA_Log("[TA " TA_LAN_BUILD " COST] net_ms=%lu core_ms=%lu capture_prepare_ms=%lu video_present_ms=%lu audio_ms=%lu gpu_frames=%u rgb_frames=%u view_prepares=%lu cumulative_frames=1 interval_ms=1\n",coarse[0],coarse[1],coarse[2],coarse[3],coarse[4],gpuFrames,rgbFrames,(unsigned long)captureRevision);
    memset(coarse,0,sizeof coarse);
#endif
    lastLog=now;lastFrames=frames;}
  }
  TA_Log("[TA " TA_LAN_BUILD " MATCH] stopped frames=%u sent=%lu received=%lu error=%d guard_pc=%06lX rng_active=%u row_active=%u\n",frames,(unsigned long)game.sent,(unsigned long)game.received,error,(unsigned long)game.lastPc,(unsigned)game.rng.active,(unsigned)game.rows.active);
  releaseGame();
  if(error){for(;;){uint16_t pads[2];TA_Input_Poll(pads);if(pads[0]&(1u<<8))break;caption(error==-10060?"CONNECTION LOST":"MATCH STOPPED","B RETURN TO LAN - SEE LOG");}}
 }
}

#endif /* N10 persistent rounds / legacy single match */
