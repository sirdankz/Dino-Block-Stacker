/* N12 connection lifetime is separate from the lifetime of each ROM round. */
#include <stdlib.h>
#ifndef TA_N12_CACHE_DIR
#define TA_N12_CACHE_DIR "E:\\TA_NATIVE_VS"
#endif
static bool bootPreparing;
static struct{Snes*s;unsigned frame,speed,character;uint32_t seed,epoch;}warm;
static void discardWarm(void){if(warm.s)snes_free(warm.s);memset(&warm,0,sizeof warm);}

static uint8_t cacheRomHash[32];
static const uint8_t*launchRom;static unsigned launchBytes;
static void cachePath(char*out,unsigned cap,const char*name){snprintf(out,cap,"%s/%s",TA_N12_CACHE_DIR,name);}
static void setupProgress(const char*detail,unsigned done,unsigned total){
 memset(screen,0,sizeof screen);ta_vs_lan_text(screen,30,65,"FIRST LAUNCH SETUP",0x006fc9ff,4);ta_vs_lan_text(screen,30,130,"ONLY NEEDED ON FIRST LAUNCH",0x00ffffff,3);ta_vs_lan_text(screen,30,190,detail,0x009fb9ce,2);
 unsigned percent=total?done*100/total:100;if(percent>100)percent=100;char text[24];snprintf(text,sizeof text,"%u/100",percent);ta_vs_lan_text(screen,30,252,text,0x006affa8,3);
 for(unsigned y=300;y<320;y++)for(unsigned x=30;x<480;x++)screen[y*512+x]=x<30+percent*450/100?0x006affa8:0x00223d55;
 TA_Video_Submit((const uint8_t*)screen);
}
static TaVsStart*menuStart;static TaVsStart*roundStart;static unsigned roundSpeed,roundCharacter;
static void leavePublic(void){
 if(publicMode&&transport.opened&&publicRoom.haveCookie){
  ta_vs_public_begin_leave(&publicRoom);uint32_t begin=GetTickCount();
  while(!publicRoom.leaveAck&&(uint32_t)(GetTickCount()-begin)<2000){
   uint8_t packet[TA_PUB_BYTES];uint32_t to;uint16_t port;size_t n=ta_vs_public_packet(&publicRoom,GetTickCount(),packet,sizeof packet,&to,&port);if(n)ta_vs_udp_send_to(&transport,to,port,packet,n);
   for(unsigned k=0;k<16;k++){uint32_t from;uint16_t fp;int bytes=ta_vs_udp_receive_any(&transport,packet,sizeof packet,&from,&fp);if(bytes<=0)break;ta_vs_public_receive(&publicRoom,GetTickCount(),from,fp,packet,(size_t)bytes);}
   caption("LEAVING ROOM","CLOSING YOUR SERVER SESSION",true);
  }
  TA_Log("[TA N13 ROOM] leave_ack=%u room=%lu\n",publicRoom.leaveAck,(unsigned long)publicRoom.room);
 }
 publicMode=false;publicRoomClosed=false;
}
static void discardRoundImage(void){
 releaseGame();memset(&pictureTx,0,sizeof pictureTx);memset(&pictureRx,0,sizeof pictureRx);
 captureRevision=0;remoteRevision=lobby.session.remoteRevision;sourceReady=false;lobby.session.snapshotSize=lobby.session.remoteSize=0;lobby.session.snapshotSent=lobby.session.snapshotRevision;lobby.session.applicationRound=roundState.epoch;
}
static bool readRoundTemplate(unsigned speed,unsigned character){
 if(roundStart&&roundSpeed==speed&&roundCharacter==character)return true;
 char name[40],path[160];snprintf(name,sizeof name,"n12-r%u-c%u.bin",speed,character);cachePath(path,sizeof path,name);
 TaVsStart*t=(TaVsStart*)malloc(sizeof *t);if(!t)return false;TaVsPreview temp;
 if(ta_vs_start_read_frame(path,cacheRomHash,2424,t,&temp)){free(t);return false;}
 free(roundStart);roundStart=t;roundSpeed=speed;roundCharacter=character;return true;
}
static void persistRoundTemplate(void){
 if(!roundStart)return;char name[40],path[160];snprintf(name,sizeof name,"n12-r%u-c%u.bin",roundSpeed,roundCharacter);cachePath(path,sizeof path,name);ta_vs_start_write(path,cacheRomHash,roundStart,&previewAtlas);
 cachePath(path,sizeof path,"n12-options.txt");FILE*f=fopen(path,"wb");if(f){fprintf(f,"%u %u\n",roundSpeed,roundCharacter);fclose(f);}
}
static void freeMenuCache(void){discardWarm();free(menuStart);free(roundStart);menuStart=roundStart=NULL;previewAtlas.ready=0;}
static int prepareMenuCache(const uint8_t*rom,unsigned bytes){
 if(menuStart&&previewAtlas.ready)return 0;
 menuStart=(TaVsStart*)malloc(sizeof *menuStart);if(!menuStart)return TA_VS_BAD_ARGUMENT;
 uint8_t hash[32];ta_rom_sha256(rom,bytes,hash);memcpy(cacheRomHash,hash,32);
 char stored[160];cachePath(stored,sizeof stored,"n12-start.bin");
 if(!ta_vs_start_read(stored,hash,menuStart,&previewAtlas)){TA_Log("[TA N13 PREVIEW] ready=1 persistent_cache=1\n");return 0;}
 if(bootPreparing)setupProgress("PREPARING ORIGINAL GAME DATA",0,2);
 if(!ta_vs_start_read("D:\\ta-n11-start.bin",hash,menuStart,&previewAtlas)){
  ta_vs_start_write(stored,hash,menuStart,&previewAtlas);TA_Log("[TA N13 PREVIEW] ready=1 portraits=13 checkpoint_bytes=%lu build_cache=1\n",(unsigned long)menuStart->bytes);return 0;
 }
 Snes*s=snes_init();int error=0;if(!s){freeMenuCache();return TA_VS_BAD_ARGUMENT;}
 if(!snes_loadRom(s,rom,bytes)||!ta_native_init(rom,bytes)){snes_free(s);freeMenuCache();return TA_VS_BAD_ARGUMENT;}
 snes_setPixelFormat(s,pixelFormatXRGB);uint32_t drawn=0;
 for(unsigned f=0;f<=2318;f++){
  uint16_t pads[2];TA_Input_Poll(pads);if(!bootPreparing&&(pads[0]&12)==12){error=TA_VS_CONFLICT;break;}
  if(!bootPreparing&&((error=pump(false))!=0||roundState.peerLeft))break;
  for(unsigned p=0;p<2;p++){unsigned buttons=ta_vs_setup_options(f,p,5,12);if(f==2302||f==2303)buttons=p?0x80:0x40;for(unsigned b=0;b<12;b++)snes_setButtonState(s,p+1,b,(buttons>>b)&1);}
  s->ppu->noPixels=f!=2300&&f!=2318;snes_runFrame(s);snes_setSamples(s,audio,800);
  if(f==1839&&ta_vs_start_save(menuStart,s,f)){error=TA_VS_BAD_ARGUMENT;break;}
  if(f==2300||f==2318){if(!ppu_taCopyActive224(s->ppu,(uint8_t*)capture,sizeof capture)){error=TA_VS_BAD_ARGUMENT;break;}
   if(f==2300)ta_vs_preview_capture(&previewAtlas,capture);else{ta_vs_preview_capture_one(&previewAtlas,0,capture);ta_vs_preview_capture_one(&previewAtlas,12,capture);}}
  uint32_t now=GetTickCount();if(!drawn||(uint32_t)(now-drawn)>80){if(bootPreparing)setupProgress("PREPARING ORIGINAL GAME DATA",f,2318);else caption("PREPARING VERSUS","ONE TIME ROM INITIALIZATION",true);drawn=now;}
 }
 snes_free(s);if(error||(!bootPreparing&&roundState.peerLeft)){freeMenuCache();return error?error:TA_VS_WRONG_SESSION;}
 ta_vs_start_write(stored,hash,menuStart,&previewAtlas);TA_Log("[TA N13 PREVIEW] ready=1 portraits=13 checkpoint_bytes=%lu build_cache=0\n",(unsigned long)menuStart->bytes);return 0;
}
static int prepareAtLaunch(const uint8_t*rom,unsigned bytes){
 launchRom=rom;launchBytes=bytes;bootPreparing=true;CreateDirectoryA(TA_N12_CACHE_DIR,NULL);
 int error=prepareMenuCache(rom,bytes);if(error){bootPreparing=false;return error;}
 char path[160];cachePath(path,sizeof path,"n12-options.txt");FILE*f=fopen(path,"rb");unsigned speed=5,character=0;
 if(f){unsigned a,b;if(fscanf(f,"%u %u",&a,&b)==2&&a>=1&&a<=10&&b<13){speed=a;character=b;}fclose(f);}ownCharacter=character;sharedSpeed=speed;
 bool hit=readRoundTemplate(speed,character);
 if(!hit){
  Snes*s=snes_init();if(!s){bootPreparing=false;return TA_VS_BAD_ARGUMENT;}
  if(!snes_loadRom(s,rom,bytes)||!ta_native_init(rom,bytes)||ta_vs_start_load(menuStart,s)){snes_free(s);bootPreparing=false;return TA_VS_BAD_ARGUMENT;}
  snes_setPixelFormat(s,pixelFormatXRGB);uint32_t drawn=0;
  for(unsigned frame=1840;frame<2425;frame++){
   uint16_t pads[2];TA_Input_Poll(pads);
   for(unsigned p=0;p<2;p++){unsigned buttons=ta_vs_setup_options(frame,p,speed,character);for(unsigned b=0;b<12;b++)snes_setButtonState(s,p+1,b,(buttons>>b)&1);}
   s->ppu->noPixels=true;snes_runFrame(s);snes_setSamples(s,audio,800);uint32_t now=GetTickCount();if(!drawn||(uint32_t)(now-drawn)>=80){setupProgress("CACHING CHARACTER AND SPEED",frame-1840,585);drawn=now;}
  }
  if(!roundStart)roundStart=(TaVsStart*)malloc(sizeof *roundStart);
  if(roundStart&&!ta_vs_start_save(roundStart,s,2424)){roundSpeed=speed;roundCharacter=character;persistRoundTemplate();}
  snes_free(s);
 }
 bootPreparing=false;TA_Log("[TA N13 BOOT] prepared=1 previous_install=%u character=%u speed=%u\n",(unsigned)hit,character,speed);return 0;
}
static int chooseRoundOptions(const uint8_t*rom,unsigned bytes,int&error){
 roundState.phase=TA_ROUND_SELECT;if((error=prepareMenuCache(rom,bytes))!=0)return -1;
 uint16_t pads[2];TA_Input_Poll(pads);uint16_t previous=pads[0];bool ready=false,sent=false;
 TA_Log("[TA N13 SELECT] round=%lu preview_ready=%u\n",(unsigned long)roundState.epoch,previewAtlas.ready);
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
 roundState.phase=TA_ROUND_LOADING;uint32_t begun=GetTickCount();unsigned simulated=0;
 uint32_t seed=ta_vs_round_seed(lobby.identity.seed,roundState.epoch);bool preloaded=warm.s&&warm.epoch==roundState.epoch&&warm.seed==seed&&warm.speed==sharedSpeed&&warm.character==ownCharacter;
 unsigned nextFrame=2425,preloadedFrames=0;Snes*s;bool hit;
 if(preloaded){s=warm.s;warm.s=NULL;nextFrame=warm.frame;preloadedFrames=nextFrame-2425;hit=true;memset(&warm,0,sizeof warm);}
 else{discardWarm();s=snes_init();if(!s){error=TA_VS_BAD_ARGUMENT;return -1;}game.s=s;
  if(!snes_loadRom(s,rom,bytes)||!ta_native_init(rom,bytes)){error=TA_VS_BAD_ARGUMENT;return -1;}snes_setPixelFormat(s,pixelFormatXRGB);
  hit=readRoundTemplate(sharedSpeed,ownCharacter);if(ta_vs_start_load(hit?roundStart:menuStart,s)){error=TA_VS_BAD_ARGUMENT;return -1;}}
 game.s=s;
 uint32_t lastDraw=0;
 for(unsigned frame=hit?2425u:1840u;frame<2425;frame++){
  uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12)==12){error=sendControl(7);return error?-1:0;}
  if((error=pump(false))!=0)return -1;if(roundState.peerLeft)return 0;
  for(unsigned p=0;p<2;p++){unsigned buttons=ta_vs_setup_options(frame,p,sharedSpeed,ownCharacter);for(unsigned b=0;b<12;b++)snes_setButtonState(s,p+1,b,(buttons>>b)&1);}
  s->ppu->noPixels=true;snes_runFrame(s);snes_setSamples(s,audio,800);simulated++;
  uint32_t now=GetTickCount();if(!lastDraw||(uint32_t)(now-lastDraw)>=80){caption("GET READY","SETTING UP YOUR CHARACTER",true);lastDraw=now;}
 }
 if(!hit){if(!roundStart)roundStart=(TaVsStart*)malloc(sizeof *roundStart);if(roundStart){if(ta_vs_start_save(roundStart,s,2424)){free(roundStart);roundStart=NULL;}else{roundSpeed=sharedSpeed;roundCharacter=ownCharacter;persistRoundTemplate();}}}
 if(!preloaded&&ta_vs_start_seed(s,seed)){error=TA_VS_CONFLICT;return -1;}
 /* Output suppression retains sprite evaluation and every guest CPU/APU/DMA
    clock. Only the final, actually presented image needs color composition. */
 for(unsigned frame=nextFrame;frame<TA_VS_OPTIONS_SETUP_FRAMES;frame++){
  uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12)==12){error=sendControl(7);return error?-1:0;}
  if((error=pump(false))!=0)return -1;if(roundState.peerLeft)return 0;
  for(unsigned p=0;p<2;p++)for(unsigned b=0;b<12;b++)snes_setButtonState(s,p+1,b,false);
  s->ppu->noPixels=frame+1<TA_VS_OPTIONS_SETUP_FRAMES;snes_runFrame(s);snes_setSamples(s,audio,800);simulated++;
  uint32_t now=GetTickCount();if(!lastDraw||(uint32_t)(now-lastDraw)>=80){caption("GET READY","PREPARING A FRESH BOARD",true);lastDraw=now;}
 }
 s->ppu->noPixels=false;
 if(word(s->ram,0x2a8)!=2||s->ppu->mode!=2||word(s->ram,0x2ac)!=sharedSpeed||word(s->ram,0x2ae)!=sharedSpeed||word(s->ram,0x2ba)!=ta_vs_character_stage(ownCharacter)){error=TA_VS_CONFLICT;return -1;}
 unsigned boardHash=2166136261u;for(unsigned k=0;k<4;k++)for(unsigned z=0;z<256;z++)boardHash=(boardHash^s->ram[0xd7c+k*512+z])*16777619u;
 ta_vs_local_init(&game,s,&lobby.session,seed);ta_vs_local_bind(&game);
 if((error=sendControl(1))!=0)return -1;
 while(!peerLoaded||ta_vs_pending(&lobby.session)){
  uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12)==12){error=sendControl(7);return error?-1:0;}
  if((error=pump(false))!=0)return -1;if(roundState.peerLeft)return 0;caption("GET READY","WAITING FOR PEER",true);
 }
 TA_Log("[TA N13 START] round=%lu seed=%lu board_hash=%08X template_hit=%u setup_frames=%u wall_ms=%lu preloaded_frames=%u\n",(unsigned long)roundState.epoch,(unsigned long)seed,boardHash,(unsigned)hit,simulated,(unsigned long)(GetTickCount()-begun),preloadedFrames);
 return 1;
}
static void prepareNextRound(unsigned budget){
 if(roundState.localOptions||roundState.peerOptions||roundState.epoch==UINT32_MAX||!roundStart||roundSpeed!=sharedSpeed||roundCharacter!=ownCharacter)return;
 if(!warm.s){
  warm.s=snes_init();if(!warm.s)return;
  if(!snes_loadRom(warm.s,launchRom,launchBytes)||!ta_native_init(launchRom,launchBytes)||ta_vs_start_load(roundStart,warm.s)){discardWarm();return;}
  snes_setPixelFormat(warm.s,pixelFormatXRGB);warm.epoch=roundState.epoch+1;warm.seed=ta_vs_round_seed(lobby.identity.seed,warm.epoch);warm.speed=sharedSpeed;warm.character=ownCharacter;warm.frame=2425;
  if(ta_vs_start_seed(warm.s,warm.seed)){discardWarm();return;}
 }
 for(unsigned k=0;k<budget&&warm.frame<TA_VS_OPTIONS_SETUP_FRAMES;k++,warm.frame++){
  for(unsigned p=1;p<=2;p++)for(unsigned b=0;b<12;b++)snes_setButtonState(warm.s,p,b,false);
  warm.s->ppu->noPixels=warm.frame+1<TA_VS_OPTIONS_SETUP_FRAMES;snes_runFrame(warm.s);snes_setSamples(warm.s,audio,800);
 }
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
 TA_Log("[TA N13 MATCH] running role=%u level=%u round=%lu seed=%lu local=P0 opponent=display_only\n",(unsigned)lobby.role,sharedSpeed,(unsigned long)roundState.epoch,(unsigned long)game.seed);
 TA_Log("[TA N13 OPTIONS] speed=%u character=%u peer_character=%u transport=%s\n",sharedSpeed,ownCharacter,peerCharacter,publicMode?"PUBLIC":"LAN");
#if !defined(TA_NO_LOG) || !TA_NO_LOG
 memset(coarse,0,sizeof coarse);
#endif
  unsigned gpuFrames=0,rgbFrames=0;
  unsigned frames=0;bool lost=false,quit=false,ended=false,backRequested=false;uint32_t backAt=0;uint32_t begun=GetTickCount(),lastLog=begun;unsigned lastFrames=0;unsigned lastUpdates=~0u,lastSeconds=~0u;
  uint16_t previous=0;int result=0;bool sentLeave=false,oldBoardReleased=false;
  for(;;){
   uint16_t pads[2];TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];++streamTick;
   if((error=pump(true))!=0)break;
   if(roundState.next){result=roundState.nextOptions?2:1;break;}
   if(roundState.peerLeft)break;
   if(peerLost||peerQuit)ended=true;
   if(ended){
    if(!oldBoardReleased){ta_vs_local_bind(NULL);game.active=0;sourceReady=false;TA_Video_SetLanOverlay(NULL,0);if(game.s){TA_Video_ReleaseLanPpu(game.s->ppu);snes_free(game.s);game.s=NULL;}oldBoardReleased=true;}
    prepareNextRound(4);
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
 while(prepareAtLaunch(rom,bytes)){freeMenuCache();for(;;){uint16_t pads[2];TA_Input_Poll(pads);if(pads[0]&(1u<<8))break;caption("SETUP FAILED","B RETRY SETUP");}}
 TA_Log("[TA N13 LAN] boot identity=N13_PERSISTENT_ROOMS_REMATCH_PREVIEWS local_controls=immediate rollback=0 guest_timing_guards=original\n");
 for(;;){
  releaseGame();leavePublic();ta_vs_udp_close(&transport);memset(&lobby,0,sizeof lobby);ta_vs_round_init(&roundState);
  peerReady=peerLoaded=peerLost=peerQuit=remoteRevision=streamTick=captureRevision=0;peerSettings=0;sourceReady=false;
  unsigned state=TA_LAN_SELECT,oldState=~0u;uint32_t localIp=0;uint16_t previous=0;int error=0;
  for(;;){
   uint16_t pads[2];TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];
   if(pressed&8){leavePublic();ta_vs_udp_close(&transport);freeMenuCache();return;}
   if(state!=TA_LAN_SELECT&&(pressed&(1u<<8)))break;
   if(state==TA_LAN_SELECT&&(pressed&(1u<<9))){error=browsePublic(rom,bytes,&localIp);if(error==1){error=0;break;}state=error?TA_LAN_ERROR:TA_LAN_ADDRESS;}
   if(state==TA_LAN_SELECT&&(pressed&3)){error=beginLobby(rom,bytes,(pressed&1)?0:1,false);state=error?TA_LAN_ERROR:TA_LAN_ADDRESS;}
   if(state!=TA_LAN_SELECT&&state!=TA_LAN_ERROR){XNADDR address;DWORD flags=XNetGetTitleXnAddr(&address);
    if(flags&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_AUTO|XNET_GET_XNADDR_PPPOE)){localIp=address.ina.s_addr;if(publicMode)publicRoom.localAddress=localIp;error=pump(false);state=error?TA_LAN_ERROR:ta_vs_lan_ready(&lobby)?TA_LAN_CONNECTED:lobby.pinned?TA_LAN_HANDSHAKE:lobby.role?TA_LAN_JOIN_WAIT:TA_LAN_HOST_WAIT;}
   }
   if(state!=oldState){TA_Log("[TA N13 LAN] state=%u role=%u rejected=%u error=%d\n",state,(unsigned)lobby.role,lobby.rejected,error);oldState=state;}
   if(state==TA_LAN_CONNECTED)break;
   ta_vs_lan_render(screen,state,localIp,lobby.pinned?lobby.peerAddress:0,peerReady,error);if(state==TA_LAN_SELECT)ta_vs_lan_text(screen,30,290,"Y PUBLIC ROOMS",0x006affa8,3);TA_Video_Submit((const uint8_t*)screen);
  }
  if(state!=TA_LAN_CONNECTED||error)continue;
  lobby.session.applicationRound=roundState.epoch;
  int ready=chooseRoundOptions(rom,bytes,error);
  while(ready>0&&!error){
   discardRoundImage();int loaded=loadRomRound(rom,bytes,error);if(loaded<=0){TA_Log("[TA N13 LOAD] stopped round=%lu result=%d error=%d peer_left=%u\n",(unsigned long)roundState.epoch,loaded,error,roundState.peerLeft);break;}
   int next=playRomRound(error);releaseGame();if(next<=0||error)break;
   if(ta_vs_round_advance(&roundState)){error=TA_VS_CONFLICT;break;}
   lobby.session.applicationRound=roundState.epoch;peerReady=peerLoaded=peerLost=peerQuit=0;
   TA_Log("[TA N13 ROUND] epoch=%lu keep_room=1 change_options=%u\n",(unsigned long)roundState.epoch,(unsigned)(next==2));
   ready=next==2?chooseRoundOptions(rom,bytes,error):1;
  }
  releaseGame();discardWarm();if(!error)drainRoundControls();leavePublic();ta_vs_udp_close(&transport);
  if(error){for(;;){uint16_t pads[2];TA_Input_Poll(pads);if(pads[0]&(1u<<8))break;caption(error==-10060?"CONNECTION LOST":"MATCH STOPPED","B BACK TO CONNECTION MENU");}}
 }
}
