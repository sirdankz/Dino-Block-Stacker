#include "ta_pc_internal.h"
extern "C" {
#include "ppu.h"
}
#include "ta_vs_mobile_view.h"
#include "ta_vs_population.h"
#include <chrono>
#include <thread>
#include <algorithm>
SDL_Window*pcWindow;
SDL_Renderer*pcRenderer;
bool pcMenuFrame=true;
static SDL_Texture *frame,*remote[4],*duel,*labels,*scene,*hud;
static const uint32_t*opponent;
static const TaPartyVisual*party;
static unsigned have[4],revision[4],lastAt[4],turn,rosterRevision=~0u;
static unsigned long stats[4];
static bool sceneValid;
static TaPopulation population;
static uint32_t backdrop[512*288],hudPixels[512*TA_POPULATION_MAX_HEIGHT];
using Clock=std::chrono::steady_clock;
void pcPace(){static auto deadline=Clock::now();constexpr auto step=std::chrono::nanoseconds(16666667);auto now=Clock::now();if(now>deadline+step*3)deadline=now;deadline+=step;std::this_thread::sleep_until(deadline);}
static SDL_FRect menuRect(){int w=1,h=1;SDL_GetRenderOutputSize(pcRenderer,&w,&h);float width=std::min((float)w,h*4.0f/3.0f),height=width*.75f;return {(w-width)*.5f,(h-height)*.5f,width,height};}
void pcMenuPoint(float wx,float wy,float&x,float&y){float rx,ry;SDL_RenderCoordinatesFromWindow(pcRenderer,wx,wy,&rx,&ry);SDL_FRect r=menuRect();x=(rx-r.x)*512/r.w;y=(ry-r.y)*480/r.h;}
static SDL_Texture*image(int w,int h,bool alpha=false){auto t=SDL_CreateTexture(pcRenderer,alpha?SDL_PIXELFORMAT_ARGB8888:SDL_PIXELFORMAT_XRGB8888,SDL_TEXTUREACCESS_STREAMING,w,h);if(t){SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(t,alpha?SDL_BLENDMODE_BLEND:SDL_BLENDMODE_NONE);}return t;}
static bool upload(SDL_Texture*t,const uint32_t*p,int stride,int w,int h){if(!t||!p)return false;SDL_Rect r={0,0,w,h};return SDL_UpdateTexture(t,&r,p,stride*4);}
static void draw(SDL_Texture*t,SDL_FRect dest,float x,float y,float w,float h){if(t){SDL_FRect src={x,y,w,h};SDL_RenderTexture(pcRenderer,t,&src,&dest);}}
static SDL_FRect rect(TaPartyRect r){return {r.left,r.top,r.right-r.left,r.bottom-r.top};}
extern "C" HRESULT TA_Video_Init(void){
 int w=960,h=720,full=0;std::string path=std::string(TA_PC_SaveDirectory())+"/window.ini";FILE*f=fopen(path.c_str(),"r");if(f){fscanf(f,"%d %d %d",&w,&h,&full);fclose(f);}w=std::clamp(w,640,3840);h=std::clamp(h,480,2160);
 pcWindow=SDL_CreateWindow("Dino Block Stacker v1.1.8",w,h,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);if(!pcWindow)return E_FAIL;SDL_SetWindowMinimumSize(pcWindow,640,480);
 pcRenderer=SDL_CreateRenderer(pcWindow,NULL);if(!pcRenderer)return E_FAIL;SDL_SetRenderVSync(pcRenderer,0);if(full)SDL_SetWindowFullscreen(pcWindow,true);
 frame=image(512,480);duel=image(224,224);for(auto&t:remote)t=image(224,224);labels=image(96,32);scene=image(512,288);hud=image(512,TA_POPULATION_MAX_HEIGHT,true);
 if(!frame||!duel||!labels||!scene||!hud)return E_FAIL;for(auto t:remote)if(!t)return E_FAIL;return S_OK;
}
void pcSaveWindow(){if(!pcWindow)return;int w,h;SDL_GetWindowSize(pcWindow,&w,&h);bool full=(SDL_GetWindowFlags(pcWindow)&SDL_WINDOW_FULLSCREEN)!=0;std::string path=std::string(TA_PC_SaveDirectory())+"/window.ini";FILE*f=fopen(path.c_str(),"w");if(f){fprintf(f,"%d %d %d\n",full?960:w,full?720:h,full?1:0);fclose(f);}}
extern "C" void TA_Video_SetPopulation(unsigned a,unsigned g,unsigned o,unsigned k){population.accounts=a;population.ingame=g;population.online=o;population.known=k;}
extern "C" void TA_Video_SetDuelMode(unsigned){}
extern "C" void TA_Video_SetPartyBackdrop(const uint32_t*p){if(p&&!sceneValid){ta_mobile_backdrop(backdrop,512,288,p,512);sceneValid=upload(scene,backdrop,512,512,288);}}
extern "C" bool TA_Video_SetLanOverlay(const uint32_t*p,unsigned){opponent=p;return true;}
extern "C" bool TA_Video_SetPartyVisual(const TaPartyVisual*p){if(!party||!p||party->self!=p->self){memset(have,0,sizeof have);memset(revision,0,sizeof revision);rosterRevision=~0u;sceneValid=false;}party=p;return true;}
extern "C" bool TA_Video_SetPartyOverlay(const uint32_t*p,unsigned){if(!p){TA_Video_SetPartyVisual(NULL);return true;}return false;}
extern "C" void TA_Video_PartyStats(unsigned long out[4]){memcpy(out,stats,sizeof stats);memset(stats,0,sizeof stats);}
static void present(const uint32_t*p,int height){
 if(!pcRenderer)return;pcEvents();pcMenuFrame=height==480;if(pcMenuFrame)pcMenuCommit();else TA_Menu_Reset();
 if(!upload(frame,p,512,512,height))throw std::runtime_error(SDL_GetError());
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
  if(opponent&&!pcMenuFrame&&upload(duel,opponent,224,224,224)){SDL_FRect r={view.x+304*view.w/512,view.y+24*view.h/224,192*view.w/512,192*view.h/224};draw(duel,r,16,24,192,192);}
 }
 unsigned hw=ta_population_image(hudPixels,512,&population,pcMenuFrame),hh=ta_population_height(pcMenuFrame);
 if(upload(hud,hudPixels,512,hw,hh)){float sx=pcMenuFrame?view.w/512.0f:h/480.0f,sy=pcMenuFrame?view.h/480.0f:sx;SDL_FRect r={pcMenuFrame?view.x+(512-hw)*sx*.5f:w-hw*sx-8,pcMenuFrame?view.y+TA_POPULATION_MENU_TOP*sy:2,hw*sx,hh*sy};draw(hud,r,0,0,(float)hw,(float)hh);}
 SDL_RenderPresent(pcRenderer);pcPace();
}
extern "C" void TA_Video_Submit(const unsigned char*p){present((const uint32_t*)p,480);}
extern "C" bool TA_Video_SubmitActive224(const unsigned char*p){present((const uint32_t*)p,224);return pcRenderer!=NULL;}
extern "C" bool TA_Video_SubmitActive224Direct(const Ppu*p){static uint32_t pixels[512*224];if(!ppu_taCopyActive224(p,(uint8_t*)pixels,sizeof pixels))return false;return TA_Video_SubmitActive224((const unsigned char*)pixels);}
extern "C" bool TA_Video_ActiveAvailable(void){return pcRenderer!=NULL;}
extern "C" bool TA_Video_SubmitNative224Direct(const Ppu*){return false;}
extern "C" void TA_Video_BeginR126(Ppu*){}
extern "C" bool TA_Video_EndR126(Ppu*){return false;}
extern "C" void TA_Video_ReleaseLanPpu(Ppu*){sceneValid=false;}
extern "C" void TA_Video_R126Counts(unsigned long out[4]){memset(out,0,4*sizeof *out);}
extern "C" void TA_Video_ReadReset(unsigned long*a,unsigned long*b,unsigned long*c,unsigned long*d){*a=*b=*c=*d=0;}
extern "C" void TA_Video_ReadResetUploadTicks(uint64_t t[3]){memset(t,0,3*sizeof *t);}
extern "C" void TA_Video_Shutdown(void){for(auto&t:remote){SDL_DestroyTexture(t);t=NULL;}SDL_DestroyTexture(frame);SDL_DestroyTexture(duel);SDL_DestroyTexture(labels);SDL_DestroyTexture(scene);SDL_DestroyTexture(hud);frame=duel=labels=scene=hud=NULL;SDL_DestroyRenderer(pcRenderer);SDL_DestroyWindow(pcWindow);pcRenderer=NULL;pcWindow=NULL;}
