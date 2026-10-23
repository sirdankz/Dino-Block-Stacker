#include "ta_vs_preview.h"
int ta_vs_preview_capture_one(TaVsPreview*p,unsigned c,const uint32_t*s){
 if(!p||!s||c>=13)return -1;
#if defined(TA_VS_FAST_START) && TA_VS_FAST_START
 static const unsigned sx[13]={64,144,224,304,64,144,224,304,384,64,144,224,304};
#else
 static const unsigned sx[13]={64,144,224,304,48,128,208,288,368,64,144,224,304};
#endif
 static const unsigned sy[13]={24,24,24,24,72,72,72,72,72,168,168,168,168};
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++)p->pixels[c][y*32+x]=s[(sy[c]+y)*512+sx[c]+2*x];
 return 0;
}
int ta_vs_preview_capture(TaVsPreview*p,const uint32_t*s){
 if(!p||!s)return -1;for(unsigned c=0;c<13;c++)ta_vs_preview_capture_one(p,c,s);p->ready=1;return 0;
}
void ta_vs_preview_draw(const TaVsPreview*p,unsigned c,uint32_t*s,unsigned x,unsigned y){
 if(!p||!p->ready||c>=13||!s||x<2||y<2||x>414||y>382)return;
 for(unsigned k=0;k<100;k++){s[(y-2)*512+x-2+k]=s[(y+97)*512+x-2+k]=0x006fc9ff;s[(y-2+k)*512+x-2]=s[(y-2+k)*512+x+97]=0x006fc9ff;}
 for(unsigned yy=0;yy<96;yy++)for(unsigned xx=0;xx<96;xx++)s[(y+yy)*512+x+xx]=p->pixels[c][(yy/3)*32+xx/3];
}

void ta_vs_preview_draw_size(const TaVsPreview*p,unsigned c,uint32_t*s,unsigned x,unsigned y,unsigned size){
 if(!p||!p->ready||c>=13||!s||size<1||size>96||x<2||y<2||x+size+2>512||y+size+2>480)return;
 for(unsigned k=0;k<size+4;k++){s[(y-2)*512+x-2+k]=s[(y+size+1)*512+x-2+k]=0x006fc9ff;s[(y-2+k)*512+x-2]=s[(y-2+k)*512+x+size+1]=0x006fc9ff;}
 for(unsigned yy=0;yy<size;yy++)for(unsigned xx=0;xx<size;xx++)s[(y+yy)*512+x+xx]=p->pixels[c][(yy*32/size)*32+xx*32/size];
}
