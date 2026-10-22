#include "ta_vs_party_layout.h"
#include <assert.h>
#include <stdio.h>
static float absf(float x){return x<0?-x:x;}
static int overlap(TaPartyRect a,TaPartyRect b){return a.left<b.right-.001f&&b.left<a.right-.001f&&a.top<b.bottom-.001f&&b.top<a.bottom-.001f;}
static void check(unsigned w,unsigned h,unsigned wide,unsigned peers,unsigned mobile){
 TaPartyLayout l=ta_party_layout_ex(w,h,wide,peers,mobile);TaPartyRect a[8];unsigned n=0;a[n++]=l.local;for(unsigned i=0;i<peers;i++)a[n++]=l.peer[i];for(unsigned i=0;i<=peers;i++)a[n++]=l.label[i];
 float pa=mobile?1.0f:(wide?16.f/9.f:4.f/3.f)*(float)h/w;
 for(unsigned i=0;i<n;i++){assert(a[i].left>=0&&a[i].top>=0&&a[i].right<=(float)w+.001f&&a[i].bottom<=(float)h+.001f);assert(a[i].right>a[i].left&&a[i].bottom>a[i].top);for(unsigned j=0;j<i;j++)assert(!overlap(a[i],a[j]));
  if(i<=peers){float expected=7.f/12.f*(i==0?224.f/(224.f-l.localSourceY):1.f);assert(absf((a[i].right-a[i].left)*pa/(a[i].bottom-a[i].top)-expected)<.0001f);}}
 TaVsView v=ta_vs_view(w,h,wide);if(!mobile)assert(l.local.bottom-l.local.top>v.bottom-v.top);
 if(mobile){
  assert(absf((l.local.left+l.local.right)*.5f-(float)w*.5f)<.001f);
  for(unsigned i=0;i<=peers;i++){assert(l.label[i].bottom-l.label[i].top>=7.999f);assert(l.label[i].right-l.label[i].left>=95.999f);}
  for(unsigned i=0;i<peers;i++)assert(l.peer[i].right<=l.local.left+.001f||l.peer[i].left>=l.local.right-.001f);
 }
 assert(l.roster.right==0);assert(l.localSourceY==16u);
 assert(absf((l.local.left+l.local.right)*.5f-(float)w*.5f)<.001f);
 printf("%ux%u wide=%u mobile=%u players=%u local=%.2fx%.2f peer=%.2fx%.2f\n",w,h,wide,mobile,peers+1,l.local.right-l.local.left,l.local.bottom-l.local.top,l.peer[0].right-l.peer[0].left,l.peer[0].bottom-l.peer[0].top);
}
int main(void){const unsigned sizes[][2]={{640,480},{720,480},{1280,720},{1920,1080}};for(unsigned s=0;s<4;s++)for(unsigned wide=0;wide<2;wide++)for(unsigned p=1;p<=3;p++)check(sizes[s][0],sizes[s][1],wide,p,0);
 const unsigned rows[]={384,288,256,230,192};for(unsigned s=0;s<5;s++)for(unsigned p=1;p<=3;p++)check(512,rows[s],1,p,1);
 assert(!ta_party_layout(0,480,0,3).peers);assert(!ta_party_layout(640,0,0,3).peers);assert(!ta_party_layout(640,480,0,0).peers);assert(ta_party_layout(640,480,0,9).peers==3);puts("Party layout assertions passed (24 console + 15 mobile cases).");return 0;}
