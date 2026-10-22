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
#define TA_POPULATION_MAX_HEIGHT 22u
static inline unsigned ta_population_height(unsigned menu){return menu?22u:11u;}
/* Keep every label legible even when the registered-account count grows. */
static inline void ta_population_count(char out[12],unsigned n){
 if(n>=1000000u)snprintf(out,12,"%uM",n/1000000u);
 else if(n>=10000u)snprintf(out,12,"%uK",n/1000u);
 else snprintf(out,12,"%u",n);
}
static inline unsigned ta_population_image(uint32_t*out,unsigned stride,const TaPopulation*p,unsigned menu){
 char text[96],a[12],g[12],o[12];
 if(p->known){ta_population_count(a,p->accounts);ta_population_count(g,p->ingame);ta_population_count(o,p->online);
  if(menu)snprintf(text,sizeof text,"ACCOUNTS %s IN-GAME %s ONLINE %s",a,g,o);
  else snprintf(text,sizeof text,"IN-GAME %s   ONLINE %s",g,o);
 }else snprintf(text,sizeof text,menu?"ACCOUNTS -- IN-GAME -- ONLINE --":"IN-GAME --   ONLINE --");
 unsigned scale=menu?2u:1u,height=ta_population_height(menu),padding=4u*scale;
 unsigned w=0;while(text[w])w++;w=w*6u*scale+2u*padding;if(w>512)w=512;
 for(unsigned y=0;y<height;y++)for(unsigned x=0;x<w;x++)out[y*stride+x]=0x00101928;
 ta_population_text(out,stride,w,height,padding,2u*scale,text,0x0096e3b2,scale);return w;
}
#endif
