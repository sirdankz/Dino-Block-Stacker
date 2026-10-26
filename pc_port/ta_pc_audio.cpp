#include "ta_pc_internal.h"
#include "ta_music.h"
#include "ta_options.h"
#include "ta_pc_audio_ring.h"
static std::atomic<unsigned> volume[3]={{TA_DEFAULT_MASTER},{TA_DEFAULT_MUSIC},{TA_DEFAULT_GAME}};
static unsigned pcAudioOption(unsigned option){return option<3?volume[option].load(std::memory_order_relaxed):0;}
#define TA_MUSIC_PRELOAD_BYTES (32u*1024u*1024u)
#define TA_MUSIC_CLOCK_MS() ((uint32_t)(SDL_GetTicksNS()/1000000u))
#define TA_Options_Get pcAudioOption
#define TA_Music_SetScene pcMusicApplyScene
#define TA_Music_MenuFrame pcMusicUnusedMenuFrame
#include "ta_music_impl.h"
#undef TA_Options_Get
#undef TA_Music_SetScene
#undef TA_Music_MenuFrame

static SDL_AudioStream*audioStream;
static PcPcmRing pcmQueue;
static std::atomic<uint32_t> requested{1};
static std::atomic<unsigned> underflows{0},overflows{0};
static uint32_t applied=~0u;
static bool priming=true;
static int16_t lastSample[2];
static unsigned recover;
extern "C" void TA_PC_AudioOptionsChanged(void){for(unsigned i=0;i<3;i++)volume[i].store(TA_Options_Get(i),std::memory_order_relaxed);}
extern "C" void TA_Music_SetScene(unsigned scene){if(scene>TA_MUSIC_RESULTS)return;uint32_t old=requested.load(std::memory_order_relaxed);if((old&3u)!=scene)requested.store(((old+4u)&~3u)|scene,std::memory_order_release);}
extern "C" void TA_Music_MenuFrame(void){/* The device clock owns PC menu playback. */}
static void audioBlock(int16_t*out){
 uint32_t command=requested.load(std::memory_order_acquire);unsigned scene=command&3u;
 if(command!=applied){pcMusicApplyScene(scene);applied=command;priming=true;recover=128;lastSample[0]=lastSample[1]=0;}
 int16_t guest[1600]={0};pcmQueue.discardBefore(command);
 if(scene==TA_MUSIC_GAME){
  if(priming&&pcmQueue.queued(command)>=3)priming=false;
  if(!priming&&pcmQueue.pop(command,guest)){
   if(recover){for(unsigned f=0;f<800&&recover;f++,recover--)for(unsigned c=0;c<2;c++)guest[f*2+c]=(int16_t)((int)guest[f*2+c]*(int)(128-recover)/128);}
   lastSample[0]=guest[1598];lastSample[1]=guest[1599];
  }else{
   if(!priming){underflows.fetch_add(1,std::memory_order_relaxed);priming=true;recover=128;}
   for(unsigned f=0;f<128;f++)for(unsigned c=0;c<2;c++)guest[f*2+c]=(int16_t)((int)lastSample[c]*(int)(127-f)/128);lastSample[0]=lastSample[1]=0;
  }
 }
 taMusicMenuSubmit=true;TA_Music_Process(guest,out,800);taMusicMenuSubmit=false;
}
static void SDLCALL audioNeeded(void*,SDL_AudioStream*stream,int additional,int){
 /* Cached PCM, fixed buffers and atomics only: no file I/O or frame-thread lock. */
 int16_t mixed[1600];while(additional>0){audioBlock(mixed);if(!SDL_PutAudioStreamData(stream,mixed,sizeof mixed))break;additional-=(int)sizeof mixed;}
}
extern "C" HRESULT TA_Audio_Init(void){
 TA_PC_AudioOptionsChanged();std::string path=std::string(SDL_GetBasePath())+"music";TA_Music_Init(path.c_str());for(auto&t:taMusicTracks)if(t.file&&!t.pcm)ta_music_close(&t);requested.store(TA_MUSIC_MENU);applied=~0u;priming=true;recover=128;pcmQueue.reset();underflows=0;overflows=0;
 SDL_AudioSpec spec={SDL_AUDIO_S16,2,48000};audioStream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,audioNeeded,NULL);
 if(!audioStream)return E_FAIL;if(!SDL_ResumeAudioStreamDevice(audioStream)){SDL_DestroyAudioStream(audioStream);audioStream=NULL;return E_FAIL;}return S_OK;
}
extern "C" void TA_Audio_Submit(const int16_t*pcm){if(!audioStream||!pcm)return;uint32_t command=requested.load(std::memory_order_acquire);if((command&3u)==TA_MUSIC_GAME&&!pcmQueue.push(command,pcm))overflows.fetch_add(1,std::memory_order_relaxed);}
void pcAudioStats(unsigned&underruns,unsigned&overruns){underruns=underflows.load(std::memory_order_relaxed);overruns=overflows.load(std::memory_order_relaxed);}
extern "C" void TA_Audio_Shutdown(void){if(audioStream)SDL_DestroyAudioStream(audioStream);audioStream=NULL;TA_Music_Shutdown();pcmQueue.reset();}
