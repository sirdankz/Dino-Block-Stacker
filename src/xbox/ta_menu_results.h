/* Shared result-only trophy and winner banner. No runtime image decoding. */
#ifndef TA_MENU_RESULTS_H
#define TA_MENU_RESULTS_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "ta_trophy_sprites.h"
#define font_rows ta_result_font
#include "ta_vs_font5x7.h"
#undef font_rows
/* A completed win has exactly one surviving seat; otherwise replay. */
static inline unsigned ta_result_winner_slot(unsigned alive){
 alive&=15u;if(!alive||(alive&(alive-1u)))return 4u;
 for(unsigned seat=0;seat<4u;seat++)if(alive&(1u<<seat))return seat;
 return 4u;
}
static inline void ta_result_trophy(uint32_t*out,unsigned now,int x,int y){
 unsigned phase=now%TA_TROPHY_LOOP_MS,frame=0;
 while(frame+1u<TA_TROPHY_FRAMES&&phase>=ta_trophy_frame_ends[frame])frame++;
 for(unsigned yy=0;yy<32u;yy++)for(unsigned xx=0;xx<32u;xx++){
  int px=x+(int)xx,py=y+(int)yy;if(px<36||px>=476||py<154||py>=384)continue;
  uint32_t c=ta_trophy_palette[ta_trophy_pixels[frame*1024u+yy*32u+xx]];unsigned a=c>>24;
  if(!a)continue;uint32_t*p=out+py*512+px;
  if(a==255u){*p=c&0xffffffu;continue;}
  unsigned k=255u-a,r=(((c>>16)&255u)*a+((*p>>16)&255u)*k+127u)/255u;
  unsigned g=(((c>>8)&255u)*a+((*p>>8)&255u)*k+127u)/255u;
  unsigned b=((c&255u)*a+(*p&255u)*k+127u)/255u;*p=(r<<16)|(g<<8)|b;
 }
}
static inline void ta_result_banner(uint32_t*out,const char*winner,unsigned y,unsigned now){
 char text[40];bool haveWinner=winner&&winner[0];
 if(haveWinner)snprintf(text,sizeof text,"WINNER: %.16s",winner);else snprintf(text,sizeof text,"REPLAYING ROUND");
 unsigned width=(unsigned)strlen(text)*12u-2u,total=width+(haveWinner?42u:0u);int x=(512-(int)total)/2;
 if(haveWinner){ta_result_trophy(out,now,x,(int)y);x+=42;}
 uint32_t ink=haveWinner?0x00f1cb72u:0x00bdc7d5u;
 for(unsigned k=0;text[k];k++){
  const uint8_t*g=ta_result_font(text[k]);
  for(unsigned yy=0;yy<14u;yy++)for(unsigned xx=0;xx<10u;xx++)if(g[yy/2u]&(1u<<(4u-xx/2u))){
   int px=x+(int)(k*12u+xx),py=(int)y+9+(int)yy;
   if(px>=36&&px<476&&py>=154&&py<384)out[py*512+px]=ink;
  }
 }
}
#endif
