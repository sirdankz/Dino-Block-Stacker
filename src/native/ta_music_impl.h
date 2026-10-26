#ifndef TA_MUSIC_IMPL_H
#define TA_MUSIC_IMPL_H
#include "ta_music.h"
#include "ta_options.h"
#include <stdio.h>
#include <string.h>
#if defined(TA_MUSIC_PRELOAD_BYTES)
#include <stdlib.h>
#endif
#ifndef TA_MUSIC_CLOCK_MS
#define TA_MUSIC_CLOCK_MS() ((uint32_t)GetTickCount())
#endif
/* Include in exactly one audio translation unit. The music is independent
   licensed content. WAV bytes are decoded explicitly on big-endian PowerPC. */
struct TaMusicTrack {
 #if defined(TA_MUSIC_PRELOAD_BYTES)
 unsigned char*pcm;
 #endif
 FILE*file;unsigned dataOffset,frames,position,bufferAt,bufferFrames;
 int gain,startGain,target;unsigned ramp;
 unsigned char buffer[8192];
};
static TaMusicTrack taMusicTracks[2];
static unsigned taMusicScene=~0u;static bool taMusicMenuSubmit,taMusicClockReady;
static uint32_t taMusicClock,taMusicDebt;
#define TA_MUSIC_RAMP 7200u
static unsigned ta_music_le16(const unsigned char*p){return (unsigned)p[0]|((unsigned)p[1]<<8);}
static unsigned ta_music_le32(const unsigned char*p){return ta_music_le16(p)|(ta_music_le16(p+2)<<16);}
static void ta_music_close(TaMusicTrack*t){if(t->file)fclose(t->file);
#if defined(TA_MUSIC_PRELOAD_BYTES)
 free(t->pcm);
#endif
 memset(t,0,sizeof *t);}
