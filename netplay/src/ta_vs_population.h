#ifndef TA_VS_POPULATION_H
#define TA_VS_POPULATION_H
#include <stdint.h>
#include <stdio.h>
#define font_rows ta_population_font
#include "ta_vs_font5x7.h"
#undef font_rows
typedef struct {unsigned accounts,ingame,online,known,revision;}TaPopulation;
static inline void ta_population_text(uint32_t*out,unsigned stride,unsigned width,unsigned height,int x,int y,const char*text,uint32_t color,unsigned scale){
 for(unsigned k=0;text[k];k++){const uint8_t*g=ta_population_font(text[k]);for(unsigned yy=0;yy<7;yy++)for(unsigned xx=0;xx<5;xx++)if(g[yy]&(1u<<(4-xx)))for(unsigned dy=0;dy<scale;dy++)for(unsigned dx=0;dx<scale;dx++){
  int px=x+(int)((k*6+xx)*scale+dx),py=y+(int)(yy*scale+dy);if(px>=0&&py>=0&&(unsigned)px<width&&(unsigned)py<height)out[py*stride+px]=color;
 }}
}
static inline unsigned ta_population_image(uint32_t*out,unsigned stride,const TaPopulation*p,unsigned menu){
 char text[96];if(p->known){if(menu)snprintf(text,sizeof text,"ACCOUNTS %u   IN-GAME %u   ONLINE %u",p->accounts,p->ingame,p->online);else snprintf(text,sizeof text,"IN-GAME %u   ONLINE %u",p->ingame,p->online);}
 else snprintf(text,sizeof text,menu?"ACCOUNTS --   IN-GAME --   ONLINE --":"IN-GAME --   ONLINE --");
 unsigned w=0;while(text[w])w++;w=w*6+8;if(w>512)w=512;
 for(unsigned y=0;y<11;y++)for(unsigned x=0;x<w;x++)out[y*stride+x]=0x00101928;
 ta_population_text(out,stride,w,11,4,2,text,0x0096e3b2,1);return w;
}
#endif
