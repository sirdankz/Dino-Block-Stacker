/* Rank artwork shared by the Original Xbox and Android menus. */
static unsigned eggTier(unsigned r){return r<1000?0:r<1200?1:r<1400?2:r<1600?3:r<1800?4:5;}
static const char*eggShort(unsigned r){static const char*n[]={"ROTTEN","BRONZE","SILVER","GOLD","PLAT","MASTER"};return n[eggTier(r)];}
#include "ta_egg_sprites34.h"
static void eggEmblem(int x,int y,unsigned rating,unsigned scale){
 unsigned t=eggTier(rating);
 for(int yy=0;yy<TA_EGG_EMBLEM_SIZE;yy++)for(int xx=0;xx<TA_EGG_EMBLEM_SIZE;xx++){
  uint32_t c=ta_egg_sprites34[t][yy*TA_EGG_EMBLEM_SIZE+xx];unsigned alpha=c>>24;if(!alpha)continue;
  for(unsigned dy=0;dy<scale;dy++)for(unsigned dx=0;dx<scale;dx++){
   int px=x+xx*(int)scale+dx,py=y+yy*(int)scale+dy;if(px<0||px>=512||py<0||py>=480)continue;
   uint32_t*b=&screen[py*512+px];if(alpha==255)*b=c&0xffffffu;
   else{unsigned keep=255-alpha;unsigned red=(((c>>16)&255)*alpha+((*b>>16)&255)*keep+127)/255,green=(((c>>8)&255)*alpha+((*b>>8)&255)*keep+127)/255,blue=((c&255)*alpha+(*b&255)*keep+127)/255;*b=(red<<16)|(green<<8)|blue;}
  }
 }
}
