#include "ta_android_platform.h"
#include "ta_controls_impl.h"
#include "ta_vs_mobile_view.h"
#include <jni.h>
#include <android/log.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
extern "C" {
#include "snes.h"
#include "ta_native.h"
#include "ta_rom_sha256.h"
}
using Clock=std::chrono::steady_clock;
static std::atomic<bool> stopped{false},paused{false},sessionRunning{false};
static std::atomic<unsigned> buttons[2],touchButtons;
static std::mutex frameMutex;static uint32_t displayFrame[512*480];static unsigned frameHeight=480,frameRevision,lastCopied=~0u;
static const TaPartyVisual*partyVisual;static std::atomic<bool> partyCacheValid{false};static std::atomic<int> viewportWidth{0},viewportHeight{0};
static unsigned long partyStats[4];
#include "ta_vs_population.h"
static TaPopulation population;
extern "C" void TA_Video_SetPartyBackdrop(const uint32_t*){}
extern "C" void TA_Video_SetPopulation(unsigned a,unsigned g,unsigned o,unsigned known){population.accounts=a;population.ingame=g;population.online=o;population.known=known;}

static std::atomic<int> rightX{0},rightY{0};
extern "C" void TA_Input_RightStick(int*x,int*y){*x=rightX.load();*y=rightY.load();}
extern "C" bool TA_Video_SetPartyVisual(const TaPartyVisual*p){if(p!=partyVisual)partyCacheValid=false;partyVisual=p;if(!p)partyCacheValid=false;return true;}
extern "C" bool TA_Video_SetPartyOverlay(const uint32_t*p,unsigned){return !p?TA_Video_SetPartyVisual(NULL):false;}
extern "C" void TA_Video_PartyStats(unsigned long out[4]){for(unsigned k=0;k<4;k++){out[k]=partyStats[k];partyStats[k]=0;}}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeTouchButtons(JNIEnv*,jclass,jint mask){touchButtons=(unsigned)mask;}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeViewport(JNIEnv*,jclass,jint w,jint h){viewportWidth=w;viewportHeight=h;partyCacheValid=false;}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeRightStick(JNIEnv*,jclass,jint x,jint y){rightX=x;rightY=y;}
static bool legacyBackdropValid;static unsigned opponentRevision;static const uint32_t*opponent;static std::string cacheRoot;
static JNIEnv*gameEnv;static jobject gameActivity;static jshortArray audioArray;static jmethodID audioMethod;
static Clock::time_point nextFrame;
struct AndroidStop{};
extern "C" const char*ta_android_cache_dir(void){return cacheRoot.c_str();}
extern "C" void TA_Log(const char*fmt,...){va_list ap;va_start(ap,fmt);__android_log_vprint(ANDROID_LOG_INFO,"DinoBlockStacker",fmt,ap);va_end(ap);}
static void (*backgroundTick)(void);
extern "C" void TA_Background_Set(void (*tick)(void)){backgroundTick=tick;}
extern "C" void TA_Input_Poll(uint16_t out[2]){while(paused&&!stopped)std::this_thread::sleep_for(std::chrono::milliseconds(10));if(stopped)throw AndroidStop();if(backgroundTick)backgroundTick();out[0]=(uint16_t)buttons[0].load(std::memory_order_relaxed);out[1]=(uint16_t)buttons[1].load(std::memory_order_relaxed);controlsPhysical[0]=out[0];controlsPhysical[1]=out[1];controlsTouch=touchButtons.load(std::memory_order_relaxed);out[0]|=(uint16_t)controlsTouch;}
/* Portable presentation fallback: cache opponents at8Hz, then scale only the
   local image each presented frame. Original pixel values and frames survive. */
