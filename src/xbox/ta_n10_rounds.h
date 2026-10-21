/* N10 connection lifetime is separate from the lifetime of each ROM round. */
#include <stdlib.h>
static void leavePublic(void){
 if(publicMode&&transport.opened&&publicRoom.haveCookie){
  ta_vs_public_begin_leave(&publicRoom);uint32_t begin=GetTickCount();
  while(!publicRoom.leaveAck&&(uint32_t)(GetTickCount()-begin)<2000){
   uint8_t packet[TA_PUB_BYTES];uint32_t to;uint16_t port;size_t n=ta_vs_public_packet(&publicRoom,GetTickCount(),packet,sizeof packet,&to,&port);if(n)ta_vs_udp_send_to(&transport,to,port,packet,n);
   for(unsigned k=0;k<16;k++){uint32_t from;uint16_t fp;int bytes=ta_vs_udp_receive_any(&transport,packet,sizeof packet,&from,&fp);if(bytes<=0)break;ta_vs_public_receive(&publicRoom,GetTickCount(),from,fp,packet,(size_t)bytes);}
   caption("LEAVING ROOM","CLOSING YOUR SERVER SESSION",true);
  }
  TA_Log("[TA N10 ROOM] leave_ack=%u room=%lu\n",publicRoom.leaveAck,(unsigned long)publicRoom.room);
 }
 publicMode=false;publicRoomClosed=false;
}
static void discardRoundImage(void){
 releaseGame();memset(&pictureTx,0,sizeof pictureTx);memset(&pictureRx,0,sizeof pictureRx);
 captureRevision=0;remoteRevision=lobby.session.remoteRevision;sourceReady=false;lobby.session.snapshotSize=lobby.session.remoteSize=0;lobby.session.snapshotSent=lobby.session.snapshotRevision;lobby.session.applicationRound=roundState.epoch;
}
static void freeMenuCache(void){free(menuCheckpoint);menuCheckpoint=NULL;menuCheckpointSize=0;previewAtlas.ready=0;}
static int prepareMenuCache(const uint8_t*rom,unsigned bytes){
 if(menuCheckpoint&&previewAtlas.ready)return 0;
 Snes*s=snes_init();if(!s)return TA_VS_BAD_ARGUMENT;
 if(!snes_loadRom(s,rom,bytes)||!ta_native_init(rom,bytes)){snes_free(s);return TA_VS_BAD_ARGUMENT;}
 snes_setPixelFormat(s,pixelFormatXRGB);int error=0;
 menuCheckpoint=(uint8_t*)malloc(2097152);if(!menuCheckpoint){snes_free(s);return TA_VS_BAD_ARGUMENT;}
 for(unsigned f=0;f<=2300;f++){
  uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12)==12){error=TA_VS_CONFLICT;break;}
  if((error=pump(false))!=0||roundState.peerLeft)break;
  for(unsigned p=0;p<2;p++){unsigned buttons=ta_vs_setup_options(f,p,5,12);for(unsigned b=0;b<12;b++)snes_setButtonState(s,p+1,b,(buttons>>b)&1);}
  snes_runFrame(s);snes_setSamples(s,audio,800);
  if(f==1839){
#if defined(XBOX_PORT)
   menuApuMasterPending=s->apuMasterPending;menuApuCycleDebtNumerator=s->apuCycleDebtNumerator;
#endif
   menuCheckpointSize=snes_saveStateFast(s,menuCheckpoint,2097152);if(menuCheckpointSize<=0){error=TA_VS_BAD_ARGUMENT;break;}}
  if(f%120==0){char progress[80];snprintf(progress,sizeof progress,"CACHING ORIGINAL PORTRAITS %u/100",f*100/2300);caption("PREPARING CHARACTER PREVIEWS",progress,true);}
 }
 if(!error&&!roundState.peerLeft){if(!ppu_taCopyActive224(s->ppu,(uint8_t*)capture,sizeof capture)||ta_vs_preview_capture(&previewAtlas,capture))error=TA_VS_BAD_ARGUMENT;}
 snes_free(s);
 if(error||roundState.peerLeft){freeMenuCache();return error?error:TA_VS_WRONG_SESSION;}
 uint8_t*smaller=(uint8_t*)realloc(menuCheckpoint,(size_t)menuCheckpointSize);if(smaller)menuCheckpoint=smaller;
 TA_Log("[TA N10 PREVIEW] ready=1 portraits=13 checkpoint_bytes=%d ROM_generated=1\n",menuCheckpointSize);return 0;
}
static int chooseRoundOptions(const uint8_t*rom,unsigned bytes,int&error){
 roundState.phase=TA_ROUND_SELECT;if((error=prepareMenuCache(rom,bytes))!=0)return -1;
 uint16_t pads[2];TA_Input_Poll(pads);uint16_t previous=pads[0];bool ready=false,sent=false;
 TA_Log("[TA N10 SELECT] round=%lu preview_ready=%u\n",(unsigned long)roundState.epoch,previewAtlas.ready);
 for(;;){
  TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];if((error=pump(false))!=0)return -1;if(roundState.peerLeft)return 0;
  if(pressed&(1u<<8)){error=sendControl(7);return error?-1:0;}
  if(!ready){bool changed=false;
   if(pressed&0x40){ownCharacter=(ownCharacter+12)%13;changed=true;}if(pressed&0x80){ownCharacter=(ownCharacter+1)%13;changed=true;}
   if(!lobby.role){if((pressed&0x10)&&sharedSpeed<10){sharedSpeed++;changed=true;}if((pressed&0x20)&&sharedSpeed>1){sharedSpeed--;changed=true;}}
   if(publicMode&&!lobby.role){publicRoom.speed=(uint8_t)sharedSpeed;if(changed)publicRoom.kept=0;}
   if(!sent||changed){if((error=sendControl(4))!=0)return -1;sent=true;}
   if(pressed&1){if((error=sendControl(0))!=0)return -1;ready=true;}
  }
  if(ready&&peerReady)return 1;
  renderOptions(ready);TA_Video_Submit((const uint8_t*)screen);
 }
}
static int loadRomRound(const uint8_t*rom,unsigned bytes,int&error){
 roundState.phase=TA_ROUND_LOADING;
 Snes*s=snes_init();if(!s){error=TA_VS_BAD_ARGUMENT;return -1;}
 game.s=s;
 if(!snes_loadRom(s,rom,bytes)||!ta_native_init(rom,bytes)){error=TA_VS_BAD_ARGUMENT;return -1;}
 snes_setPixelFormat(s,pixelFormatXRGB);
 if(!snes_loadStateFast(s,menuCheckpoint,menuCheckpointSize)){error=TA_VS_BAD_ARGUMENT;return -1;}
#if defined(XBOX_PORT)
 /* The generic state format predates exact Xbox rational APU timing. Preserve
    its fractional phase alongside this process-local menu checkpoint. */
 s->apuMasterPending=menuApuMasterPending;s->apuCycleDebtNumerator=menuApuCycleDebtNumerator;
#endif
 for(unsigned frame=1840;frame<TA_VS_OPTIONS_SETUP_FRAMES;frame++){
  uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12)==12){error=sendControl(7);return error?-1:0;}
  if((error=pump(false))!=0)return -1;if(roundState.peerLeft)return 0;
  for(unsigned p=0;p<2;p++){unsigned buttons=ta_vs_setup_options(frame,p,sharedSpeed,ownCharacter);for(unsigned b=0;b<12;b++)snes_setButtonState(s,p+1,b,(buttons>>b)&1);}
  snes_runFrame(s);snes_setSamples(s,audio,800);
  if(frame%120==0){char progress[80];snprintf(progress,sizeof progress,"ROUND %lu  LOADING %u/100",(unsigned long)roundState.epoch,(frame-1840)*100/(TA_VS_OPTIONS_SETUP_FRAMES-1840));caption("LOADING VERSUS",progress,true);}
 }
 if(word(s->ram,0x2a8)!=2||s->ppu->mode!=2||word(s->ram,0x2ac)!=sharedSpeed||word(s->ram,0x2ae)!=sharedSpeed||word(s->ram,0x2ba)!=ta_vs_character_stage(ownCharacter)){error=TA_VS_CONFLICT;return -1;}
 uint32_t seed=ta_vs_round_seed(lobby.identity.seed,roundState.epoch);ta_vs_local_init(&game,s,&lobby.session,seed);ta_vs_local_bind(&game);
 if((error=sendControl(1))!=0)return -1;uint32_t startAt=0;
 while(!peerLoaded||!startAt||(int32_t)(GetTickCount()-startAt)<0){
  uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12)==12){error=sendControl(7);return error?-1:0;}
  if((error=pump(false))!=0)return -1;if(roundState.peerLeft)return 0;
  if(peerLoaded&&!startAt)startAt=GetTickCount()+500;caption(peerLoaded?"GET READY":"WAITING FOR PEER","LOCAL CONTROLS START WITH MATCH",true);
 }
 return 1;
}
static void showRoundResult(bool lost){
 TA_Video_SetLanOverlay(NULL,0);memset(screen,0,sizeof screen);
 ta_vs_lan_text(screen,32,64,"TETRIS ATTACK VERSUS",0x006fc9ff,3);
 ta_vs_lan_text(screen,32,138,lost?(peerLost?"DRAW":"YOU LOST"):"YOU WIN",0x00ffffff,5);
 ta_vs_lan_text(screen,32,238,"A REMATCH",0x006affa8,3);
 ta_vs_lan_text(screen,32,286,"X CHANGE CHARACTER AND SPEED",0x009fb9ce,2);
 ta_vs_lan_text(screen,32,330,"B LEAVE ROOM",0x009fb9ce,2);
 ta_vs_lan_text(screen,32,390,roundState.localVote?"REMATCH REQUESTED - WAITING":roundState.peerVote?"PEER WANTS ANOTHER ROUND":"ROOM STAYS CONNECTED",0x00ffffff,2);
 TA_Video_Submit((const uint8_t*)screen);
}
static void drainRoundControls(void){uint32_t begin=GetTickCount();while(ta_vs_pending(&lobby.session)&&!roundState.peerLeft&&(uint32_t)(GetTickCount()-begin)<1500){if(pump(false))break;caption("LEAVING MATCH","WAITING FOR PEER ACKNOWLEDGEMENT",true);}}
static int playRomRound(int&error){
 roundState.phase=TA_ROUND_PLAY;Snes*s=game.s;
 TA_Log("[TA N10 MATCH] running role=%u level=%u round=%lu seed=%lu local=P0 opponent=display_only\n",(unsigned)lobby.role,sharedSpeed,(unsigned long)roundState.epoch,(unsigned long)game.seed);
 TA_Log("[TA N10 OPTIONS] speed=%u character=%u peer_character=%u transport=%s\n",sharedSpeed,ownCharacter,peerCharacter,publicMode?"PUBLIC":"LAN");
#if !defined(TA_NO_LOG) || !TA_NO_LOG
 memset(coarse,0,sizeof coarse);
#endif
  unsigned gpuFrames=0,rgbFrames=0;
  unsigned frames=0;bool lost=false,quit=false,ended=false,backRequested=false;uint32_t backAt=0;uint32_t begun=GetTickCount(),lastLog=begun;unsigned lastFrames=0;unsigned lastUpdates=~0u,lastSeconds=~0u;
  uint16_t previous=0;int result=0;bool sentLeave=false;
  for(;;){
   uint16_t pads[2];TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];++streamTick;
   if((error=pump(true))!=0)break;
   if(roundState.next){result=roundState.nextOptions?2:1;break;}
   if(roundState.peerLeft)break;
   if(peerLost||peerQuit)ended=true;
   if(ended){
    roundState.phase=TA_ROUND_RESULT;game.active=0;ta_vs_local_bind(NULL);sourceReady=false;
    if(pressed&(1u<<8)){error=sendControl(7);sentLeave=!error;break;}
    if(!roundState.localVote&&(pressed&3)){unsigned options=(pressed&2)!=0;error=sendControl(options?8:5);if(error)break;ta_vs_round_local_vote(&roundState,options);}
    unsigned offer=ta_vs_round_offer(&roundState,lobby.role);if(offer){error=sendControl(offer);if(error)break;roundState.next=roundState.epoch+1;roundState.nextOptions=(uint8_t)(offer==9);}
    if(roundState.next){result=roundState.nextOptions?2:1;break;}
   }
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
    if(game.ticks&&word(s->ram,0x2a0)==5){lost=true;ended=true;roundState.phase=TA_ROUND_RESULT;if((error=sendControl(2))!=0)break;}
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
    roundState.phase=TA_ROUND_RESULT;sourceReady=false;game.active=0;ta_vs_local_bind(NULL);
#if defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
    TA_Video_SetLanOverlay(NULL,0);
    showRoundResult(lost);
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
  if(sentLeave)drainRoundControls();
  return error?-1:result;
}

