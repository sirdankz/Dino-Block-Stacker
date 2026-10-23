#ifndef TA_VS_POPULATION_H
#define TA_VS_POPULATION_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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
/* The artwork's tab interior spans source rows 12 through 25. */
#define TA_POPULATION_MENU_TOP 12u
#define TA_POPULATION_MENU_HEIGHT 14u
static inline unsigned ta_population_height(unsigned menu){return menu?TA_POPULATION_MENU_HEIGHT:11u;}
/* Keep every label legible even when the registered-account count grows. */
static inline void ta_population_count(char out[12],unsigned n){
 if(n>=1000000000u)snprintf(out,12,"%uB",n/1000000000u);
 else if(n>=1000000u)snprintf(out,12,"%uM",n/1000000u);
 else if(n>=10000u)snprintf(out,12,"%uK",n/1000u);
 else snprintf(out,12,"%u",n);
}
/* Smaller menu glyphs leave the dotted top-tab trim unobstructed. */
#define TA_POPULATION_MENU_GLYPH_WIDTH 8u
#define TA_POPULATION_MENU_GLYPH_HEIGHT 10u
#define TA_POPULATION_MENU_ADVANCE 10u
static inline void ta_population_menu_text(uint32_t*out,unsigned stride,unsigned width,unsigned height,unsigned x,unsigned y,const char*text){
 for(unsigned k=0;text[k];k++){
  const uint8_t*g=ta_population_font(text[k]);
  for(unsigned yy=0;yy<TA_POPULATION_MENU_GLYPH_HEIGHT;yy++)for(unsigned xx=0;xx<TA_POPULATION_MENU_GLYPH_WIDTH;xx++){
   unsigned px=x+k*TA_POPULATION_MENU_ADVANCE+xx,py=y+yy;
   if(px<width&&py<height&&(g[yy*7u/TA_POPULATION_MENU_GLYPH_HEIGHT]&(1u<<(4u-xx*5u/TA_POPULATION_MENU_GLYPH_WIDTH))))out[py*stride+px]=0xff96e3b2u;
  }
 }
}
static inline unsigned ta_population_image(uint32_t*out,unsigned stride,const TaPopulation*p,unsigned menu){
 char text[96],a[12]="--",g[12]="--",o[12]="--";
 if(p->known){ta_population_count(a,p->accounts);ta_population_count(g,p->ingame);ta_population_count(o,p->online);}
 if(menu){
  snprintf(text,sizeof text,"ACCTS %s IN-GAME %s ONLINE %s",a,g,o);
  if(strlen(text)*TA_POPULATION_MENU_ADVANCE+16u>352u)snprintf(text,sizeof text,"ACCTS %s GAME %s ON %s",a,g,o);
 }else snprintf(text,sizeof text,"IN-GAME %s   ONLINE %s",g,o);
 unsigned height=ta_population_height(menu),padding=menu?8u:4u,advance=menu?TA_POPULATION_MENU_ADVANCE:6u;
 unsigned w=(unsigned)strlen(text)*advance+2u*padding;if(w>512u)w=512u;
 /* ARGB glyphs only: pixels between letters leave underlying artwork intact. */
 for(unsigned y=0;y<height;y++)for(unsigned x=0;x<w;x++)out[y*stride+x]=0;
 if(menu)ta_population_menu_text(out,stride,w,height,padding,(height-TA_POPULATION_MENU_GLYPH_HEIGHT)/2u,text);
 else ta_population_text(out,stride,w,height,padding,2,text,0xff96e3b2u,1);
 return w;
}
#endif
