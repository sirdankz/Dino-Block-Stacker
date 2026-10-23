#ifndef TA_VS_PARTY_LAYOUT_H
#define TA_VS_PARTY_LAYOUT_H
#include <stdint.h>
#include "ta_vs_view.h"

typedef struct { float left,top,right,bottom; } TaPartyRect;
typedef struct {
 TaPartyRect local,roster,peer[3],label[4];
 unsigned localSourceY;
 unsigned peers;
} TaPartyLayout;
/* Images remain the original complete 224x224 left-side ROM pictures.
   The label atlas uses a 512-pixel stride, 96x32 pixels at the origin. */
typedef struct {
 const uint32_t *pixels[4];
 unsigned revision[4];
 const uint32_t *roster;
 unsigned rosterRevision,members,self,mode;
} TaPartyVisual;

/* Revisions may change for every received tile; display uploads have a separate
   wall-clock budget. Unsigned elapsed time also handles the clock wrapping. */
static inline int ta_party_upload_due(unsigned have,unsigned previous,unsigned revision,
 uint32_t now,uint32_t last,unsigned interval){
 return revision&&revision!=previous&&(!have||(uint32_t)(now-last)>=interval);
}
static inline unsigned ta_party_capture_interval(unsigned players){return players>2?8u:4u;}

static inline TaPartyRect ta_party_header(TaPartyRect board,float h,float pa){
 TaPartyRect r;float w=h*12.0f/pa;if(w>board.right-board.left)w=board.right-board.left;
 r.left=(board.left+board.right-w)*0.5f;r.right=r.left+w;r.top=board.top-h;r.bottom=board.top;return r;
}
static inline TaPartyLayout ta_party_layout_ex(unsigned width,unsigned height,unsigned wide,unsigned peers,unsigned mobile){
 TaPartyLayout r={0};if(!width||!height||!peers)return r;if(peers>3)peers=3;r.peers=peers;
 float pa=mobile?1.0f:(wide?16.0f/9.0f:4.0f/3.0f)*(float)height/(float)width;
 float margin=(float)height*0.012f,gap=(float)height*0.008f,header=(float)height*0.025f;if(header<8.0f)header=8.0f;
 float gapX=gap/pa,marginX=margin/pa;
 float localH=(float)height-margin*2-header,aspect=(7.0f/12.0f)/pa,localAspect=aspect*(224.0f/208.0f);
 float localW=localH*localAspect;if(localW>(float)width*0.48f){localW=(float)width*0.48f;localH=localW/localAspect;}
 r.localSourceY=16;r.local.left=((float)width-localW)*0.5f;r.local.right=r.local.left+localW;
 r.local.top=((float)height-localH-header)*0.5f+header;r.local.bottom=r.local.top+localH;
 r.label[0]=ta_party_header(r.local,header,pa);float labelW=header*12.0f/pa;
 if(labelW<(float)width){r.label[0].left=((float)width-labelW)*0.5f;r.label[0].right=r.label[0].left+labelW;}
 float sideW=r.local.left-gapX-marginX;
 for(unsigned i=0;i<peers;i++){
  unsigned onLeft=peers>1&&(peers==2?i==0:i<2),stacked=peers==3&&onLeft;
  float cap=localH*(stacked?0.44f:peers==3?0.62f:0.82f),maxWidthH=sideW/aspect;if(cap>maxWidthH)cap=maxWidthH;
  if(stacked){float maxH=((float)height-2*margin-2*header-gap)*0.5f;if(cap>maxH)cap=maxH;}
  float w=cap*aspect,cx=onLeft?(marginX+sideW*0.5f):((float)width-marginX-sideW*0.5f);
  float groupH=(cap+header)*(stacked?2.0f:1.0f)+(stacked?gap:0.0f);
  float top=((float)height-groupH)*0.5f+header+(stacked?(cap+header+gap)*(float)i:0.0f);
  r.peer[i].left=cx-w*0.5f;r.peer[i].right=cx+w*0.5f;r.peer[i].top=top;r.peer[i].bottom=top+cap;
  r.label[i+1]=ta_party_header(r.peer[i],header,pa);float lw=header*12.0f/pa;if(lw>sideW)lw=sideW;
  r.label[i+1].left=cx-lw*0.5f;r.label[i+1].right=cx+lw*0.5f;
 }
 return r;
}

static inline TaPartyLayout ta_party_layout(unsigned w,unsigned h,unsigned wide,unsigned peers){return ta_party_layout_ex(w,h,wide,peers,0);}
#endif