extern "C" void TA_LanGame_Run(const uint8_t*rom,unsigned bytes){
 TA_Log("[TA N10 LAN] boot identity=N10_PERSISTENT_ROOMS_REMATCH_PREVIEWS local_controls=immediate rollback=0 guest_timing_guards=original\n");
 for(;;){
  releaseGame();leavePublic();ta_vs_udp_close(&transport);memset(&lobby,0,sizeof lobby);ta_vs_round_init(&roundState);
  peerReady=peerLoaded=peerLost=peerQuit=remoteRevision=streamTick=captureRevision=0;peerSettings=0;sourceReady=false;
  unsigned state=TA_LAN_SELECT,oldState=~0u;uint32_t localIp=0;uint16_t previous=0;int error=0;
  for(;;){
   uint16_t pads[2];TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];
   if(pressed&(1u<<9)){leavePublic();ta_vs_udp_close(&transport);freeMenuCache();return;}
   if(state!=TA_LAN_SELECT&&(pressed&(1u<<8)))break;
   if(state==TA_LAN_SELECT&&(pressed&8)){error=browsePublic(rom,bytes,&localIp);if(error==1){error=0;break;}state=error?TA_LAN_ERROR:TA_LAN_ADDRESS;}
   if(state==TA_LAN_SELECT&&(pressed&3)){error=beginLobby(rom,bytes,(pressed&1)?0:1,false);state=error?TA_LAN_ERROR:TA_LAN_ADDRESS;}
   if(state!=TA_LAN_SELECT&&state!=TA_LAN_ERROR){XNADDR address;DWORD flags=XNetGetTitleXnAddr(&address);
    if(flags&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_AUTO|XNET_GET_XNADDR_PPPOE)){localIp=address.ina.s_addr;if(publicMode)publicRoom.localAddress=localIp;error=pump(false);state=error?TA_LAN_ERROR:ta_vs_lan_ready(&lobby)?TA_LAN_CONNECTED:lobby.pinned?TA_LAN_HANDSHAKE:lobby.role?TA_LAN_JOIN_WAIT:TA_LAN_HOST_WAIT;}
   }
   if(state!=oldState){TA_Log("[TA N10 LAN] state=%u role=%u rejected=%u error=%d\n",state,(unsigned)lobby.role,lobby.rejected,error);oldState=state;}
   if(state==TA_LAN_CONNECTED)break;
   ta_vs_lan_render(screen,state,localIp,lobby.pinned?lobby.peerAddress:0,peerReady,error);if(state==TA_LAN_SELECT)ta_vs_lan_text(screen,30,290,"START PUBLIC ROOMS",0x006affa8,3);TA_Video_Submit((const uint8_t*)screen);
  }
  if(state!=TA_LAN_CONNECTED||error)continue;
  lobby.session.applicationRound=roundState.epoch;
  int ready=chooseRoundOptions(rom,bytes,error);
  while(ready>0&&!error){
   discardRoundImage();int loaded=loadRomRound(rom,bytes,error);if(loaded<=0){TA_Log("[TA N10 LOAD] stopped round=%lu result=%d error=%d peer_left=%u\n",(unsigned long)roundState.epoch,loaded,error,roundState.peerLeft);break;}
   int next=playRomRound(error);releaseGame();if(next<=0||error)break;
   if(ta_vs_round_advance(&roundState)){error=TA_VS_CONFLICT;break;}
   lobby.session.applicationRound=roundState.epoch;peerReady=peerLoaded=peerLost=peerQuit=0;
   TA_Log("[TA N10 ROUND] epoch=%lu keep_room=1 change_options=%u\n",(unsigned long)roundState.epoch,(unsigned)(next==2));
   ready=next==2?chooseRoundOptions(rom,bytes,error):1;
  }
  releaseGame();if(!error)drainRoundControls();leavePublic();ta_vs_udp_close(&transport);
  if(error){for(;;){uint16_t pads[2];TA_Input_Poll(pads);if(pads[0]&(1u<<8))break;caption(error==-10060?"CONNECTION LOST":"MATCH STOPPED","B BACK TO CONNECTION MENU");}}
 }
}
