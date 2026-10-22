#include "ta_vs_picture.h"
#include <string.h>
static unsigned r16(const uint8_t*p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t r32(const uint8_t*p){return ((uint32_t)r16(p)<<16)|r16(p+2);}
static void w16(uint8_t*p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void w32(uint8_t*p,uint32_t v){w16(p,v>>16);w16(p+2,v);}
static unsigned offset(unsigned tile){return (tile/14)*8*224+(tile%14)*16;}
#if defined(TA_VS_PREPARED_PICTURE) && TA_VS_PREPARED_PICTURE
static void pending(TaVsPictureTx*t,unsigned tile){if(!t->dirty[tile]){t->dirty[tile]=1;t->pendingCount++;}}
int ta_vs_picture_prepare(TaVsPictureTx*t,const uint32_t*rgb,unsigned stride,uint32_t revision){
 if(!t||!rgb||stride<224||stride>4096||!revision)return -1;
 if(t->preparedRevision==revision)return 0;
 for(unsigned tile=0;tile<TA_VS_PIC_TILES;tile++){
  unsigned o=offset(tile),sx=(tile%14)*16,sy=(tile/14)*8,changed=!t->valid[tile];
  if(t->fieldOnly&&(sx<16||sx>=208||sy<24||sy>=216))continue;
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<16;x++){
   unsigned at=o+y*224+x;uint32_t v=rgb[(sy+y)*stride+sx+x]&0xffffffu;
   changed|=v!=t->sent[at];t->sent[at]=v;
  }
  if(changed)pending(t,tile);
 }
 t->preparedRevision=revision;return 0;
}
size_t ta_vs_picture_next(TaVsPictureTx*t,uint32_t tick,uint8_t*out,size_t cap){
 if(!t||!t->preparedRevision||!tick||!out||cap<401||cap>1152)return 0;
 if(t->refreshTick!=tick){
  for(unsigned tile=0;tile<TA_VS_PIC_TILES;tile++)if(t->valid[tile]&&(uint32_t)(tick-t->stamp[tile])>=60)pending(t,tile);
  t->refreshTick=tick;
 }
 if(!t->pendingCount)return 0;
 size_t used=12;unsigned count=0;
 for(unsigned checked=0;checked<TA_VS_PIC_TILES&&t->pendingCount;checked++){
  unsigned tile=t->cursor,o=offset(tile);if(!t->dirty[tile]){t->cursor=(tile+1)%TA_VS_PIC_TILES;continue;}
  uint32_t colors[128];for(unsigned y=0;y<8;y++)memcpy(colors+y*16,t->sent+o+y*224,64);
  uint8_t packed[512];unsigned n=0;
  for(unsigned k=0;k<128;){unsigned run=1;while(k+run<128&&run<128&&colors[k+run]==colors[k])run++;packed[n++]=(uint8_t)run;packed[n++]=(uint8_t)colors[k];packed[n++]=(uint8_t)(colors[k]>>8);packed[n++]=(uint8_t)(colors[k]>>16);k+=run;}
  unsigned format=n<384?1:0,bytes=format?n:384;if(used+5+bytes>cap)break;
  w16(out+used,tile);out[used+2]=(uint8_t)format;w16(out+used+3,bytes);used+=5;
  if(format)memcpy(out+used,packed,bytes);else for(unsigned k=0;k<128;k++){out[used+k*3]=(uint8_t)colors[k];out[used+k*3+1]=(uint8_t)(colors[k]>>8);out[used+k*3+2]=(uint8_t)(colors[k]>>16);}
  used+=bytes;t->dirty[tile]=0;t->pendingCount--;t->valid[tile]=1;t->stamp[tile]=tick;t->cursor=(tile+1)%TA_VS_PIC_TILES;count++;
 }
 if(!count)return 0;memcpy(out,"PVS6",4);w32(out+4,tick);w16(out+8,count);w16(out+10,0);return used;
}
/* Legacy caller API retained; N7 frontend prepares only on a new RGB capture. */
size_t ta_vs_picture_packet(TaVsPictureTx*t,const uint32_t*rgb,unsigned stride,uint32_t frame,uint8_t*out,size_t cap){
 if(!out||cap<401||cap>1152)return 0;
 if(ta_vs_picture_prepare(t,rgb,stride,frame))return 0;return ta_vs_picture_next(t,frame,out,cap);
}
#else
size_t ta_vs_picture_packet(TaVsPictureTx*t,const uint32_t*rgb,unsigned stride,uint32_t frame,uint8_t*out,size_t cap){
 if(!t||!rgb||stride<224||stride>4096||!frame||!out||cap<401||cap>1152)return 0;
 size_t used=12;unsigned count=0;
 for(unsigned checked=0;checked<TA_VS_PIC_TILES;checked++){
  unsigned tile=t->cursor,o=offset(tile),sx=(tile%14)*16,sy=(tile/14)*8;uint32_t colors[128];unsigned changed=!t->valid[tile]||(uint32_t)(frame-t->stamp[tile])>=60;
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<16;x++){unsigned k=y*16+x;colors[k]=rgb[(sy+y)*stride+sx+x]&0xffffffu;if(colors[k]!=t->sent[o+y*224+x])changed=1;}
  if(!changed){t->cursor=(tile+1)%TA_VS_PIC_TILES;continue;}
  uint8_t packed[512];unsigned n=0;
  for(unsigned k=0;k<128;){unsigned run=1;while(k+run<128&&run<128&&colors[k+run]==colors[k])run++;packed[n++]=(uint8_t)run;packed[n++]=(uint8_t)colors[k];packed[n++]=(uint8_t)(colors[k]>>8);packed[n++]=(uint8_t)(colors[k]>>16);k+=run;}
  unsigned format=n<384?1:0,bytes=format?n:384;if(used+5+bytes>cap)break;
  w16(out+used,tile);out[used+2]=(uint8_t)format;w16(out+used+3,bytes);used+=5;
  if(format)memcpy(out+used,packed,bytes);else for(unsigned k=0;k<128;k++){out[used+k*3]=(uint8_t)colors[k];out[used+k*3+1]=(uint8_t)(colors[k]>>8);out[used+k*3+2]=(uint8_t)(colors[k]>>16);}
  used+=bytes;for(unsigned y=0;y<8;y++)memcpy(t->sent+o+y*224,colors+y*16,64);t->valid[tile]=1;t->stamp[tile]=frame;t->cursor=(tile+1)%TA_VS_PIC_TILES;count++;
 }
 if(!count)return 0;memcpy(out,"PVS6",4);w32(out+4,frame);w16(out+8,count);w16(out+10,0);return used;
}
#endif
static int tile_decode(const uint8_t*p,unsigned n,unsigned format,uint32_t*out){
 if(format==0){if(n!=384)return 0;for(unsigned k=0;k<128;k++)out[k]=p[k*3]|((uint32_t)p[k*3+1]<<8)|((uint32_t)p[k*3+2]<<16);return 1;}
 if(format!=1||!n||(n&3)||n>512)return 0;unsigned k=0;for(unsigned a=0;a<n;a+=4){unsigned run=p[a];if(!run||run>128-k)return 0;uint32_t v=p[a+1]|((uint32_t)p[a+2]<<8)|((uint32_t)p[a+3]<<16);while(run--)out[k++]=v;}return k==128;
}
int ta_vs_picture_receive(TaVsPictureRx*r,const uint8_t*p,size_t n){
 if(!r||!p||n<12||n>1152||memcmp(p,"PVS6",4)||!r32(p+4)||r16(p+10))return -1;
 unsigned count=r16(p+8);if(!count||count>TA_VS_PIC_TILES)return -1;size_t a=12;uint8_t seen[TA_VS_PIC_TILES]={0};uint32_t colors[128];
 /* Validate every record before touching the displayed image. */
 for(unsigned k=0;k<count;k++){if(a+5>n)return -1;unsigned tile=r16(p+a),bytes=r16(p+a+3),format=p[a+2];a+=5;if(tile>=TA_VS_PIC_TILES||seen[tile]||bytes>n-a||!tile_decode(p+a,bytes,format,colors))return -1;seen[tile]=1;a+=bytes;}if(a!=n)return -1;
 a=12;uint32_t frame=r32(p+4);for(unsigned k=0;k<count;k++){unsigned tile=r16(p+a),bytes=r16(p+a+3),format=p[a+2];a+=5;
  if(!r->valid[tile]||(int32_t)(frame-r->stamp[tile])>=0){tile_decode(p+a,bytes,format,colors);unsigned o=offset(tile);for(unsigned y=0;y<8;y++)memcpy(r->pixels+o+y*224,colors+y*16,64);r->stamp[tile]=frame;r->valid[tile]=1;r->updates++;}a+=bytes;
 }return 0;
}