static bool ta_music_open(TaMusicTrack*t,const char*directory,const char*name){
 char path[512];int length=snprintf(path,sizeof path,"%s/%s",directory,name);if(length<0||(unsigned)length>=sizeof path)return false;
 FILE*f=fopen(path,"rb");if(!f)return false;unsigned char h[16]={0};bool valid=false;unsigned data=0,bytes=0;
 if(fseek(f,0,SEEK_END)){fclose(f);return false;}long total=ftell(f);
 if(total<44||total>256*1024*1024||fseek(f,0,SEEK_SET)||fread(h,1,12,f)!=12||memcmp(h,"RIFF",4)||memcmp(h+8,"WAVE",4)||ta_music_le32(h+4)!=(unsigned)total-8){fclose(f);return false;}
 while(ftell(f)>=0&&ftell(f)+8<=total){
  if(fread(h,1,8,f)!=8)break;unsigned n=ta_music_le32(h+4);long begin=ftell(f);if(n>(unsigned)(total-begin))break;
  if(!memcmp(h,"fmt ",4)){unsigned char fmt[16];if(n<16||fread(fmt,1,16,f)!=16)break;valid=ta_music_le16(fmt)==1&&ta_music_le16(fmt+2)==2&&ta_music_le32(fmt+4)==48000&&ta_music_le32(fmt+8)==192000&&ta_music_le16(fmt+12)==4&&ta_music_le16(fmt+14)==16;}
  else if(!memcmp(h,"data",4)){if(data){valid=false;break;}data=(unsigned)begin;bytes=n;}
  if(fseek(f,begin+(long)n+(n&1u),SEEK_SET))break;
 }
 if(!valid||!data||bytes<4||(bytes&3u)||fseek(f,(long)data,SEEK_SET)){fclose(f);return false;}
 t->file=f;t->dataOffset=data;t->frames=bytes/4;
#if defined(TA_MUSIC_PRELOAD_BYTES)
 // Desktop playback owns a bounded startup cache. Consoles keep streaming.
 if(bytes<=TA_MUSIC_PRELOAD_BYTES){t->pcm=(unsigned char*)malloc(bytes);if(t->pcm&&fread(t->pcm,1,bytes,f)!=bytes){free(t->pcm);t->pcm=NULL;}if(!t->pcm)fseek(f,(long)data,SEEK_SET);}
#endif
 return true;
}
static void ta_music_rewind(TaMusicTrack*t){
#if defined(TA_MUSIC_PRELOAD_BYTES)
 if(t->pcm){t->position=t->bufferAt=t->bufferFrames=0;return;}
#endif
 if(t->file&&fseek(t->file,(long)t->dataOffset,SEEK_SET)==0)t->position=t->bufferAt=t->bufferFrames=0;
}
static void ta_music_sample(TaMusicTrack*t,int*out){
 out[0]=out[1]=0;if(!t->file)return;
 if(t->position>=t->frames)ta_music_rewind(t);
#if defined(TA_MUSIC_PRELOAD_BYTES)
 if(t->pcm){const unsigned char*p=t->pcm+t->position++*4;out[0]=(int)(int16_t)ta_music_le16(p);out[1]=(int)(int16_t)ta_music_le16(p+2);return;}
#endif
 if(t->bufferAt>=t->bufferFrames){unsigned count=t->frames-t->position;if(count>2048)count=2048;
  size_t got=fread(t->buffer,4,count,t->file);if(got!=count||!got){ta_music_close(t);return;}t->bufferAt=0;t->bufferFrames=(unsigned)got;}
 const unsigned char*p=t->buffer+t->bufferAt*4;out[0]=(int)(int16_t)ta_music_le16(p);out[1]=(int)(int16_t)ta_music_le16(p+2);t->bufferAt++;t->position++;
}
static void ta_music_step(TaMusicTrack*t){if(t->ramp<TA_MUSIC_RAMP){++t->ramp;t->gain=t->startGain+(t->target-t->startGain)*(int)t->ramp/(int)TA_MUSIC_RAMP;}else t->gain=t->target;}
extern "C" void TA_Music_Shutdown(void){for(unsigned i=0;i<2;i++)ta_music_close(taMusicTracks+i);taMusicScene=~0u;taMusicMenuSubmit=taMusicClockReady=false;taMusicDebt=0;}
extern "C" void TA_Music_SetScene(unsigned scene){
 if(scene>TA_MUSIC_RESULTS||scene==taMusicScene)return;
 if(scene==TA_MUSIC_RESULTS)ta_music_rewind(taMusicTracks+1);
 taMusicScene=scene;if(scene==TA_MUSIC_GAME)taMusicClockReady=false;for(unsigned i=0;i<2;i++){TaMusicTrack*t=taMusicTracks+i;t->startGain=t->gain;t->target=t->file&&scene==i+1?32768:0;t->ramp=0;}
}
extern "C" void TA_Music_Init(const char*directory){TA_Music_Shutdown();if(directory){ta_music_open(taMusicTracks,directory,"menu.wav");ta_music_open(taMusicTracks+1,directory,"results.wav");}TA_Music_SetScene(TA_MUSIC_MENU);}
extern "C" int TA_Music_Process(const int16_t*game,int16_t*out,unsigned frames){
 if(!game||!out||frames>800)return 0;if(taMusicScene!=TA_MUSIC_GAME&&!taMusicMenuSubmit)return 0;
 unsigned master=TA_Options_Get(TA_OPT_MASTER),musicVolume=TA_Options_Get(TA_OPT_MUSIC),gameVolume=TA_Options_Get(TA_OPT_GAME);
 if(taMusicScene==TA_MUSIC_GAME&&!taMusicTracks[0].gain&&!taMusicTracks[1].gain){
  unsigned gain=master*gameVolume;if(gain==10000u)memcpy(out,game,frames*4);else for(unsigned i=0;i<frames*2;i++)out[i]=(int16_t)((int)game[i]*(int)gain/10000);return 1;
 }
 for(unsigned frame=0;frame<frames;frame++){
  int mix[2]={0,0},sum=0;
  for(unsigned i=0;i<2;i++){TaMusicTrack*t=taMusicTracks+i;ta_music_step(t);sum+=t->gain;
   if(t->gain||t->target){int pcm[2];ta_music_sample(t,pcm);int gain=t->gain*(int)musicVolume/100;mix[0]+=pcm[0]*gain;mix[1]+=pcm[1]*gain;}}
  int guest=taMusicScene==TA_MUSIC_GAME?(32768-(sum<32768?sum:32768))*(int)gameVolume/100:0;
  for(unsigned c=0;c<2;c++){int value=(mix[c]+(int)game[frame*2+c]*guest)/32768;if(value>32767)value=32767;if(value< -32768)value=-32768;out[frame*2+c]=(int16_t)(value*(int)master/100);}
 }return 1;
}
extern "C" void TA_Audio_Submit(const int16_t*);
extern "C" void TA_Music_MenuFrame(void){
 if(taMusicScene==TA_MUSIC_GAME||taMusicScene==~0u)return;
 static const int16_t silence[1600]={0};uint32_t now=TA_MUSIC_CLOCK_MS();
 if(!taMusicClockReady){taMusicClockReady=true;taMusicClock=now;taMusicDebt=800;}
 else{uint32_t elapsed=now-taMusicClock;taMusicClock=now;if(elapsed>100)elapsed=100;taMusicDebt+=elapsed*48u;if(taMusicDebt>2400)taMusicDebt=2400;}
 while(taMusicDebt>=800){taMusicDebt-=800;taMusicMenuSubmit=true;TA_Audio_Submit(silence);taMusicMenuSubmit=false;}
}
#endif
