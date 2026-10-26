#include "ta_360_platform.h"
#include <xaudio2.h>
#include "ta_music_impl.h"
static IXAudio2*engine;
static IXAudio2MasteringVoice*master;
static IXAudio2SourceVoice*voice;
static HANDLE completed;
struct AudioSlot {volatile LONG busy;short samples[1600];};
static AudioSlot slots[6];
struct AudioCallback:IXAudio2VoiceCallback{
 void OnVoiceProcessingPassStart(UINT32){} void OnVoiceProcessingPassEnd(){}
 void OnStreamEnd(){} void OnBufferStart(void*){} void OnLoopEnd(void*){}
 void OnBufferEnd(void*context){if(context)InterlockedExchange(&static_cast<AudioSlot*>(context)->busy,0);SetEvent(completed);}
 void OnVoiceError(void*,HRESULT){SetEvent(completed);}
};
static AudioCallback callback;
extern "C" HRESULT TA_Audio_Init(void){
 TA_Music_Init("game:\\music");
 completed=CreateEvent(NULL,FALSE,FALSE,NULL);if(!completed)return E_FAIL;
 HRESULT hr=XAudio2Create(&engine);if(FAILED(hr))return hr;
 hr=engine->CreateMasteringVoice(&master,2,48000);if(FAILED(hr))return hr;
 WAVEFORMATEX fmt;memset(&fmt,0,sizeof fmt);fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=2;fmt.nSamplesPerSec=48000;fmt.wBitsPerSample=16;fmt.nBlockAlign=4;fmt.nAvgBytesPerSec=192000;
 hr=engine->CreateSourceVoice(&voice,&fmt,0,1.0f,&callback);if(FAILED(hr))return hr;
 return voice->Start(0);
}
extern "C" void TA_Audio_Submit(const int16_t*data){
 if(!voice||!data)return;static int16_t mixed[1600];if(!TA_Music_Process(data,mixed,800))return;data=mixed;
 AudioSlot*slot=NULL;for(unsigned i=0;i<6;i++)if(InterlockedCompareExchange(&slots[i].busy,1,0)==0){slot=slots+i;break;}
 /* Never overwrite audio still owned by the mixer, and never block networking. */
 if(!slot)return;
 memcpy(slot->samples,data,sizeof slot->samples);
 XAUDIO2_BUFFER buffer;memset(&buffer,0,sizeof buffer);buffer.AudioBytes=sizeof slot->samples;buffer.pAudioData=(const BYTE*)slot->samples;buffer.pContext=slot;
 if(FAILED(voice->SubmitSourceBuffer(&buffer)))InterlockedExchange(&slot->busy,0);
}
extern "C" void TA_Audio_Shutdown(void){
 TA_Music_Shutdown();
 if(voice){voice->Stop();voice->DestroyVoice();voice=NULL;}if(master){master->DestroyVoice();master=NULL;}if(engine){engine->Release();engine=NULL;}if(completed){CloseHandle(completed);completed=NULL;}
}
