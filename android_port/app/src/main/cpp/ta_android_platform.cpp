#include "ta_android_platform.h"
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
static std::atomic<unsigned> buttons[2];
static std::mutex frameMutex;static uint32_t displayFrame[512*480];static unsigned frameHeight=480,frameRevision,lastCopied=~0u;
static const uint32_t*opponent;static std::string cacheRoot;
static JNIEnv*gameEnv;static jobject gameActivity;static jshortArray audioArray;static jmethodID audioMethod;
static Clock::time_point nextFrame;
struct AndroidStop{};
extern "C" const char*ta_android_cache_dir(void){return cacheRoot.c_str();}
extern "C" void TA_Log(const char*fmt,...){va_list ap;va_start(ap,fmt);__android_log_vprint(ANDROID_LOG_INFO,"DinoBlockStacker",fmt,ap);va_end(ap);}
extern "C" void TA_Input_Poll(uint16_t out[2]){while(paused&&!stopped)std::this_thread::sleep_for(std::chrono::milliseconds(10));if(stopped)throw AndroidStop();out[0]=(uint16_t)buttons[0].load(std::memory_order_relaxed);out[1]=(uint16_t)buttons[1].load(std::memory_order_relaxed);}
static void present(const uint32_t*p,unsigned height){
 if(stopped)throw AndroidStop();{
 std::lock_guard<std::mutex> lock(frameMutex);memcpy(displayFrame,p,512*height*4);
 /* Same N13 source/destination playfield geometry as Xbox. No added HUD. */
 if(opponent&&height==224)for(unsigned y=24;y<216;y++)memcpy(displayFrame+y*512+304,opponent+y*224+16,192*4);
 frameHeight=height;frameRevision++;}
 auto now=Clock::now();if(nextFrame<now-std::chrono::milliseconds(100))nextFrame=now;nextFrame+=std::chrono::nanoseconds(1000000000LL/60);std::this_thread::sleep_until(nextFrame);
}
extern "C" void TA_Video_Submit(const unsigned char*p){present((const uint32_t*)p,480);}
extern "C" bool TA_Video_SubmitActive224(const unsigned char*p){present((const uint32_t*)p,224);return true;}
extern "C" void TA_Video_BeginR126(Ppu*){}
extern "C" bool TA_Video_EndR126(Ppu*){return false;}
extern "C" bool TA_Video_SubmitNative224Direct(const Ppu*){return false;}
extern "C" bool TA_Video_SubmitActive224Direct(const Ppu*p){static uint32_t pixels[512*224];if(!ppu_taCopyActive224(p,(uint8_t*)pixels,sizeof pixels))return false;present(pixels,224);return true;}
extern "C" bool TA_Video_SetLanOverlay(const uint32_t*p,unsigned){opponent=p;return true;}
extern "C" void TA_Video_ReleaseLanPpu(Ppu*){}
extern "C" void TA_Audio_Submit(const int16_t*p){if(gameEnv&&audioArray&&audioMethod){gameEnv->SetShortArrayRegion(audioArray,0,1600,(const jshort*)p);gameEnv->CallVoidMethod(gameActivity,audioMethod,audioArray);if(gameEnv->ExceptionCheck()){gameEnv->ExceptionClear();TA_Log("Audio output unavailable; guest APU timing retained\n");audioMethod=NULL;}}}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativePrepare(JNIEnv*,jclass){if(!sessionRunning){stopped=false;paused=false;buttons[0]=buttons[1]=0;}}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeButtons(JNIEnv*,jclass,jint player,jint mask){if(player>=0&&player<2)buttons[player]=(unsigned)mask&0xfffu;}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativePause(JNIEnv*,jclass,jboolean p){paused=p;buttons[0]=buttons[1]=0;}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeStop(JNIEnv*,jclass){stopped=true;paused=false;buttons[0]=buttons[1]=0;}
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
 bool expected=false;if(!sessionRunning.compare_exchange_strong(expected,true))return env->NewStringUTF("A game is already running.");buttons[0]=buttons[1]=0;opponent=NULL;nextFrame=Clock::now();
 std::string error,romPath=getString(env,romName);cacheRoot=getString(env,directory);gameEnv=env;gameActivity=activity;audioArray=env->NewShortArray(1600);audioMethod=env->GetMethodID(env->GetObjectClass(activity),"writeAudio","([S)V");Snes*offline=NULL;
 try{
  std::vector<uint8_t>rom(1048576);FILE*f=fopen(romPath.c_str(),"rb");bool ok=f&&fread(rom.data(),1,rom.size(),f)==rom.size()&&fgetc(f)==EOF;if(f)fclose(f);uint8_t hash[32];if(ok)ta_rom_sha256(rom.data(),rom.size(),hash);
  static const uint8_t expectedHash[32]={0x57,0x9b,0xf1,0xa1,0x98,0x8d,0x4a,0xf0,0x6a,0x69,0xcc,0x1d,0x82,0xa2,0x47,0x8e,0xbe,0x51,0x94,0x0c,0x5c,0xed,0x7f,0x97,0xe8,0x30,0x29,0xa2,0x4e,0x6a,0xa7,0x78};
  if(!ok||memcmp(hash,expectedHash,32))error="Import the compatible 1 MB unheadered ROM listed in the README.";
  else{
   TA_Log("ANDROID A1 N17 crossplay; original rules/timing/packet layouts; portable renderer\n");TA_LanGame_Run(rom.data(),(unsigned)rom.size());
   offline=snes_init();if(!offline||!snes_loadRom(offline,rom.data(),(unsigned)rom.size())||!ta_native_init(rom.data(),(unsigned)rom.size()))error="Could not load original game.";
   else{snes_setPixelFormat(offline,pixelFormatXRGB);static int16_t pcm[1600];static uint8_t pixels[512*480*4];for(;;){uint16_t pads[2];TA_Input_Poll(pads);for(unsigned p=0;p<2;p++)for(unsigned b=0;b<12;b++)snes_setButtonState(offline,p+1,b,(pads[p]>>b)&1);snes_runFrame(offline);if(!TA_Video_SubmitActive224Direct(offline->ppu)){snes_setPixels(offline,pixels);TA_Video_Submit(pixels);}snes_setSamples(offline,pcm,800);TA_Audio_Submit(pcm);}}
  }
 }catch(const AndroidStop&){}catch(const std::exception&e){error=e.what();}catch(...){error="Native game stopped unexpectedly.";}
 if(offline)snes_free(offline);ta_android_frontend_close();audioMethod=NULL;if(audioArray)env->DeleteLocalRef(audioArray);audioArray=NULL;gameActivity=NULL;gameEnv=NULL;sessionRunning=false;return error.empty()?NULL:env->NewStringUTF(error.c_str());
}
