#ifndef TA_R129_PALETTE_H
#define TA_R129_PALETTE_H
#include "ta_r128_atlas.h"
/* Separate full-byte atlas: never share masked subtraction texture contents. */
static inline void ta_r129_tile(const TaR127Palette*p,unsigned palette,unsigned tile,uint32_t out[64]){
 for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
  uint32_t c=p->color[tile*8+x];if(!palette&&!y)c=0;
  out[ta_r127_spread(x)|(ta_r127_spread(y)<<1)]=c;
 }
}
static inline void ta_r129_test_palette(unsigned v,TaR127Palette*p){
 for(unsigned x=0;x<256;x++)p->color[x]=(x<<16)|((v*8+(x>>5))<<8)|((x*73+v*19)&255);
}
static inline uint32_t ta_r129_test_pixel(unsigned i){
 return ((i&255)<<24)|((8*((i>>8)&31)+1)<<16);
}
#endif
