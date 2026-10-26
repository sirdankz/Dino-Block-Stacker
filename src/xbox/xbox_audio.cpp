/* 48k stereo consumer-paced DirectSound ring; initial simple reference audio. */
#include "ta_platform.h"
#include "ta_music_impl.h"
#define SAMPLES 800
#define BYTES (SAMPLES*4)
#define BLOCKS 12
#define DS_BLOCKS 6
static int16_t ring[BLOCKS*SAMPLES*2];
static volatile LONG wp=0,rp=0;
static HANDLE thread=NULL;static volatile bool running=false;
static LPDIRECTSOUND8 ds=NULL;static LPDIRECTSOUNDBUFFER buf=NULL;
static DWORD dspos=0;
static DWORD WINAPI feeder(LPVOID){
  while(running){
    if(buf){DWORD play=0,write=0;
      if(SUCCEEDED(buf->GetCurrentPosition(&play,&write))){
        DWORD freeBytes=(dspos<=play)?play-dspos:DS_BLOCKS*BYTES-dspos+play;
        while(freeBytes>=BYTES){
          LONG rd=rp;LONG avail=(wp-rd+BLOCKS*BYTES)%(BLOCKS*BYTES);
          if(avail<BYTES)break;
          const uint8_t *src=(const uint8_t*)(ring+(rd/BYTES)*SAMPLES*2);
          void *a=NULL,*b=NULL;DWORD na=0,nb=0;
          if(FAILED(buf->Lock(dspos,BYTES,&a,&na,&b,&nb,0)))break;
          if(a)memcpy(a,src,na);if(b)memcpy(b,src+na,nb);
          buf->Unlock(a,na,b,nb);
          dspos=(dspos+BYTES)%(DS_BLOCKS*BYTES);
          rp=(rd+BYTES)%(BLOCKS*BYTES);freeBytes-=BYTES;
        }
      }
    }
    Sleep(2);
  }
  return 0;
}
extern "C" HRESULT TA_Audio_Init(void){
  TA_Music_Init("D:\\music");
  HRESULT hr=DirectSoundCreate(NULL,&ds,NULL);if(FAILED(hr))return hr;
  WAVEFORMATEX w;memset(&w,0,sizeof(w));
  w.wFormatTag=WAVE_FORMAT_PCM;w.nChannels=2;w.nSamplesPerSec=48000;
  w.wBitsPerSample=16;w.nBlockAlign=4;w.nAvgBytesPerSec=192000;
  DSBUFFERDESC d;memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
  d.dwFlags=DSBCAPS_CTRLVOLUME;d.dwBufferBytes=DS_BLOCKS*BYTES;d.lpwfxFormat=&w;
  hr=ds->CreateSoundBuffer(&d,&buf,NULL);if(FAILED(hr))return hr;
  void *a=NULL,*b=NULL;DWORD na=0,nb=0;
  if(SUCCEEDED(buf->Lock(0,DS_BLOCKS*BYTES,&a,&na,&b,&nb,0))){if(a)memset(a,0,na);if(b)memset(b,0,nb);buf->Unlock(a,na,b,nb);}
  wp=rp=0;dspos=0;buf->Play(0,0,DSBPLAY_LOOPING);
  running=true;thread=CreateThread(NULL,0,feeder,NULL,0,NULL);
  return thread?S_OK:E_FAIL;
}
extern "C" void TA_Audio_Submit(const int16_t* pcm){
  if(!pcm)return;static int16_t mixed[1600];if(!TA_Music_Process(pcm,mixed,800))return;pcm=mixed;LONG w=wp,r=rp;LONG used=(w-r+BLOCKS*BYTES)%(BLOCKS*BYTES);
  if(used+BYTES>=BLOCKS*BYTES)return;
  memcpy(ring+(w/BYTES)*SAMPLES*2,pcm,BYTES);
  wp=(w+BYTES)%(BLOCKS*BYTES);
}
extern "C" void TA_Audio_Shutdown(void){TA_Music_Shutdown();running=false;
  if(thread){WaitForSingleObject(thread,1000);CloseHandle(thread);thread=NULL;}
  if(buf){buf->Stop();buf->Release();buf=NULL;}if(ds){ds->Release();ds=NULL;}}
