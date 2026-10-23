#ifndef TA_VS_MOBILE_VIEW_H
#define TA_VS_MOBILE_VIEW_H
#include <stdint.h>
/* Extend real scene pixels from the local player's upper
   background. This rectangle is above its stationary opening panels. No
   generated graphics, extra emulation, filters or game-state writes. */
static inline void ta_mobile_backdrop(uint32_t*out,unsigned width,unsigned height,const uint32_t*frame,unsigned stride){
 if(!out||!frame||!width||!height||width>512||stride<512)return;
 unsigned cropH=(unsigned)((uint64_t)192u*7u*height/(12u*width));if(cropH<1)cropH=1;if(cropH>88)cropH=88;
 unsigned y0=24u+(88u-cropH)/2u,lookup[512];for(unsigned x=0;x<width;x++)lookup[x]=16u+x*192u/width;
 for(unsigned y=0;y<height;y++){const uint32_t*src=frame+(y0+y*cropH/height)*stride;for(unsigned x=0;x<width;x++)out[y*width+x]=src[lookup[x]];}
}
#endif
