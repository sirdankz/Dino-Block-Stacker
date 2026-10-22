#ifndef TA_VS_PARTY_VIEW_H
#define TA_VS_PARTY_VIEW_H
#include "ta_vs_party.h"
#include <stdio.h>
#include <string.h>
#include "ta_vs_lan_screen.h"
/* Runtime Xbox path updates only this narrow names texture. Actual peer
   pictures are scaled by the GPU, never recomposed into a CPU canvas. */
static void ta_party_roster(uint32_t*out,const TaVsParty*p){
 for(unsigned y=0;y<224;y++)for(unsigned x=224;x<288;x++)out[y*512+x]=0;
 for(unsigned slot=0;slot<4;slot++)if(p->members&(1u<<slot)){
  unsigned color=slot==p->self?0x0096e3b2:!(p->alive&(1u<<slot))?0x00586070:slot==p->target?0x00f1cb72:0x00bdc7d5;
  char label[10];label[0]=(char)('1'+slot);label[1]=' ';unsigned k=0;for(;k<6&&p->names[slot][k];k++)label[k+2]=p->names[slot][k];label[k+2]=0;
  unsigned y=62+slot*27;if(slot==p->target)for(unsigned yy=y-3;yy<y+10;yy++)for(unsigned x=224;x<286;x++)out[yy*512+x]=0x003a5363;
  ta_vs_lan_text(out,227,(int)y,label,color,1);
 }
}
#define font_rows ta_party_font_rows
#include "ta_vs_font5x7.h"
#undef font_rows
static void ta_party_abbreviation(char out[4],const char*name){unsigned i=0;for(;i<3&&name[i];i++){char c=name[i];out[i]=c>='a'&&c<='z'?(char)(c-'a'+'A'):c;}out[i]=0;}
static void ta_party_labels(uint32_t*out,const TaVsParty*p){
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<96;x++)out[y*512+x]=0x00192840;
 unsigned next=ta_vs_party_next_target(p);
 for(unsigned seat=0;seat<4;seat++)if(p->members&(1u<<seat)){
  char label[16],abbr[4];ta_party_abbreviation(abbr,p->names[seat]);snprintf(label,sizeof label,"%s%s",abbr,seat==next&&seat!=p->self?" SEND JUNK":"");
  unsigned n=(unsigned)strlen(label),x=(96u-n*6u)/2u,y=seat*8u;
  uint32_t color=seat==p->self?0x0096e3b2:!(p->alive&(1u<<seat))?0x00708090:seat==next?0x00f1cb72:0x00f1f3f8;
  for(unsigned k=0;k<n;k++){const uint8_t*g=ta_party_font_rows(label[k]);for(unsigned yy=0;yy<7;yy++)for(unsigned xx=0;xx<5;xx++)if(g[yy]&(1u<<(4-xx)))out[(y+yy)*512+x+k*6+xx]=color;}
  if(seat==next&&seat!=p->self)for(unsigned xx=3;xx<93;xx++)out[(seat*8u+7u)*512+xx]=color;
 }
}
/* Software reference retained for pixel-preservation regression tests. */
static void ta_party_view(uint32_t*out,const TaVsParty*p,const TaVsPictureRx*rx){
 /* Clear unused layout space. Every visible board pixel comes from the
    corresponding player's original 224x224 ROM-rendered left-side image. */
 for(unsigned y=0;y<224;y++)for(unsigned x=224;x<512;x++)out[y*512+x]=0;
 unsigned count=ta_vs_party_count(p->members)-1,index=0;
 for(unsigned s=0;s<4;s++)if(p->members&(1u<<s)){
  unsigned color=s==p->self?0x0096e3b2:!(p->alive&(1u<<s))?0x00586070:s==p->target?0x00f1cb72:0x00bdc7d5;
  char label[10];label[0]=(char)('1'+s);label[1]=' ';unsigned k=0;for(;k<6&&p->names[s][k];k++)label[k+2]=p->names[s][k];label[k+2]=0;
  unsigned y=62+s*27;if(s==p->target){for(unsigned yy=y-3;yy<y+10;yy++)for(unsigned x=224;x<286;x++)out[yy*512+x]=0x003a5363;}ta_vs_lan_text(out,227,(int)y,label,color,1);
  if(s==p->self)continue;
  /* Two half-size full images stack without overlap. Three half-size full
     images form a 2+1 layout. Both retain the game's original board frames,
     textures, sprites and colors; no generated outlines or knockout tint. */
  unsigned size=count==1?224:112;
  unsigned x=count==1?288:count==2?344:288+(index%2)*112;
  unsigned y0=count==1?0:count==2?index*112:(index/2)*112;
  if(count==3&&index==2)x=344;
  for(unsigned yy=0;yy<size;yy++)for(unsigned xx=0;xx<size;xx++)out[(y0+yy)*512+x+xx]=rx[s].pixels[(yy*224/size)*224+xx*224/size];
  index++;
 }
}
#endif
