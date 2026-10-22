#include "ta_vs_mobile_view.h"
#include <assert.h>
#include <stdio.h>
static uint32_t source[512*224],output[512*384+2];
int main(void){for(unsigned y=0;y<224;y++)for(unsigned x=0;x<512;x++)source[y*512+x]=0x10000000u|(y<<12)|x;
 const unsigned rows[]={128,192,230,256,288,384};
 for(unsigned k=0;k<6;k++){unsigned h=rows[k];output[0]=output[512*h+1]=0xdeadbeef;ta_mobile_backdrop(output+1,512,h,source,512);
  assert(output[0]==0xdeadbeef&&output[512*h+1]==0xdeadbeef);
  for(unsigned i=0;i<512*h;i++){uint32_t pixel=output[i+1];unsigned x=pixel&0xfff,y=(pixel>>12)&0xfff;assert(x>=304&&x<496&&y>=24&&y<112);assert(pixel==source[y*512+x]);assert(pixel!=0);}
 }
 puts("Mobile backdrop tests passed: real scene pixels only, cover scaling and guarded bounds.");return 0;}
