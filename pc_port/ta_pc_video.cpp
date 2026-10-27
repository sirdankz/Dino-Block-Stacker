#include "ta_pc_internal.h"
#include "ta_music.h"
#include "ta_options.h"
extern "C" {
#include "ppu.h"
}
#include "ta_vs_mobile_view.h"
#include "ta_vs_population.h"
#include <algorithm>
#include "ta_pc_frame_clock.h"
#include "ta_pc_renderer_policy.h"
SDL_Window*pcWindow;
SDL_Renderer*pcRenderer;
bool pcMenuFrame=true;
static SDL_Texture *frame,*frames[3],*remote[4],*duel,*labels,*scene,*hud;
static unsigned frameSlot,opponentRevision;
static bool opponentDirty,duelValid,hudValid,hudMenu;
static TaPopulation hudPopulation;
static unsigned hudWidth,hudHeight;
static const uint32_t*opponent;
static const TaPartyVisual*party;
static unsigned have[4],revision[4],lastAt[4],turn,rosterRevision=~0u;
static unsigned long stats[4];
static bool sceneValid;
static int windowWidth=960,windowHeight=720;
static TaPopulation population;
static PcRendererPolicy rendererPolicy;
static bool rendererOverride,recoveryPending,rendererNeedsCommit,saveRendererOnExit;
static int observedWidth,observedHeight;
static SDL_WindowFlags observedFlags;
void pcVideoWindowChanged(){rendererPolicy.settle(SDL_GetTicksNS());}
static std::wstring rendererPath(){return pcWide(std::string(TA_PC_SaveDirectory())+"/renderer.ini");}
struct PcRendererConfig{unsigned mode=0;bool autoOpenGL=false;};
static PcRendererConfig rendererConfig;
static int pendingRenderer=-1;
static unsigned rendererStatus;
static const char*rendererDrivers[]={NULL,"opengl","direct3d11","direct3d12","software"};
static const char*rendererNames[]={"AUTOMATIC","OPENGL","DIRECT3D 11","DIRECT3D 12","SOFTWARE"};
static unsigned encodeRendererConfig(char out[64],const PcRendererConfig&c){return (unsigned)snprintf(out,64,"DINO_RENDERER_V2\nmode=%u\nauto_gl=%u\n",c.mode,c.autoOpenGL?1u:0u);}
static bool loadRendererConfig(){
 auto path=rendererPath();FILE*f=_wfopen(path.c_str(),L"rb");if(!f)return false;
 char data[65]={0};size_t bytes=fread(data,1,sizeof data,f);fclose(f);
 static const char legacy[]="DINO_RENDERER_V1\nopengl\n";
 if(bytes==sizeof legacy-1&&!memcmp(data,legacy,bytes)){rendererConfig={0,true};return true;}
 for(unsigned mode=0;mode<5;mode++)for(unsigned gl=0;gl<2;gl++){
  PcRendererConfig c{mode,gl!=0};char expected[64];unsigned n=encodeRendererConfig(expected,c);
  if(bytes==n&&!memcmp(data,expected,n)){rendererConfig=c;return true;}
 }
 return false;
}
static bool saveRendererPreference(){
 auto path=rendererPath(),temp=path+L".tmp";FILE*f=_wfopen(temp.c_str(),L"wb");if(!f)return false;
 char data[64];unsigned n=encodeRendererConfig(data,rendererConfig);bool ok=fwrite(data,1,n,f)==n;if(fclose(f))ok=false;
 if(ok)ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
 if(!ok)DeleteFileW(temp.c_str());return ok;
}
extern "C" unsigned TA_PC_RendererChoice(void){return pendingRenderer>=0?(unsigned)pendingRenderer:rendererConfig.mode;}
extern "C" const char*TA_PC_RendererName(void){
 if(!pcRenderer)return "UNAVAILABLE";const char*name=SDL_GetRendererName(pcRenderer);
 for(unsigned i=1;i<5;i++)if(!strcmp(name,rendererDrivers[i]))return rendererNames[i];return name;
}
extern "C" unsigned TA_PC_RendererStatus(void){return rendererOverride?5u:rendererStatus;}
extern "C" void TA_PC_SetRenderer(unsigned mode){
 if(mode>=5)return;if(rendererOverride){rendererStatus=5;return;}
 pendingRenderer=(int)mode;recoveryPending=false;rendererStatus=1;
}
static uint32_t backdrop[512*288],hudPixels[512*TA_POPULATION_MAX_HEIGHT];
void pcPace(){
 // A late frame proceeds immediately; missed deadlines never add another frame of sleep.
 static PcFrameClock clock;Uint64 delay=clock.wait(SDL_GetTicksNS());if(delay)SDL_DelayPrecise(delay);
}
struct PcFrameTiming{Uint64 interval,work,upload,render,pace;unsigned game,underruns,overruns,opengl;};
static bool timingEnabled;static Uint64 timingStart,timingEnd;
static std::vector<PcFrameTiming> frameTimings;
void pcSetFrameTimings(bool enabled){timingEnabled=enabled;if(enabled)frameTimings.reserve(18000);}
void pcFrameStart(){if(timingEnabled&&!timingStart)timingStart=SDL_GetTicksNS();}
void pcWriteFrameTimings(){
 if(!timingEnabled||frameTimings.empty())return;
 std::string path=std::string(TA_PC_SaveDirectory())+"/frame-timings.csv";FILE*f=fopen(path.c_str(),"w");if(!f)return;
 fprintf(f,"frame,game,interval_ms,work_ms,upload_ms,render_ms,pace_ms,audio_underruns,audio_overruns,renderer_opengl\n");
 unsigned i=0;for(const auto&t:frameTimings)fprintf(f,"%u,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%u,%u,%u\n",++i,t.game,t.interval/1e6,t.work/1e6,t.upload/1e6,t.render/1e6,t.pace/1e6,t.underruns,t.overruns,t.opengl);
 fclose(f);frameTimings.clear();
}
static SDL_FRect menuRect(){int w=1,h=1;SDL_GetRenderOutputSize(pcRenderer,&w,&h);float width=std::min((float)w,h*4.0f/3.0f),height=width*.75f;return {(w-width)*.5f,(h-height)*.5f,width,height};}
void pcMenuPoint(float wx,float wy,float&x,float&y){float rx,ry;SDL_RenderCoordinatesFromWindow(pcRenderer,wx,wy,&rx,&ry);SDL_FRect r=menuRect();x=(rx-r.x)*512/r.w;y=(ry-r.y)*480/r.h;}
static SDL_Texture*image(SDL_Renderer*renderer,int w,int h,bool alpha=false){auto t=SDL_CreateTexture(renderer,alpha?SDL_PIXELFORMAT_ARGB8888:SDL_PIXELFORMAT_XRGB8888,SDL_TEXTUREACCESS_STREAMING,w,h);if(t){SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(t,alpha?SDL_BLENDMODE_BLEND:SDL_BLENDMODE_NONE);}return t;}
static bool upload(SDL_Texture*t,const uint32_t*p,int stride,int w,int h){if(!t||!p)return false;SDL_Rect r={0,0,w,h};return SDL_UpdateTexture(t,&r,p,stride*4);}
static void draw(SDL_Texture*t,SDL_FRect dest,float x,float y,float w,float h){if(t){SDL_FRect src={x,y,w,h};SDL_RenderTexture(pcRenderer,t,&src,&dest);}}
static SDL_FRect rect(TaPartyRect r){return {r.left,r.top,r.right-r.left,r.bottom-r.top};}
struct PcTextures {
 SDL_Texture*frames[3]={},*remote[4]={},*duel=NULL,*labels=NULL,*scene=NULL,*hud=NULL;
};
static void freeTextures(PcTextures&t){for(auto p:t.frames)SDL_DestroyTexture(p);for(auto p:t.remote)SDL_DestroyTexture(p);SDL_DestroyTexture(t.duel);SDL_DestroyTexture(t.labels);SDL_DestroyTexture(t.scene);SDL_DestroyTexture(t.hud);t=PcTextures{};}
static bool createTextures(SDL_Renderer*r,PcTextures&t){
 for(auto&p:t.frames)p=image(r,512,480);for(auto&p:t.remote)p=image(r,224,224);
 t.duel=image(r,224,224);t.labels=image(r,96,32);t.scene=image(r,512,288);t.hud=image(r,512,TA_POPULATION_MAX_HEIGHT,true);
 bool ok=t.duel&&t.labels&&t.scene&&t.hud;for(auto p:t.frames)ok=ok&&p;for(auto p:t.remote)ok=ok&&p;
 if(!ok)freeTextures(t);return ok;
}
static PcTextures currentTextures(){PcTextures t;memcpy(t.frames,frames,sizeof frames);memcpy(t.remote,remote,sizeof remote);t.duel=duel;t.labels=labels;t.scene=scene;t.hud=hud;return t;}
static void installTextures(const PcTextures&t){memcpy(frames,t.frames,sizeof frames);memcpy(remote,t.remote,sizeof remote);duel=t.duel;labels=t.labels;scene=t.scene;hud=t.hud;frameSlot=0;frame=frames[0];}
static bool switchRenderer(const uint32_t*p,int height,const char*driver){
 int w,h,x,y;SDL_GetWindowSize(pcWindow,&w,&h);SDL_GetWindowPosition(pcWindow,&x,&y);auto flags=SDL_GetWindowFlags(pcWindow);
 if(flags&SDL_WINDOW_FULLSCREEN){w=windowWidth;h=windowHeight;}
 SDL_Window*nextWindow=SDL_CreateWindow("Dino Block Stacker v1.2.9",w,h,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_HIDDEN);
 if(!nextWindow)return false;SDL_SetWindowMinimumSize(nextWindow,640,480);SDL_SetWindowPosition(nextWindow,x,y);
 SDL_Renderer*next=SDL_CreateRenderer(nextWindow,driver);PcTextures textures;
 if(!next){SDL_DestroyWindow(nextWindow);return false;}
 SDL_SetRenderVSync(next,0);
 auto discard=[&](){freeTextures(textures);SDL_DestroyRenderer(next);SDL_DestroyWindow(nextWindow);};
 if(!createTextures(next,textures)){discard();return false;}
 bool ok=true;
 for(auto t:textures.frames)ok=upload(t,p,512,512,height)&&ok;
 if(opponent&&duelValid)ok=upload(textures.duel,opponent,224,224,224)&&ok;
 if(sceneValid)ok=upload(textures.scene,backdrop,512,512,288)&&ok;
 if(hudValid)ok=upload(textures.hud,hudPixels,512,hudWidth,hudHeight)&&ok;
 if(party){
  for(unsigned i=0;i<4;i++)if(have[i]&&i!=party->self&&(party->members&(1u<<i)))ok=upload(textures.remote[i],party->pixels[i],224,224,224)&&ok;
  if(rosterRevision!=~0u)ok=upload(textures.labels,party->roster,512,96,32)&&ok;
 }
 if(!ok){discard();return false;}
 if((flags&SDL_WINDOW_FULLSCREEN)&&!SDL_SetWindowFullscreen(nextWindow,true)){discard();return false;}
 if(flags&SDL_WINDOW_MAXIMIZED)SDL_MaximizeWindow(nextWindow);
 if(!(flags&SDL_WINDOW_HIDDEN)&&!SDL_ShowWindow(nextWindow)){discard();return false;}
 SDL_Window*previousWindow=pcWindow;SDL_Renderer*previous=pcRenderer;PcTextures old=currentTextures();
 pcWindow=nextWindow;pcRenderer=next;installTextures(textures);

 if(flags&SDL_WINDOW_INPUT_FOCUS)SDL_RaiseWindow(pcWindow);
 if(pcTextMode){SDL_StopTextInput(previousWindow);SDL_StartTextInput(pcWindow);}
 freeTextures(old);SDL_DestroyRenderer(previous);SDL_DestroyWindow(previousWindow);
 pcVideoWindowChanged();return true;
}
static void applyRendererRequest(const uint32_t*p,int height,unsigned mode,bool recovering){
 const char*driver=rendererDrivers[mode];if(!mode&&(recovering||rendererConfig.autoOpenGL))driver="opengl";
 bool same=driver?!strcmp(SDL_GetRendererName(pcRenderer),driver):mode==rendererConfig.mode&&!recovering;
 if(!same&&!switchRenderer(p,height,driver)){rendererStatus=3;return;}
 rendererConfig.mode=mode;if(recovering)rendererConfig.autoOpenGL=true;
 rendererNeedsCommit=true;rendererStatus=1;rendererPolicy=PcRendererPolicy{};pcVideoWindowChanged();
}
extern "C" HRESULT TA_Video_Init(void){
 rendererPolicy=PcRendererPolicy{};recoveryPending=rendererNeedsCommit=saveRendererOnExit=false;pendingRenderer=-1;rendererConfig=PcRendererConfig{};rendererStatus=0;observedWidth=observedHeight=0;observedFlags=0;
 sceneValid=hudValid=duelValid=false;opponentDirty=opponent!=NULL;memset(have,0,sizeof have);memset(revision,0,sizeof revision);rosterRevision=~0u;turn=0;
 int w=960,h=720,full=0;std::string path=std::string(TA_PC_SaveDirectory())+"/window.ini";FILE*f=fopen(path.c_str(),"r");if(f){fscanf(f,"%d %d %d",&w,&h,&full);fclose(f);}if(TA_Options_Loaded())full=(int)TA_Options_Get(TA_OPT_FULLSCREEN);else TA_Options_Set(TA_OPT_FULLSCREEN,full?1:0);w=std::clamp(w,640,3840);h=std::clamp(h,480,2160);
 windowWidth=w;windowHeight=h;
 pcWindow=SDL_CreateWindow("Dino Block Stacker v1.2.9",w,h,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);if(!pcWindow)return E_FAIL;SDL_SetWindowMinimumSize(pcWindow,640,480);
 const char*hint=SDL_GetHint(SDL_HINT_RENDER_DRIVER);rendererOverride=hint&&*hint;
 loadRendererConfig();
 const char*driver=rendererConfig.mode?rendererDrivers[rendererConfig.mode]:rendererConfig.autoOpenGL?"opengl":NULL;
 if(!rendererOverride&&driver){pcRenderer=SDL_CreateRenderer(pcWindow,driver);if(!pcRenderer)rendererStatus=6;}
 if(!pcRenderer)pcRenderer=SDL_CreateRenderer(pcWindow,NULL);if(!pcRenderer)return E_FAIL;SDL_SetRenderVSync(pcRenderer,0);if(full)SDL_SetWindowFullscreen(pcWindow,true);
 PcTextures textures;if(!createTextures(pcRenderer,textures))return E_FAIL;installTextures(textures);pcVideoWindowChanged();return S_OK;
}
void pcSaveWindow(){if(!pcWindow)return;int w,h;SDL_GetWindowSize(pcWindow,&w,&h);bool full=(SDL_GetWindowFlags(pcWindow)&SDL_WINDOW_FULLSCREEN)!=0;std::string path=std::string(TA_PC_SaveDirectory())+"/window.ini";if(!full){windowWidth=w;windowHeight=h;}FILE*f=fopen(path.c_str(),"w");if(f){fprintf(f,"%d %d %d\n",windowWidth,windowHeight,full?1:0);fclose(f);}}
extern "C" void TA_PC_ApplyDisplayOptions(unsigned option){
 if(!pcWindow)return;pcVideoWindowChanged();pcSaveWindow();
 if(option==1){static const int sizes[4][2]={{960,720},{1280,720},{1280,960},{1920,1080}};unsigned i=TA_Options_Get(TA_OPT_WINDOW_SIZE);SDL_SetWindowFullscreen(pcWindow,false);SDL_SyncWindow(pcWindow);TA_Options_Set(TA_OPT_FULLSCREEN,0);SDL_SetWindowSize(pcWindow,sizes[i][0],sizes[i][1]);SDL_SyncWindow(pcWindow);}
 else{SDL_SetWindowFullscreen(pcWindow,TA_Options_Get(TA_OPT_FULLSCREEN)!=0);SDL_SyncWindow(pcWindow);TA_Options_Set(TA_OPT_FULLSCREEN,(SDL_GetWindowFlags(pcWindow)&SDL_WINDOW_FULLSCREEN)?1:0);}
 pcSaveWindow();pcVideoWindowChanged();
}
extern "C" void TA_Video_SetPopulation(unsigned a,unsigned g,unsigned o,unsigned k){population.accounts=a;population.ingame=g;population.online=o;population.known=k;}
extern "C" void TA_Video_SetDuelMode(unsigned){}
extern "C" void TA_Video_SetPartyBackdrop(const uint32_t*p){if(p&&!sceneValid){ta_mobile_backdrop(backdrop,512,288,p,512);sceneValid=upload(scene,backdrop,512,512,288);}}
extern "C" bool TA_Video_SetLanOverlay(const uint32_t*p,unsigned revision){if(p!=opponent||revision!=opponentRevision)opponentDirty=true;opponent=p;opponentRevision=revision;if(!p)duelValid=false;return true;}
extern "C" bool TA_Video_SetPartyVisual(const TaPartyVisual*p){if(!party||!p||party->self!=p->self){memset(have,0,sizeof have);memset(revision,0,sizeof revision);rosterRevision=~0u;sceneValid=false;}party=p;return true;}
extern "C" bool TA_Video_SetPartyOverlay(const uint32_t*p,unsigned){if(!p){TA_Video_SetPartyVisual(NULL);return true;}return false;}
extern "C" void TA_Video_PartyStats(unsigned long out[4]){memcpy(out,stats,sizeof stats);memset(stats,0,sizeof stats);}
static void present(const uint32_t*p,int height){
 if(!pcRenderer)return;pcEvents();
 if(pendingRenderer>=0){unsigned mode=(unsigned)pendingRenderer;pendingRenderer=-1;applyRendererRequest(p,height,mode,false);}
 else if(recoveryPending){recoveryPending=false;applyRendererRequest(p,height,0,true);}
 pcMenuFrame=height==480;if(pcMenuFrame)pcMenuCommit();else TA_Menu_Reset();
 Uint64 beforeUpload=SDL_GetTicksNS();
 frameSlot=(frameSlot+1)%3;frame=frames[frameSlot];
 if(!upload(frame,p,512,512,height))throw std::runtime_error(SDL_GetError());
 Uint64 afterUpload=timingEnabled?SDL_GetTicksNS():0;
 SDL_SetRenderDrawColor(pcRenderer,20,40,60,255);SDL_RenderClear(pcRenderer);int w,h;SDL_GetRenderOutputSize(pcRenderer,&w,&h);SDL_FRect view=menuRect();bool multiple=party&&!pcMenuFrame;
 if(multiple){
  if(!sceneValid)TA_Video_SetPartyBackdrop(p);draw(scene,{0,0,(float)w,(float)h},0,0,512,288);
  unsigned peers=0;for(unsigned s=0;s<4;s++)if(s!=party->self&&(party->members&(1u<<s)))peers++;
  if(peers){TaPartyLayout layout=ta_party_layout_ex(w,h,1,peers,1);unsigned now=GetTickCount();
   for(unsigned k=0;k<4;k++){unsigned s=(turn+k)%4;if(s==party->self||!(party->members&(1u<<s)))continue;
    if(ta_party_upload_due(have[s],revision[s],party->revision[s],now,lastAt[s],peers>1?125:66)&&upload(remote[s],party->pixels[s],224,224,224)){have[s]=1;revision[s]=party->revision[s];lastAt[s]=now;turn=(s+1)%4;stats[0]++;stats[3]+=224*224*4;break;}}
   if(rosterRevision!=party->rosterRevision&&upload(labels,party->roster,512,96,32)){rosterRevision=party->rosterRevision;stats[1]++;stats[3]+=96*32*4;}
   draw(frame,rect(layout.local),0,(float)layout.localSourceY,224,(float)(224-layout.localSourceY));draw(labels,rect(layout.label[0]),0,(float)(party->self*8),96,8);
   unsigned index=0;for(unsigned s=0;s<4;s++)if(s!=party->self&&(party->members&(1u<<s))){if(have[s])draw(remote[s],rect(layout.peer[index]),0,0,224,224);draw(labels,rect(layout.label[index+1]),0,(float)(s*8),96,8);index++;}
  }else draw(frame,view,0,0,512,(float)height);
 }else{
  draw(frame,view,0,0,512,(float)height);
  if(opponent&&!pcMenuFrame){if(opponentDirty&&upload(duel,opponent,224,224,224)){opponentDirty=false;duelValid=true;}if(duelValid){SDL_FRect r={view.x+304*view.w/512,view.y+24*view.h/224,192*view.w/512,192*view.h/224};draw(duel,r,16,24,192,192);}}
 }
 if(!hudValid||hudMenu!=pcMenuFrame||memcmp(&population,&hudPopulation,sizeof population)){
  unsigned hw=ta_population_image(hudPixels,512,&population,pcMenuFrame),hh=ta_population_height(pcMenuFrame);
  if(upload(hud,hudPixels,512,hw,hh)){hudValid=true;hudMenu=pcMenuFrame;hudPopulation=population;hudWidth=hw;hudHeight=hh;}
 }
 if(hudValid){unsigned hw=hudWidth,hh=hudHeight;float sx=pcMenuFrame?view.w/512.0f:h/480.0f,sy=pcMenuFrame?view.h/480.0f:sx;SDL_FRect r={pcMenuFrame?view.x+(512-hw)*sx*.5f:w-hw*sx-8,pcMenuFrame?view.y+TA_POPULATION_MENU_TOP*sy:2,hw*sx,hh*sy};draw(hud,r,0,0,(float)hw,(float)hh);}
 bool presented=SDL_RenderPresent(pcRenderer);Uint64 rendered=SDL_GetTicksNS();
 if(presented&&rendererNeedsCommit){rendererNeedsCommit=false;saveRendererOnExit=!saveRendererPreference();rendererStatus=saveRendererOnExit?4u:2u;}
 auto flags=SDL_GetWindowFlags(pcWindow);auto relevant=flags&(SDL_WINDOW_FULLSCREEN|SDL_WINDOW_MINIMIZED|SDL_WINDOW_MAXIMIZED|SDL_WINDOW_INPUT_FOCUS);
 if(w!=observedWidth||h!=observedHeight||relevant!=observedFlags){observedWidth=w;observedHeight=h;observedFlags=relevant;rendererPolicy.settle(rendered);}
 bool eligible=!rendererOverride&&!rendererConfig.mode&&strcmp(SDL_GetRendererName(pcRenderer),"opengl")&&(flags&SDL_WINDOW_INPUT_FOCUS)&&!(flags&(SDL_WINDOW_MINIMIZED|SDL_WINDOW_HIDDEN));
 if(rendererPolicy.sample(rendered,rendered-beforeUpload,eligible))recoveryPending=true;
 pcPace();
 if(timingEnabled){Uint64 end=SDL_GetTicksNS();if(frameTimings.size()<18000){unsigned underruns,overruns;pcAudioStats(underruns,overruns);frameTimings.push_back({timingEnd?end-timingEnd:0,timingStart&&beforeUpload>=timingStart?beforeUpload-timingStart:0,afterUpload-beforeUpload,rendered-afterUpload,end-rendered,pcMenuFrame?0u:1u,underruns,overruns,!strcmp(SDL_GetRendererName(pcRenderer),"opengl")?1u:0u});}timingEnd=end;timingStart=0;}

}
extern "C" void TA_Video_Submit(const unsigned char*p){TA_Music_MenuFrame();present((const uint32_t*)p,480);}
extern "C" bool TA_Video_SubmitActive224(const unsigned char*p){TA_Music_SetScene(TA_MUSIC_GAME);present((const uint32_t*)p,224);return pcRenderer!=NULL;}
extern "C" bool TA_Video_SubmitActive224Direct(const Ppu*p){TA_Music_SetScene(TA_MUSIC_GAME);static uint32_t pixels[512*224];if(!ppu_taCopyActive224(p,(uint8_t*)pixels,sizeof pixels))return false;return TA_Video_SubmitActive224((const unsigned char*)pixels);}
extern "C" bool TA_Video_ActiveAvailable(void){return pcRenderer!=NULL;}
extern "C" bool TA_Video_SubmitNative224Direct(const Ppu*){TA_Music_SetScene(TA_MUSIC_GAME);return false;}
extern "C" void TA_Video_BeginR126(Ppu*){TA_Music_SetScene(TA_MUSIC_GAME);}
extern "C" bool TA_Video_EndR126(Ppu*){return false;}
extern "C" void TA_Video_ReleaseLanPpu(Ppu*){sceneValid=false;}
extern "C" void TA_Video_R126Counts(unsigned long out[4]){memset(out,0,4*sizeof *out);}
extern "C" void TA_Video_ReadReset(unsigned long*a,unsigned long*b,unsigned long*c,unsigned long*d){*a=*b=*c=*d=0;}
extern "C" void TA_Video_ReadResetUploadTicks(uint64_t t[3]){memset(t,0,3*sizeof *t);}
extern "C" void TA_Video_Shutdown(void){pcWriteFrameTimings();if(saveRendererOnExit)saveRendererPreference();auto textures=currentTextures();freeTextures(textures);installTextures(PcTextures{});SDL_DestroyRenderer(pcRenderer);SDL_DestroyWindow(pcWindow);pcRenderer=NULL;pcWindow=NULL;hudValid=duelValid=false;}
