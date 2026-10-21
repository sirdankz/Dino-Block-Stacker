#include "ta_vs_start.h"
#include <stdio.h>
#include <string.h>
static unsigned word(const uint8_t*w,unsigned a){return w[a]|w[a+1]<<8;}
static void w32(uint8_t*p,uint32_t n){for(unsigned k=0;k<4;k++)p[k]=(uint8_t)(n>>(8*k));}
static uint32_t r32(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t crc(uint32_t h,const void*v,size_t n){const uint8_t*p=v;while(n--){h^=*p++;for(unsigned k=0;k<8;k++)h=(h>>1)^((0u-(h&1u))&0xedb88320u);}return h;}
int ta_vs_start_save(TaVsStart*t,Snes*s,unsigned frame){if(!t||!s)return -1;int n=snes_saveStateFast(s,t->data,sizeof t->data);if(n<=0)return -1;t->bytes=(uint32_t)n;t->frame=frame;
#if defined(XBOX_PORT)
 t->masterPending=s->apuMasterPending;t->cycleDebt=s->apuCycleDebtNumerator;
#else
 t->masterPending=0;t->cycleDebt=0;
#endif
 return 0;}
int ta_vs_start_load(const TaVsStart*t,Snes*s){if(!t||!s||!t->bytes||t->bytes>TA_START_CAPACITY||!snes_loadStateFast(s,t->data,(int)t->bytes))return -1;
#if defined(XBOX_PORT)
 s->apuMasterPending=t->masterPending;s->apuCycleDebtNumerator=t->cycleDebt;
#endif
 s->ppu->noPixels=false;return 0;}
int ta_vs_start_seed(Snes*s,uint32_t seed){
 /* Verified original menu just entered match initialization; no starting rows
    have been built. Supply only the game's existing RNG words, not panel RAM. */
 if(!s||word(s->ram,0x2a8)!=2||word(s->ram,0x2a0)!=3||s->ppu->mode!=1)return -1;
 unsigned index=seed&2047u,bias=(seed>>11)&255u;s->ram[0x1a74]=(uint8_t)index;s->ram[0x1a75]=(uint8_t)(index>>8);s->ram[0x1a76]=(uint8_t)bias;s->ram[0x1a77]=0;return 0;
}
int ta_vs_start_write(const char*name,const uint8_t hash[32],const TaVsStart*t,const TaVsPreview*p){
 if(!name||!hash||!t||t->bytes>TA_START_CAPACITY||!p||!p->ready)return -1;
 uint8_t h[72]={0};memcpy(h,"TAN11C01",8);memcpy(h+8,hash,32);w32(h+40,t->bytes);w32(h+44,t->frame);w32(h+48,t->masterPending);w32(h+52,(uint32_t)t->cycleDebt);w32(h+56,(uint32_t)((uint64_t)t->cycleDebt>>32));
 uint32_t sum=crc(~0u,h,68);sum=crc(sum,t->data,t->bytes);sum=crc(sum,p->pixels,sizeof p->pixels);w32(h+68,~sum);
 FILE*f=fopen(name,"wb");if(!f)return -1;int ok=fwrite(h,1,sizeof h,f)==sizeof h&&fwrite(t->data,1,t->bytes,f)==t->bytes&&fwrite(p->pixels,1,sizeof p->pixels,f)==sizeof p->pixels;if(fclose(f))ok=0;return ok?0:-1;
}
int ta_vs_start_read_frame(const char*name,const uint8_t hash[32],unsigned expected,TaVsStart*t,TaVsPreview*p){
 if(!name||!hash||!t||!p)return -1;FILE*f=fopen(name,"rb");if(!f)return -1;uint8_t h[72];int ok=fread(h,1,sizeof h,f)==sizeof h&&!memcmp(h,"TAN11C01",8)&&!memcmp(h+8,hash,32);
 unsigned n=ok?r32(h+40):0;if(!n||n>TA_START_CAPACITY||r32(h+44)!=expected)ok=0;
 if(ok)ok=fread(t->data,1,n,f)==n&&fread(p->pixels,1,sizeof p->pixels,f)==sizeof p->pixels&&fgetc(f)==EOF;
 fclose(f);if(!ok)return -1;uint32_t sum=crc(~0u,h,68);sum=crc(sum,t->data,n);sum=crc(sum,p->pixels,sizeof p->pixels);if(~sum!=r32(h+68))return -1;
 t->bytes=n;t->frame=r32(h+44);t->masterPending=r32(h+48);t->cycleDebt=(int64_t)((uint64_t)r32(h+52)|(uint64_t)r32(h+56)<<32);p->ready=1;return 0;
}

int ta_vs_start_read(const char*n,const uint8_t h[32],TaVsStart*t,TaVsPreview*p){return ta_vs_start_read_frame(n,h,1839,t,p);}

/* Embedded build cache uses exactly the same identity, length and CRC guards. */
int ta_vs_start_read_memory(const uint8_t*b,size_t bytes,const uint8_t hash[32],unsigned expected,TaVsStart*t,TaVsPreview*p){
 if(!b||!hash||!t||!p||bytes<72||memcmp(b,"TAN11C01",8)||memcmp(b+8,hash,32))return -1;
 unsigned n=r32(b+40);if(!n||n>TA_START_CAPACITY||r32(b+44)!=expected||bytes!=72u+n+sizeof p->pixels)return -1;
 uint32_t sum=crc(~0u,b,68);sum=crc(sum,b+72,bytes-72);if(~sum!=r32(b+68))return -1;
 memcpy(t->data,b+72,n);memcpy(p->pixels,b+72+n,sizeof p->pixels);t->bytes=n;t->frame=expected;t->masterPending=r32(b+48);t->cycleDebt=(int64_t)((uint64_t)r32(b+52)|(uint64_t)r32(b+56)<<32);p->ready=1;return 0;
}
