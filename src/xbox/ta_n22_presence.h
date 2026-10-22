/* R137: independent account presence on the frontend thread, including offline
   play. No threads, blocking authentication, game packets or guest RAM writes. */
#ifndef TA_N22_PRESENCE_H
#define TA_N22_PRESENCE_H
static TaVsUdp taPresenceUdp;
static TaVsPublic taPresenceRoom;
static TaAccountLink taPresenceLink;
static TaVsIdentity taPresenceIdentity;
static char taPresenceName[16];static uint8_t taPresenceToken[32];
static bool taPresenceEnabled,taPresenceAuthenticated;
static uint32_t taPresenceNext,taPresenceTickAt,taPresenceRequestAt,taPresenceCountsAt;
static unsigned taPresenceCommand,taPresenceSerial;
static void taPresenceDrop(uint32_t now,unsigned delay){
 if(taPresenceUdp.opened&&taPresenceRoom.haveCookie){uint8_t b[32];size_t n=ta_vs_public_leave(&taPresenceRoom,b,sizeof b);if(n)ta_vs_udp_send_to(&taPresenceUdp,taPresenceRoom.serverAddress,taPresenceRoom.serverPort,b,n);}
 ta_vs_udp_close(&taPresenceUdp);memset(&taPresenceRoom,0,sizeof taPresenceRoom);memset(&taPresenceLink,0,sizeof taPresenceLink);
 taPresenceAuthenticated=false;taPresenceCommand=0;taPresenceNext=now+delay;
}
static bool taPresenceRequest(unsigned command,const void*data,unsigned bytes,uint32_t now){
 if(ta_account_request(&taPresenceLink,&taPresenceRoom,command,data,bytes))return false;
 taPresenceCommand=command;taPresenceRequestAt=now;return true;
}
static void taPresenceTick(void){
 if(!taPresenceEnabled)return;uint32_t now=GetTickCount();if((uint32_t)(now-taPresenceTickAt)<100u)return;taPresenceTickAt=now;
 if(!profileCount||profileActive>=profileCount){if(taPresenceUdp.opened)taPresenceDrop(now,0);memset(taPresenceName,0,sizeof taPresenceName);memset(taPresenceToken,0,sizeof taPresenceToken);return;}
 if(memcmp(taPresenceName,savedProfiles[profileActive].name,16)||memcmp(taPresenceToken,savedProfiles[profileActive].token,32)){
  taPresenceDrop(now,0);memcpy(taPresenceName,savedProfiles[profileActive].name,16);memcpy(taPresenceToken,savedProfiles[profileActive].token,32);
 }
 if((int32_t)(now-taPresenceNext)<0)return;
 if(!taPresenceUdp.opened){
  if(ta_vs_udp_open(&taPresenceUdp,0)){taPresenceNext=now+15000u;return;}
  uint8_t secret[32];if(XNetRandom(secret,sizeof secret)||ta_account_init(&taPresenceLink,secret,ta_n17_account_key)){memset(secret,0,sizeof secret);taPresenceDrop(now,15000);return;}memset(secret,0,sizeof secret);
  uint32_t address=ta_vs_public_ipv4(TA_N17_SERVER);unsigned port=TA_N17_PORT;
  #if defined(TA_ANDROID)
  FILE*f=NULL;
#else
  FILE*f=fopen("D:\\ta-server.txt","rb");
#endif
  if(f){char ip[64];unsigned p=TA_N17_PORT;if(fscanf(f,"%63s %u",ip,&p)>=1&&ta_vs_public_ipv4(ip)&&p&&p<65536){address=ta_vs_public_ipv4(ip);port=p;}fclose(f);}
  uint64_t nonce=__builtin_readcyclecounter()^((uint64_t)now<<32)^(++taPresenceSerial);if(!nonce)nonce=1;
  ta_vs_public_init(&taPresenceRoom,&taPresenceIdentity,nonce,address,(uint16_t)port,taPresenceUdp.localPort,now);taPresenceCountsAt=now-10000u;
 }
 XNADDR local;memset(&local,0,sizeof local);DWORD status=XNetGetTitleXnAddr(&local);
 if(!(status&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_AUTO|XNET_GET_XNADDR_PPPOE))||!local.ina.s_addr){
  if((uint32_t)(now-taPresenceRoom.started)>30000u)taPresenceDrop(now,15000);return;
 }
 taPresenceRoom.localAddress=local.ina.s_addr;
 for(unsigned k=0;k<4;k++){
  uint8_t b[TA_PUB_BYTES];uint32_t from;uint16_t port;int n=ta_vs_udp_receive_any(&taPresenceUdp,b,sizeof b,&from,&port);if(!n)break;
  if(n<0){if(n==-WSAEMSGSIZE)continue;taPresenceDrop(now,5000);return;}
  if(from!=taPresenceRoom.serverAddress||port!=taPresenceRoom.serverPort)continue;
  if(n>=6&&b[5]==16){if(!ta_account_receive(&taPresenceLink,&taPresenceRoom,b,(size_t)n))taPresenceRoom.lastServer=now;}
  else ta_vs_public_receive(&taPresenceRoom,now,from,port,b,(size_t)n);
 }
 if(taPresenceRoom.error||(uint32_t)(now-taPresenceRoom.lastServer)>20000u){taPresenceDrop(now,5000);return;}
 if(taPresenceCommand){
  if(taPresenceLink.pending){if((uint32_t)(now-taPresenceRequestAt)>6000u){taPresenceDrop(now,5000);return;}}
  else{
   const uint8_t*b=taPresenceLink.reply;unsigned n=taPresenceLink.replyBytes;
   if(n<2||b[0]!=taPresenceCommand||b[1]){taPresenceDrop(now,n>=2&&b[1]==3?60000u:5000u);return;}
   if(taPresenceCommand==TA_ACCOUNT_RESUME){TaAccountProfile p;if(n!=78||ta_account_profile(&p,b+34,44)){taPresenceDrop(now,15000);return;}taPresenceAuthenticated=true;TA_Log("[TA R137 PRESENCE] signed_in=1 background=1\n");}
   else if(taPresenceCommand==TA_ACCOUNT_COUNTS&&n==14){taSocialAccounts=taSocialU32(b+2);taSocialInGame=taSocialU32(b+6);taSocialOnline=taSocialU32(b+10);taSocialCountKnown=1;TA_Video_SetPopulation(taSocialAccounts,taSocialInGame,taSocialOnline,1);}
   taPresenceCommand=0;taPresenceLink.replyBytes=0;
  }
 }
 /* A LIST keeps an authenticated connection alive without creating a room.
    The ordinary browse protocol retries at 500 ms; this channel needs 5 s. */
 if(!taPresenceRoom.haveCookie||!taPresenceRoom.requested||(uint32_t)(now-taPresenceRoom.lastRequest)>=5000u){
  uint8_t b[TA_PUB_BYTES];uint32_t to;uint16_t port;size_t n=ta_vs_public_packet(&taPresenceRoom,now,b,sizeof b,&to,&port);
  if(n&&ta_vs_udp_send_to(&taPresenceUdp,to,port,b,n)<0){taPresenceDrop(now,5000);return;}
 }
 if(taPresenceRoom.haveCookie&&!taPresenceCommand){
  if(!taPresenceAuthenticated){uint8_t data[48];memcpy(data,taPresenceName,16);memcpy(data+16,taPresenceToken,32);taPresenceRequest(TA_ACCOUNT_RESUME,data,sizeof data,now);memset(data,0,sizeof data);}
  else if((uint32_t)(now-taPresenceCountsAt)>=10000u){if(taPresenceRequest(TA_ACCOUNT_COUNTS,NULL,0,now))taPresenceCountsAt=now;}
 }
 const uint8_t*b;size_t n=ta_account_packet(&taPresenceLink,now,&b);if(n&&ta_vs_udp_send_to(&taPresenceUdp,taPresenceRoom.serverAddress,taPresenceRoom.serverPort,b,n)<0)taPresenceDrop(now,5000);
}
static void taPresenceStart(const uint8_t*rom,unsigned bytes){
 uint32_t now=GetTickCount();taPresenceDrop(now,0);memset(taPresenceName,0,sizeof taPresenceName);memset(taPresenceToken,0,sizeof taPresenceToken);accountProfiles(false);
 memset(&taPresenceIdentity,0,sizeof taPresenceIdentity);ta_rom_sha256(rom,bytes,taPresenceIdentity.rom);
 static const uint8_t rules[]="TA_NATIVE_VS_N11_PREBOARD_SEED_FAST_START_V1";ta_rom_sha256(rules,sizeof rules-1,taPresenceIdentity.rules);
 taPresenceEnabled=true;taPresenceTickAt=now-100u;TA_Background_Set(taPresenceTick);
}
#endif
