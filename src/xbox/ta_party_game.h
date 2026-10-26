/* Independent party rooms: legacy accounts/ratings and two-player rooms retain
   their existing protocol. All remote boards are presentation-only. */
extern "C" {
#include "ta_vs_party.h"
}
#include "ta_vs_party_view.h"
static TaVsParty party;
static bool partyReplayPending;
static TaVsUdp partyTransport;
static TaVsPictureRx partyPictures[4];
static uint32_t partyRemote[4],partyViewRevision;
static TaPartyVisual partyDisplay;
static uint32_t partyAddress;
static uint16_t partyPort;
static uint32_t partyAccountAt;
static int partyPump(void){
 N7_TICK(partyNetwork);
 uint32_t now=GetTickCount();
 if(transport.opened&&publicMode&&(uint32_t)(now-partyAccountAt)>=50u){partyAccountAt=now;pump(false);}
 if(game.sendContext==&party&&party.phase==2){int r=ta_vs_local_flush(&game);if(r)return r;}
 for(unsigned k=0;k<24;k++){
  uint8_t b[TA_PARTY_BYTES];uint32_t from;uint16_t port;int n=ta_vs_udp_receive_any(&partyTransport,b,sizeof b,&from,&port);if(!n)break;
  if(n<0){if(n==-WSAEMSGSIZE)continue;return n;}if(from!=partyAddress||port!=partyPort)continue;
  int r=ta_vs_party_receive(&party,now,b,(size_t)n);if(r==TA_VS_CONFLICT||r==TA_VS_IDENTITY_MISMATCH||r<=-300)return r;
 }
 for(unsigned k=0;k<8;k++){uint8_t b[TA_PARTY_BYTES];size_t n=ta_vs_party_packet(&party,now,b,sizeof b);if(!n)break;int r=ta_vs_udp_send_to(&partyTransport,partyAddress,partyPort,b,n);if(r<0)return r;}
 if((uint32_t)(now-party.lastServer)>15000u)return -10060;
 for(unsigned s=0;s<4;s++)if(s!=party.self&&(party.members&(1u<<s))){size_t bytes;uint32_t revision;const uint8_t*p=ta_vs_remote_snapshot(party.peer+s,&bytes,&revision);
  if(p&&revision!=partyRemote[s]){partyRemote[s]=revision;if(ta_vs_picture_receive(partyPictures+s,p,bytes))return TA_VS_BAD_PACKET;}}
 N7_ADD(partyNetwork,0);return 0;
}
static void partyClose(void){
 partyReplayPending=false;TA_Video_SetPartyOverlay(NULL,0);ta_vs_local_bind(NULL);game.active=0;
 if(partyTransport.opened&&party.haveCookie){party.leaving=1;party.error=0;for(unsigned k=0;k<3;k++){uint8_t b[TA_PARTY_BYTES];size_t n=ta_vs_party_packet(&party,GetTickCount(),b,sizeof b);if(n)ta_vs_udp_send_to(&partyTransport,partyAddress,partyPort,b,n);}}
 ta_vs_udp_close(&partyTransport);releaseGame();
}
static bool partyLinksReady(void){for(unsigned s=0;s<4;s++)if(s!=party.self&&(party.members&party.present&(1u<<s))&&!ta_vs_ready(party.peer+s))return false;return true;}
static int partyReceiveAttacks(void*context,uint8_t*w,size_t n){return ta_vs_party_deliver((TaVsParty*)context,w,n);}
static int partyLoad(const uint8_t*rom,unsigned bytes){
 Snes*s=snes_init();if(!s)return TA_VS_BAD_ARGUMENT;game.s=s;
 if(!snes_loadRom(s,rom,bytes)||!ta_native_init(rom,bytes))return TA_VS_BAD_ARGUMENT;snes_setPixelFormat(s,pixelFormatXRGB);
 bool hit=readRoundTemplate(party.speed,ownCharacter);if(ta_vs_start_load(hit?roundStart:menuStart,s))return TA_VS_BAD_ARGUMENT;
 uint32_t drawn=0;
 for(unsigned frame=hit?2425:1840;frame<TA_VS_OPTIONS_SETUP_FRAMES;frame++){
  uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12)==12)return TA_VS_CONFLICT;
  int r=partyPump();if(r)return r;if(party.phase==3||party.phase==0)return TA_VS_CONFLICT;
  if(frame==2425&&ta_vs_start_seed(s,party.seed))return TA_VS_CONFLICT;
  for(unsigned p=0;p<2;p++){unsigned buttons=frame<2425?ta_vs_setup_options(frame,p,party.speed,ownCharacter):0;for(unsigned b=0;b<12;b++)snes_setButtonState(s,p+1,b,(buttons>>b)&1);}
  s->ppu->noPixels=frame+1<TA_VS_OPTIONS_SETUP_FRAMES;snes_runFrame(s);snes_setSamples(s,audio,800);
  uint32_t now=GetTickCount();if((uint32_t)(now-drawn)>80){caption("PARTY GET READY","PREPARING YOUR BOARD",true);drawn=now;}
 }
 s->ppu->noPixels=false;
 if(word(s->ram,0x2a8)!=2||s->ppu->mode!=2||word(s->ram,0x2ac)!=party.speed||word(s->ram,0x2ae)!=party.speed||word(s->ram,0x2ba)!=ta_vs_character_stage(ownCharacter))return TA_VS_CONFLICT;
 ta_vs_local_init(&game,s,NULL,party.seed);game.sendContext=&party;game.selectTarget=ta_vs_party_target;game.sendAttack=ta_vs_party_attack_to;game.receiveAttacks=partyReceiveAttacks;
 while(!partyLinksReady()){int r=partyPump();if(r)return r;if(party.phase==3||party.phase==0)return TA_VS_CONFLICT;uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12)==12)return TA_VS_CONFLICT;caption("PARTY GET READY","CONNECTING ALL PLAYERS",true);}
 party.flags|=2;party.requestSent=0;
 while(party.phase==1){int r=partyPump();if(r)return r;uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12)==12)return TA_VS_CONFLICT;caption("PARTY GET READY","WAITING FOR ALL BOARDS",true);}
 if(party.phase!=2)return TA_VS_CONFLICT;ta_vs_local_bind(&game);return 0;
}
static bool partyDisplayUpdate(unsigned&state,unsigned&target){
 unsigned next=ta_vs_party_next_target(&party);if(state!=party.revision||target!=next){ta_party_labels(overlay,&party);++partyViewRevision;state=party.revision;target=next;}
 partyDisplay.members=party.members;partyDisplay.self=party.self;partyDisplay.mode=party.mode;partyDisplay.roster=overlay;partyDisplay.rosterRevision=partyViewRevision;
 for(unsigned s=0;s<4;s++){partyDisplay.pixels[s]=partyPictures[s].pixels;partyDisplay.revision[s]=partyPictures[s].updates;}
 return TA_Video_SetPartyVisual(&partyDisplay);
}
static void partySpeedInput(unsigned pressed){
 if(!(pressed&(0x40u|0x80u)))return;
 unsigned current=party.speedRequest?party.speedRequest:party.self==party.host?party.speed:party.speedRequests[party.self]?party.speedRequests[party.self]:party.speed;
 unsigned speed=(pressed&0x80u)?(current<10?current+1:10):(current>1?current-1:1);ta_vs_party_speed(&party,speed);
}
static void partyTouchSpeed(int y=158);
static void partyResults(bool win,bool armed,const char*winnerName){
 menuTouchEnabled=armed;
 accountFrame(win?"PARTY YOU WIN":winnerName?"PARTY MATCH ENDED":"REPLAYING ROUND",!winnerName?"SIMULTANEOUS TOP-OUT - NO RATING CHANGE":party.ranked?"PARTY RATING RECORDED":"CASUAL PARTY - SAME ROOM REMATCH");
 ta_result_banner(screen,winnerName,158,(uint32_t)GetTickCount());
 char text[80];snprintf(text,sizeof text,"SPEED %u  LEFT/RIGHT %s",party.speed,party.self==party.host?"CHANGE":"REQUEST");ta_vs_lan_text_box(screen,96,201,320,14,text,uiGold,2);partyTouchSpeed(201);
 for(unsigned seat=0;seat<4;seat++)if(party.members&(1u<<seat)){
  const char*status=!(party.present&(1u<<seat))?"LEFT ROOM":party.ready&(1u<<seat)?"WANTS REMATCH":"DECIDING";
  snprintf(text,sizeof text,"%-8s %s%s",party.names[seat],status,seat==party.host?" HOST":"");ta_menu_text(screen,58,224+seat*34,text,seat==party.self?uiGold:uiText,2);
  if(party.speedRequests[seat]){snprintf(text,sizeof text,"REQUESTS SPEED %u",party.speedRequests[seat]);ta_menu_text(screen,170,241+seat*34,text,uiMuted,2);}
 }
 ta_menu_text(screen,44,368,"ALL REMATCH VOTES RETURN TO THE LOBBY",uiMuted,2);
 accountFooter(armed?(partyReplayPending?"REPLAYING AUTOMATICALLY  B LEAVE":"A REMATCH  B LEAVE"):"RESULTS - INPUT LOCKED BRIEFLY",party.waitingCount?"NEW PLAYER JOINED - NEXT ROUND":party.returning?"WAITING FOR OTHER PLAYERS":"CHOOSE SPEED THEN REQUEST REMATCH");menuTouchEnabled=true;
}
static int partyResultsConfirmTick(void){int error=partyPump();return error?error:party.phase!=3?1:0;}
static int partyPlay(void){
 TA_Music_SetScene(TA_MUSIC_GAME);
 Snes*s=game.s;unsigned frame=0,tick=0,revision=0,viewState=~0u,viewTarget=~0u;uint16_t previous=0,previousGame=0;bool lost=false,ended=false,inputArmed=false;uint32_t endedAt=0;
 char winnerName[17]={0};bool resultWin=false;
 const unsigned captureInterval=ta_party_capture_interval(ta_vs_party_count(party.members));
#if !defined(TA_NO_LOG) || !TA_NO_LOG
 uint32_t logAt=GetTickCount();unsigned logFrames=0,gpuFrames=0,rgbFrames=0,captures=0;unsigned long captureCoreMs=0,gpuCoreMs=0;memset(coarse,0,sizeof coarse);unsigned long unusedStats[4];TA_Video_PartyStats(unusedStats);
#endif
 memset(&pictureTx,0,sizeof pictureTx);pictureTx.fieldOnly=0;
 for(;;){
  uint16_t pads[2];TA_Input_Poll(pads);int x,y;TA_Input_RightStick(&x,&y);ta_vs_party_focus(&party,x,y);
  uint16_t pressed=pads[0]&~previous;previous=pads[0];if(ended){int ignored=0;pressed|=(uint16_t)TA_Menu_Read(&ignored);}
  uint16_t mapped=TA_Input_GameButtons(pads[0],0),gamePressed=mapped&~previousGame;previousGame=mapped;
  if(party.mode&&(gamePressed&0x1000)){ta_vs_party_focus(&party,0,0);ta_vs_party_focus(&party,20000,0);}
  if(++tick==0)++tick;int error=partyPump();if(error)return error;
  if((pads[0]&12)==12&&!lost){lost=true;party.flags|=4;party.requestSent=0;ta_vs_local_bind(NULL);game.active=0;}
  if(party.phase==3&&!ended){unsigned winner=ta_result_winner_slot(party.alive);
   if(winner<4u)snprintf(winnerName,sizeof winnerName,"%.16s",party.names[winner][0]?party.names[winner]:"PLAYER");
   partyReplayPending=winner>=4u;resultWin=winner==party.self;ended=true;endedAt=GetTickCount();inputArmed=false;TA_Video_SetPartyVisual(NULL);ta_vs_local_bind(NULL);game.active=0;}
  if(ended){TaMenuMusicScope musicScope(TA_MUSIC_RESULTS);TaLobbyUiScope lobbyScope(partyResultsConfirmTick);if(party.phase==0||(party.phase==1&&party.returning==0))return 1;if(!inputArmed&&(uint32_t)(GetTickCount()-endedAt)>=1000u&&!(pads[0]&(1u|0x100u)))inputArmed=true;
   if(inputArmed&&(pressed&0x100u)){int status=0;bool leave=accountConfirmLeave("LEAVE THE PARTY INSTEAD OF REMATCHING?",partyResultsConfirmTick,&status);if(status<0)return status;if(leave)return 0;TA_Input_Poll(pads);previous=pads[0];continue;}
   if((inputArmed&&((pressed&1u)||partyReplayPending))||(ta_vs_party_count(party.present)==1&&(party.present&(1u<<party.self))&&(uint32_t)(GetTickCount()-endedAt)>=1500u))ta_vs_party_return(&party);
   if(inputArmed)partySpeedInput(pressed);partyResults(resultWin,inputArmed,winnerName[0]?winnerName:NULL);memset(audio,0,sizeof audio);TA_Audio_Submit(audio);continue;}
  if(!lost){
   uint16_t buttons=mapped&~(8u|0x1000u);for(unsigned b=0;b<12;b++){snes_setButtonState(s,1,b,(buttons>>b)&1);snes_setButtonState(s,2,b,false);}
   bool captureFrame=(frame++%captureInterval)==0;if(!captureFrame)TA_Video_BeginR126(s->ppu);N7_TICK(partyCore);snes_runFrame(s);N7_ADD(partyCore,1);
#if !defined(TA_NO_LOG) || !TA_NO_LOG
   if(captureFrame){captureCoreMs+=GetTickCount()-partyCore;captures++;}else gpuCoreMs+=GetTickCount()-partyCore;
#endif
   if(game.error)return game.error;
   if(game.ticks&&word(s->ram,0x2a0)==5){lost=true;party.flags|=4;party.requestSent=0;ta_vs_local_bind(NULL);game.active=0;}
   N7_TICK(partyCapture);if(captureFrame){if(!ppu_taCopyActive224(s->ppu,(uint8_t*)capture,sizeof capture))return TA_VS_CONFLICT;TA_Video_SetPartyBackdrop(capture);if(ta_vs_picture_prepare(&pictureTx,capture,512,++revision))return TA_VS_BAD_PACKET;}
   uint8_t payload[TA_VS_SNAPSHOT_BYTES];size_t n=ta_vs_picture_next(&pictureTx,tick,payload,sizeof payload);
   if(n)for(unsigned peer=0;peer<4;peer++)if(peer!=party.self&&(party.members&party.present&(1u<<peer))){int r=ta_vs_set_snapshot(party.peer+peer,payload,n);if(r)return r;}
   N7_ADD(partyCapture,2);N7_TICK(partyVideo);if(!partyDisplayUpdate(viewState,viewTarget))return TA_VS_CONFLICT;
   bool submitted=!captureFrame&&TA_Video_EndR126(s->ppu);
#if !defined(TA_NO_LOG) || !TA_NO_LOG
   if(submitted)gpuFrames++;else rgbFrames++;
#endif
   if(!submitted&&!TA_Video_SubmitNative224Direct(s->ppu)&&!TA_Video_SubmitActive224Direct(s->ppu)){if(!ppu_taCopyActive224(s->ppu,(uint8_t*)capture,sizeof capture))return TA_VS_CONFLICT;presentCapture();}
   N7_ADD(partyVideo,3);N7_TICK(partyAudio);snes_setSamples(s,audio,800);TA_Audio_Submit(audio);N7_ADD(partyAudio,4);
  }else{
   if(!partyDisplayUpdate(viewState,viewTarget))return TA_VS_CONFLICT;presentCapture();memset(audio,0,sizeof audio);TA_Audio_Submit(audio);
  }
  error=partyPump();if(error)return error;
#if !defined(TA_NO_LOG) || !TA_NO_LOG
  uint32_t now=GetTickCount();if((uint32_t)(now-logAt)>=5000u){unsigned long stats[4];TA_Video_PartyStats(stats);
   TA_Log("[TA R136 PARTY] players=%u frames=%u fps_x100=%lu sent=%lu backlog=%u lost=%u capture_every=%u\n",ta_vs_party_count(party.members),frame,(unsigned long)((uint64_t)(frame-logFrames)*100000u/(now-logAt)),(unsigned long)game.sent,game.outgoingCount,(unsigned)lost,captureInterval);
   TA_Log("[TA R136 COST] net_ms=%lu core_ms=%lu capture_encode_ms=%lu video_ms=%lu audio_ms=%lu\n",coarse[0],coarse[1],coarse[2],coarse[3],coarse[4]);
   TA_Log("[TA R136 DISPLAY] gpu_frames=%u rgb_frames=%u captures=%u capture_core_ms=%lu gpu_core_ms=%lu peer_uploads=%lu names_uploads=%lu busy_skips=%lu copied_bytes=%lu\n",gpuFrames,rgbFrames,captures,captureCoreMs,gpuCoreMs,stats[0],stats[1],stats[2],stats[3]);
   memset(coarse,0,sizeof coarse);logAt=now;logFrames=frame;gpuFrames=rgbFrames=captures=0;captureCoreMs=gpuCoreMs=0;
  }
#endif
 }
}
static int partyChatError;
static bool partyChatWaiting;
static uint32_t partyChatRoom;
static uint64_t partyChatNonce;
static int partyChatTick(void){
 int error=partyPump();if(error){partyChatError=error;return error;}
 return party.room!=partyChatRoom||party.roomNonce!=partyChatNonce||(!party.waiting&&(partyChatWaiting||party.phase!=0))?1:0;
}
static int partyLobbyConfirmTick(void){int error=partyPump();return error?error:party.waiting?0:party.phase!=0?1:0;}
static void partyOpenChat(void){
 partyChatError=0;partyChatWaiting=party.waiting!=0;partyChatRoom=party.room;partyChatNonce=party.roomNonce;
 taSocialLobbyMenu(partyChatTick,2,party.room,party.roomNonce);
}
static void partyTouchSpeed(int y){
 menuTouchAction(36,y-5,54,25,0x40);menuTouchAction(422,y-5,54,25,0x80);
 ta_menu_text(screen,40,y,"<",uiGold,2);ta_menu_text(screen,458,y,">",uiGold,2);
}
static void runParty(const uint8_t*rom,unsigned bytes,uint32_t joinRoom=0,int hostMode=-1){
 uint64_t nonce=0;XNetRandom((BYTE*)&nonce,sizeof nonce);if(!nonce)nonce=__builtin_readcyclecounter()|1;
 if(!accountEnsure())return;
 bool verifyAccount=accountAuthenticated;publicPartyBrowsing=false;
 partyClose();discardWarm();TA_Video_SetLanOverlay(NULL,0);
 int error=ta_vs_udp_open(&partyTransport,0);if(error)return;
 TaVsIdentity id;memset(&id,0,sizeof id);ta_rom_sha256(rom,bytes,id.rom);static const uint8_t rules[]="DINO_PARTY_V4_EGG_RANKS";ta_rom_sha256(rules,sizeof rules-1,id.rules);
 ta_vs_party_init(&party,&id,nonce,profileCount?savedProfiles[profileActive].name:"PLAYER",GetTickCount());
 if(verifyAccount)party.authState=3;
 memset(partyRemote,0,sizeof partyRemote);memset(partyPictures,0,sizeof partyPictures);memset(&partyDisplay,0,sizeof partyDisplay);
 if(joinRoom)ta_vs_party_join(&party,joinRoom);else if(hostMode>=0)ta_vs_party_host(&party,sharedSpeed,(unsigned)hostMode);
 accountPartyEndpoint(&partyAddress,&partyPort);
 TaMenuPad pad;menuBegin(&pad);unsigned choice=0;bool networkReady=false,automaticAttempted=false;uint32_t networkOpened=GetTickCount();
 for(;;){
  TaLobbyUiScope lobbyScope(partyLobbyConfirmTick);
  unsigned pressed=menuRead(&pad);if(pressed&0x100){int status=0;bool leave=accountConfirmLeave("LEAVE THIS PARTY OR WAITING QUEUE?",partyLobbyConfirmTick,&status);menuBegin(&pad);if(status<0){error=status;break;}if(leave)break;continue;}
  XNADDR address;memset(&address,0,sizeof address);DWORD flags=XNetGetTitleXnAddr(&address);
  if(!networkReady&&(flags&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_AUTO|XNET_GET_XNADDR_PPPOE))&&address.ina.s_addr){networkReady=true;party.lastServer=GetTickCount();}
  if(!networkReady){if((uint32_t)(GetTickCount()-networkOpened)>30000u){error=-10060;break;}accountFrame("PARTY ONLINE","GETTING NETWORK ADDRESS");accountFooter("B BACK",NULL);continue;}
  if((error=partyPump())!=0){
   if(error==-10060&&!party.haveCookie&&!automaticAttempted&&(flags&XNET_GET_XNADDR_STATIC)){
    automaticAttempted=true;ta_vs_udp_close(&partyTransport);error=ta_vs_udp_open_dhcp(&partyTransport,0);if(error)break;
    party.lastServer=networkOpened=GetTickCount();party.requestSent=0;networkReady=false;continue;
   }break;
  }
  if(party.haveCookie&&party.authState==3){
   uint8_t request[24];for(unsigned k=0;k<8;k++)request[7-k]=(uint8_t)(nonce>>(k*8));memcpy(request+8,party.cookie,16);
   if(!accountCall(TA_ACCOUNT_PARTY_TICKET,request,24,"VERIFYING PARTY ACCOUNT")||accountLink.replyBytes!=78){error=-306;break;}
   memcpy(party.authTicket,accountLink.reply+2,76);party.authState=1;party.requestSent=0;party.lastServer=GetTickCount();continue;
  }
  if(verifyAccount&&party.authState==2&&!party.authenticated){error=-306;break;}
  if(party.room&&(party.waiting||party.phase==0)&&(pressed&4u)){partyOpenChat();if(partyChatError){error=partyChatError;break;}menuBegin(&pad);continue;}
  if(party.waiting){
   accountFrame("JOINED PUBLIC PARTY",party.waiting==3?"ROUND FINISHED - WAITING FOR LOBBY":"MATCH IN PROGRESS - NEXT ROUND");
   char waitText[64];snprintf(waitText,sizeof waitText,"ROOM %06lu  QUEUE %u",(unsigned long)party.room,(unsigned)party.queuePosition);ta_menu_text(screen,44,190,waitText,uiGold,2);
   ta_menu_text(screen,44,250,"CURRENT MATCH CONTINUES",uiText,2);accountRoomFooter(NULL,"NEXT LOBBY STARTS AFTER REMATCH VOTES");continue;
  }
   if(partyReplayPending&&party.phase==0){
    if(!(party.flags&1u)){party.flags|=1u;party.requestSent=0;}
    if(party.self==party.host&&ta_vs_party_count(party.present)>=2u&&party.ready==party.present&&!party.start){party.start=1;party.requestSent=0;}
   }
   if(party.phase==1){
    lobbyScope.suspend();partyReplayPending=false;error=partyLoad(rom,bytes);
   if(error&&party.cancelled&&party.phase==0){releaseGame();TA_Video_SetPartyVisual(NULL);error=0;menuBegin(&pad);continue;}
   int result=error?0:partyPlay();if(result<0)error=result;
   releaseGame();TA_Video_SetPartyVisual(NULL);
   if(error||result==0)break;
   memset(partyRemote,0,sizeof partyRemote);memset(partyPictures,0,sizeof partyPictures);memset(&partyDisplay,0,sizeof partyDisplay);menuBegin(&pad);continue;
  }
  /* A queued public join/create owns the connection until it completes.
     A during the handshake must not replace it with the default AUTO row. */
  if(!party.room&&party.request){
   accountFrame("PARTY ONLINE",party.request==TA_PARTY_JOIN?"JOINING PUBLIC ROOM":"CREATING PUBLIC ROOM");
   accountFooter("B CANCEL",party.haveCookie?"WAITING FOR ROOM":"CONNECTING TO PARTY RELAY");continue;
  }
  accountFrame("PARTY ONLINE",party.room?(party.ranked?"RANKED PARTY - VERIFIED ACCOUNTS":"CASUAL - 3 VERIFIED ACCOUNTS TO RANK"):"3-4 VERIFIED ACCOUNTS EARN PARTY RANKS");
  if(!party.room){unsigned total=party.roomCount+3;choice=menuMove(choice,total,pressed);
   if(pressed&1){if(choice==0||choice==1)ta_vs_party_host(&party,sharedSpeed,choice);else if(choice==total-1)break;else ta_vs_party_join(&party,party.rooms[choice-2].id);}
   unsigned first=choice>5?choice-5:0;for(unsigned row=0;row<6&&first+row<total;row++){unsigned i=first+row;char label[64];
    if(i<2)strcpy(label,i?"HOST FOCUS PARTY":"HOST AUTO PARTY");else if(i==total-1)strcpy(label,"BACK");else{TaPartyRoom*r=party.rooms+i-2;snprintf(label,sizeof label,"%06lu %s %u/4 %s",(unsigned long)r->id,r->name,(unsigned)r->count,r->mode?"FOCUS":"AUTO");}menuRow(160+row*36,label,choice==i,2);}
   accountFooter("D-PAD MOVE  A SELECT  B BACK",party.haveCookie?"RELAY CONNECTED":"CONNECTING TO PARTY RELAY");
  }else{
   partySpeedInput(pressed);if(ta_lobby_ready_pressed(pressed)){party.flags^=1u;party.requestSent=0;}
   if(ta_lobby_party_can_start(party.phase,party.self,party.host,party.members,party.present,party.ready,party.flags,party.start,party.speedRequest)){party.start=1;party.requestSent=0;}
   char text[64];snprintf(text,sizeof text,"ROOM %06lu  %s  SPEED %u",(unsigned long)party.room,party.mode?"FOCUS":"AUTO",(unsigned)party.speed);ta_vs_lan_text_box(screen,96,158,320,14,text,uiGold,2);partyTouchSpeed();
   for(unsigned s=0;s<4;s++){snprintf(text,sizeof text,"%u %-8s %s%s",s+1,party.members&(1u<<s)?party.names[s]:"OPEN",party.ready&(1u<<s)?"READY":"WAITING",s==party.host?" HOST":"");ta_menu_text(screen,58,185+s*40,text,s==party.self?uiGold:uiText,2);}
   if(party.mode){ta_menu_text(screen,44,345,"HIGHLIGHT WHO RECEIVES YOUR JUNK",uiGold,2);char hint[80];snprintf(hint,sizeof hint,"RIGHT STICK OR %s",controlPhysical(TA_Controls_Get(0,7)));ta_menu_text(screen,44,366,hint,uiMuted,2);}
   for(unsigned seat=0;seat<4;seat++)if(party.speedRequests[seat]){snprintf(text,sizeof text,"SPEED %u?",party.speedRequests[seat]);ta_menu_text(screen,350,202+seat*40,text,uiGold,2);}
   accountReadyFooter((party.flags&1u)?"START UNREADY":"START READY",party.self==party.host?"L/R CHANGE SPEED":"L/R REQUEST SPEED",true);
  }
 }
 partyClose();leavePublic();ta_vs_udp_close(&transport);
 if(error){menuBegin(&pad);for(;;){unsigned pressed=menuRead(&pad);if(pressed&(1u|0x100u))break;char detail[64];snprintf(detail,sizeof detail,"PARTY ERROR %d",error);caption(error==-304?"UPDATE PARTY RELAY TO V1.0.8":error==-10060?"PARTY RELAY UNAVAILABLE":error==-302?"ROOM FULL OR CLOSED":error==-303?"GAME VERSION DOES NOT MATCH":error==-305?"ACCOUNT ALREADY IN A PARTY":error==-306?"PLEASE SIGN IN AGAIN":"PARTY STOPPED",detail);}}
}
