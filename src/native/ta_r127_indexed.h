/* R127 presentation-only palettes and exact indexed NV2A model.
 * Four-row replication keeps normalized dependent coordinates away from edges.
 * Coordinate zero is neutral; coordinate 253 selects the per-frame fixed-color list. */
#ifndef TA_R127_INDEXED_H
#define TA_R127_INDEXED_H
#include <stdint.h>
enum { TA_R127_PALETTES=32 };
typedef struct { uint32_t color[256],fixed; } TaR127Palette;
static inline unsigned ta_r127_spread(unsigned x){
 x=(x|(x<<4))&0x0f0fu;x=(x|(x<<2))&0x3333u;return (x|(x<<1))&0x5555u;
}
static inline uint32_t ta_r127_rgb(uint32_t a,uint32_t b){
 uint32_t out=0;
 for(unsigned k=0;k<3;k++){
  int c=(int)((a>>(8*k+3))&31u)-(int)((b>>(8*k+3))&31u);
  if(c<0)c=0;out|=(uint32_t)((c<<3)|(c>>2))<<(8*k);
 }
 return out;
}
static inline uint32_t ta_r127_decode(uint32_t packed,const TaR127Palette*p){
 unsigned sub=(packed>>8)&255u;
 unsigned row=packed&255u;
 const TaR127Palette*frame=p-(((packed>>16)&255u)/8);
 uint32_t b=!row?0:(row==253u?frame[31].color[sub]:p->color[sub]);
 return ta_r127_rgb(p->color[packed>>24],b);
}
/* 32 contiguous 256-byte writes per palette, avoiding scattered GPU writes. */
static inline void ta_r127_tile(const TaR127Palette*p,unsigned palette,
                                unsigned tile,uint32_t out[64]){
 for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
  unsigned i=tile*8+x;
  uint32_t c=p->color[i]&0xf8f8f8u;
  if(!palette&&!y)c=0;
  out[ta_r127_spread(x)|(ta_r127_spread(y)<<1)]=c;
 }
}
static inline void ta_r127_test_palette(unsigned v,TaR127Palette*p){
 for(unsigned x=0;x<256;x++)
  { unsigned c=(((v<<8)|x)*109u+37u)&32767u;
    p->color[x]=((c&31u)<<19)|(((c>>5)&31u)<<11)|(((c>>10)&31u)<<3); }
 p->color[255]=0xf8f8f8u;
 p->fixed=(((v*3)&31u)<<19)|(((v*9)&31u)<<11)|(((v*19)&31u)<<3);
}
static inline uint32_t ta_r127_test_pixel(unsigned i){
 unsigned phase=i>>13,v=(i>>8)&31u,x=i&255u;
 unsigned m=(phase==1||phase==2)?255:x,s=(phase==1||phase==2)?x:(phase*37+x*17)&255u;
 unsigned row=phase==0?0:(phase==2?253:v*8+1);
 return (m<<24)|((v*8+1)<<16)|(s<<8)|row;
}
#endif
