#include "ta_vs_board_view.h"
#include <string.h>
static const unsigned planes[4]={0x0d7c,0x0f7c,0x117c,0x137c};
static const unsigned cursor[4]={0x03a4,0x03a8,0x03ac,0x03b0};
static void put16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static uint16_t get16(const uint8_t*p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
int ta_vs_board_capture(const uint8_t*w,size_t n,unsigned player,uint32_t frame,uint8_t*out,size_t cap){
 if(!w||n!=TA_VS_BOARD_WRAM_BYTES||player>1||!out||cap<TA_VS_BOARD_VIEW_BYTES)return 0;
 uintptr_t a=(uintptr_t)w,b=(uintptr_t)out;
 if(b>=a?b-a<n:a-b<TA_VS_BOARD_VIEW_BYTES)return 0;
 out[0]=1;out[1]=(uint8_t)player;out[2]=out[3]=0;
 out[4]=(uint8_t)(frame>>24);out[5]=(uint8_t)(frame>>16);out[6]=(uint8_t)(frame>>8);out[7]=(uint8_t)frame;
 for(unsigned k=0;k<4;k++){unsigned a=cursor[k]+player*2;put16(out+8+k*2,(uint16_t)(w[a]|((uint16_t)w[a+1]<<8)));}
 for(unsigned k=0;k<4;k++)memcpy(out+16+k*256,w+planes[k]+player*256,256);
 return 1;
}
int ta_vs_board_decode(const uint8_t*p,size_t n,TaVsBoardView*out){
 if(!p||!out||n!=TA_VS_BOARD_VIEW_BYTES||p[0]!=1||p[1]>1||p[2]||p[3])return 0;
 out->sourcePlayer=p[1];out->frame=((uint32_t)get16(p+4)<<16)|get16(p+6);
 for(unsigned k=0;k<4;k++)out->cursorFields[k]=get16(p+8+k*2);
 for(unsigned k=0;k<4;k++)memcpy(out->plane[k],p+16+k*256,256);
 return 1;
}