static void partyBlit(uint32_t*out,unsigned rows,TaPartyRect rect,const uint32_t*source,unsigned stride,unsigned sourceX,unsigned sourceW,unsigned sourceH=224,unsigned sourceY=0){
 int left=(int)rect.left,top=(int)rect.top,right=(int)rect.right,bottom=(int)rect.bottom;
 if(left<0)left=0;if(top<0)top=0;if(right>512)right=512;if(bottom>(int)rows)bottom=(int)rows;
 unsigned w=(unsigned)(right-left),h=(unsigned)(bottom-top);if(!w||!h||!source)return;
 unsigned lookup[512];for(unsigned x=0;x<w;x++)lookup[x]=sourceX+x*sourceW/w;
 for(unsigned y=0;y<h;y++){const uint32_t*src=source+(sourceY+y*sourceH/h)*stride;uint32_t*dst=out+(top+y)*512+left;for(unsigned x=0;x<w;x++)dst[x]=src[lookup[x]];}
}
static unsigned onlineRows(void){int w=viewportWidth.load(),h=viewportHeight.load();unsigned n=w>0&&h>0?(unsigned)(512LL*h/w):288u;if(n<128)n=128;if(n>384)n=384;if(n==224)n=226;return n;}
static unsigned partyCompose(const uint32_t*local){
 static uint32_t canvas[512*384],lastAt[4];static unsigned uploadTurn;static unsigned lastRevision[4],lastRoster,lastRows;static bool have[4];
 unsigned wide=viewportHeight>0&&viewportWidth.load()*100>=viewportHeight.load()*155;
 unsigned rows=onlineRows(),peers=0;for(unsigned s=0;s<4;s++)if(s!=partyVisual->self&&(partyVisual->members&(1u<<s)))peers++;
 TaPartyLayout layout=ta_party_layout_ex(512,rows,wide,peers,1);uint32_t now=GetTickCount();
 if(!partyCacheValid||lastRows!=rows){ta_mobile_backdrop(canvas,512,rows,local,512);memset(lastRevision,0,sizeof lastRevision);memset(have,0,sizeof have);lastRoster=~0u;lastRows=rows;partyCacheValid=true;}
 if(lastRoster!=partyVisual->rosterRevision){partyBlit(canvas,rows,layout.label[0],partyVisual->roster,512,0,96,8,partyVisual->self*8);
  unsigned index=1;for(unsigned s=0;s<4;s++)if(s!=partyVisual->self&&(partyVisual->members&(1u<<s)))partyBlit(canvas,rows,layout.label[index++],partyVisual->roster,512,0,96,8,s*8);lastRoster=partyVisual->rosterRevision;partyStats[1]++;partyStats[3]+=96*32*4;}
 unsigned indexFor[4]={0},index=0;for(unsigned s=0;s<4;s++)if(s!=partyVisual->self&&(partyVisual->members&(1u<<s)))indexFor[s]=index++;
 for(unsigned k=0;k<4;k++){unsigned s=(uploadTurn+k)%4;if(s==partyVisual->self||!(partyVisual->members&(1u<<s)))continue;
  if(ta_party_upload_due(have[s],lastRevision[s],partyVisual->revision[s],now,lastAt[s],peers>1?125u:66u)){partyBlit(canvas,rows,layout.peer[indexFor[s]],partyVisual->pixels[s],224,0,224);lastRevision[s]=partyVisual->revision[s];lastAt[s]=now;have[s]=true;uploadTurn=(s+1)%4;partyStats[0]++;partyStats[3]+=224*224*4;break;}
 }
 memcpy(displayFrame,canvas,512*rows*4);partyBlit(displayFrame,rows,layout.local,local,512,0,224,224-layout.localSourceY,layout.localSourceY);return rows;
}
static void present(const uint32_t*p,unsigned height){
 if(stopped)throw AndroidStop();{
 std::lock_guard<std::mutex> lock(frameMutex);memcpy(displayFrame,p,512*height*4);
 /* Same N13 source/destination playfield geometry as Xbox. No added HUD. */
 if(partyVisual&&height==224)height=partyCompose(p);
 else if(opponent&&height==224){
  static uint32_t peers[512*384];static unsigned revision=~0u,oldRows;unsigned rows=onlineRows();TaPartyLayout l=ta_party_layout_ex(512,rows,1,1,1);
  if(!legacyBackdropValid||oldRows!=rows){ta_mobile_backdrop(peers,512,rows,p,512);legacyBackdropValid=true;revision=~opponentRevision;}
  if(revision!=opponentRevision||oldRows!=rows){partyBlit(peers,rows,l.peer[0],opponent,224,0,224);revision=opponentRevision;oldRows=rows;}
  memcpy(displayFrame,peers,512*rows*4);partyBlit(displayFrame,rows,l.local,p,512,0,224,224-l.localSourceY,l.localSourceY);height=rows;
 }
 static uint32_t hud[512*TA_POPULATION_MAX_HEIGHT];unsigned menu=height==480;
 unsigned hw=ta_population_image(hud,512,&population,menu),hh=ta_population_height(menu);
 unsigned hx=menu?(512-hw)/2:512-hw-5,hy=menu?6:1;
 for(unsigned y=0;y<hh;y++)memcpy(displayFrame+(hy+y)*512+hx,hud+y*512,hw*4);
 frameHeight=height;frameRevision++;}
 auto now=Clock::now();if(nextFrame<now-std::chrono::milliseconds(100))nextFrame=now;nextFrame+=std::chrono::nanoseconds(1000000000LL/60);std::this_thread::sleep_until(nextFrame);
}
extern "C" void TA_Video_Submit(const unsigned char*p){present((const uint32_t*)p,480);}
extern "C" bool TA_Video_SubmitActive224(const unsigned char*p){present((const uint32_t*)p,224);return true;}
extern "C" void TA_Video_BeginR126(Ppu*){}
extern "C" bool TA_Video_EndR126(Ppu*){return false;}
extern "C" bool TA_Video_SubmitNative224Direct(const Ppu*){return false;}
extern "C" bool TA_Video_SubmitActive224Direct(const Ppu*p){static uint32_t pixels[512*224];if(!ppu_taCopyActive224(p,(uint8_t*)pixels,sizeof pixels))return false;present(pixels,224);return true;}
extern "C" bool TA_Video_SetLanOverlay(const uint32_t*p,unsigned revision){if(!p||opponent!=p)legacyBackdropValid=false;opponentRevision=revision;opponent=p;return true;}
extern "C" void TA_Video_ReleaseLanPpu(Ppu*){}
extern "C" void TA_Audio_Submit(const int16_t*p){if(gameEnv&&audioArray&&audioMethod){gameEnv->SetShortArrayRegion(audioArray,0,1600,(const jshort*)p);gameEnv->CallVoidMethod(gameActivity,audioMethod,audioArray);if(gameEnv->ExceptionCheck()){gameEnv->ExceptionClear();TA_Log("Audio output unavailable; guest APU timing retained\n");audioMethod=NULL;}}}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativePrepare(JNIEnv*,jclass){if(!sessionRunning){stopped=false;paused=false;buttons[0]=buttons[1]=0;touchButtons=0;}}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeButtons(JNIEnv*,jclass,jint player,jint mask){if(player>=0&&player<2)buttons[player]=ta_controls_physical_bits((unsigned)mask);}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativePause(JNIEnv*,jclass,jboolean p){paused=p;buttons[0]=buttons[1]=0;touchButtons=0;}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeStop(JNIEnv*,jclass){stopped=true;paused=false;buttons[0]=buttons[1]=0;touchButtons=0;}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeRefresh(JNIEnv*,jclass){std::lock_guard<std::mutex> lock(frameMutex);lastCopied=~0u;}
extern "C" JNIEXPORT jint JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeCopyFrame(JNIEnv*env,jclass,jobject buffer){
 void*memory=env->GetDirectBufferAddress(buffer);if(!memory||env->GetDirectBufferCapacity(buffer)<512*480*4)return 0;std::lock_guard<std::mutex> lock(frameMutex);if(lastCopied==frameRevision)return 0;
 uint32_t*out=(uint32_t*)memory;for(unsigned i=0;i<512*frameHeight;i++){uint32_t c=displayFrame[i];out[i]=(c&0xff00u)|((c>>16)&255u)|((c&255u)<<16)|0xff000000u;}lastCopied=frameRevision;return (jint)frameHeight;
}
extern "C" int ta_android_chat_input(char*out,unsigned capacity){
 if(!out||capacity<2||!gameEnv||!gameActivity||stopped)return 0;
 JNIEnv*env=gameEnv;jclass cls=env->GetObjectClass(gameActivity);
 jmethodID method=env->GetMethodID(cls,"promptChatText","()Ljava/lang/String;");
 env->DeleteLocalRef(cls);
 if(!method){if(env->ExceptionCheck())env->ExceptionClear();return 0;}
 jstring answer=(jstring)env->CallObjectMethod(gameActivity,method);
 if(env->ExceptionCheck()){env->ExceptionClear();return 0;}
 if(!answer)return 0;
 const char*data=env->GetStringUTFChars(answer,NULL);unsigned count=0;
 if(data){
  while(data[count]&&count<capacity-1){unsigned char ch=(unsigned char)data[count];if(ch<32||ch>126){count=0;break;}count++;}
  if(count&&data[count]==0){memcpy(out,data,count);out[count]=0;}else count=0;
  env->ReleaseStringUTFChars(answer,data);
 }
 env->DeleteLocalRef(answer);return count?1:0;
}
static std::string getString(JNIEnv*env,jstring s){const char*p=env->GetStringUTFChars(s,NULL);std::string r=p?p:"";if(p)env->ReleaseStringUTFChars(s,p);return r;}
extern "C" JNIEXPORT jstring JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeRun(JNIEnv*env,jobject activity,jstring romName,jstring directory){
 bool expected=false;if(!sessionRunning.compare_exchange_strong(expected,true))return env->NewStringUTF("A game is already running.");buttons[0]=buttons[1]=0;touchButtons=0;opponent=NULL;partyVisual=NULL;partyCacheValid=false;rightX=rightY=0;nextFrame=Clock::now();
 std::string error,romPath=getString(env,romName);cacheRoot=getString(env,directory);gameEnv=env;gameActivity=activity;audioArray=env->NewShortArray(1600);audioMethod=env->GetMethodID(env->GetObjectClass(activity),"writeAudio","([S)V");Snes*offline=NULL;
 try{
  std::vector<uint8_t>rom(1048576);FILE*f=fopen(romPath.c_str(),"rb");bool ok=f&&fread(rom.data(),1,rom.size(),f)==rom.size()&&fgetc(f)==EOF;if(f)fclose(f);uint8_t hash[32];if(ok)ta_rom_sha256(rom.data(),rom.size(),hash);
  static const uint8_t expectedHash[32]={0x57,0x9b,0xf1,0xa1,0x98,0x8d,0x4a,0xf0,0x6a,0x69,0xcc,0x1d,0x82,0xa2,0x47,0x8e,0xbe,0x51,0x94,0x0c,0x5c,0xed,0x7f,0x97,0xe8,0x30,0x29,0xa2,0x4e,0x6a,0xa7,0x78};
  if(!ok||memcmp(hash,expectedHash,32))error="Import the compatible 1 MB unheadered ROM listed in the README.";
  else{
   TA_Log("ANDROID A1 N17 crossplay; original rules/timing/packet layouts; portable renderer\n");TA_LanGame_Run(rom.data(),(unsigned)rom.size());
   offline=snes_init();if(!offline||!snes_loadRom(offline,rom.data(),(unsigned)rom.size())||!ta_native_init(rom.data(),(unsigned)rom.size()))error="Could not load original game.";
   else{snes_setPixelFormat(offline,pixelFormatXRGB);static int16_t pcm[1600];static uint8_t pixels[512*480*4];for(;;){uint16_t pads[2];TA_Input_Poll(pads);for(unsigned p=0;p<2;p++)pads[p]=TA_Input_GameButtons(pads[p],p);for(unsigned p=0;p<2;p++)for(unsigned b=0;b<12;b++)snes_setButtonState(offline,p+1,b,(pads[p]>>b)&1);snes_runFrame(offline);if(!TA_Video_SubmitActive224Direct(offline->ppu)){snes_setPixels(offline,pixels);TA_Video_Submit(pixels);}snes_setSamples(offline,pcm,800);TA_Audio_Submit(pcm);}}
  }
 }catch(const AndroidStop&){}catch(const std::exception&e){error=e.what();}catch(...){error="Native game stopped unexpectedly.";}
 if(offline)snes_free(offline);ta_android_frontend_close();audioMethod=NULL;if(audioArray)env->DeleteLocalRef(audioArray);audioArray=NULL;gameActivity=NULL;gameEnv=NULL;sessionRunning=false;return error.empty()?NULL:env->NewStringUTF(error.c_str());
}
