/* Real ROM images are uploaded only when changed and displayed by GPU quads.
   No CPU rescaling, full-canvas clears, or texture-busy waits on this path. */
static const TaPartyVisual*partyVisual;
struct PartyTexture {LPDIRECT3DTEXTURE8 tex[3];const uint32_t*source;unsigned slot,revision;uint32_t lastUpload;bool have;};
static PartyTexture partyBoard[4],partyNames;
static unsigned partyUploadTurn;
#include "ta_vs_population.h"
static PartyTexture partyScenery,populationTexture;
static const uint32_t*partyScenerySource;
static TaPopulation population={0,0,0,0,1};
static uint32_t populationPixels[512*TA_POPULATION_MAX_HEIGHT];
static unsigned populationMenu=2,populationWidth;
extern "C" void TA_Video_SetPartyBackdrop(const uint32_t*p){if(!partyScenery.have)partyScenerySource=p;}
extern "C" void TA_Video_SetPopulation(unsigned a,unsigned g,unsigned o,unsigned known){if(population.accounts!=a||population.ingame!=g||population.online!=o||population.known!=known){population.accounts=a;population.ingame=g;population.online=o;population.known=known;++population.revision;}}

static unsigned long partyStats[4]; /* board uploads, names uploads, busy skips, copied bytes */
static unsigned partyPeers(void){unsigned n=0;if(partyVisual)for(unsigned s=0;s<4;s++)if(s!=partyVisual->self&&(partyVisual->members&(1u<<s)))n++;return n;}
static bool partyCreate(PartyTexture*t,unsigned width,unsigned rows=256){for(unsigned k=0;k<3;k++)if(!t->tex[k]&&FAILED(dev->CreateTexture(width,rows,1,0,D3DFMT_LIN_X8R8G8B8,D3DPOOL_DEFAULT,&t->tex[k])))return false;return true;}
static void partyInvalidate(PartyTexture*t){t->source=NULL;t->revision=~0u;t->lastUpload=0;t->have=false;}
extern "C" bool TA_Video_SetPartyVisual(const TaPartyVisual*p){
 if(!p){partyVisual=NULL;partyScenerySource=NULL;partyInvalidate(&partyScenery);for(unsigned s=0;s<4;s++)partyInvalidate(partyBoard+s);partyInvalidate(&partyNames);return true;}
 if(p->self>3||p->members>15||!(p->members&(1u<<p->self))||!p->roster)return false;
 if(!partyCreate(&partyNames,128,32))return false;
 for(unsigned s=0;s<4;s++)if(s!=p->self&&(p->members&(1u<<s))){if(!p->pixels[s]||!partyCreate(partyBoard+s,256))return false;}
 if(!partyVisual)TA_Log("[TA R136 VIDEO] width=%d height=%d widescreen=%u multi_remote_fps_cap=8\n",width,height,(unsigned)displayWidescreen);partyVisual=p;return true;
}
extern "C" bool TA_Video_SetPartyOverlay(const uint32_t*p,unsigned){return !p?TA_Video_SetPartyVisual(NULL):false;}
extern "C" void TA_Video_PartyStats(unsigned long out[4]){for(unsigned k=0;k<4;k++){out[k]=partyStats[k];partyStats[k]=0;}}
static bool partyUpload(PartyTexture*t,const uint32_t*image,unsigned stride,unsigned x,unsigned columns,unsigned revision,uint32_t now,unsigned interval,unsigned kind,unsigned rows=224,unsigned sourceY=0){
 if(t->source!=image){partyInvalidate(t);t->source=image;}
 if(!ta_party_upload_due(t->have,t->revision,revision,now,t->lastUpload,interval))return false;
 dev->SetTexture(0,NULL);unsigned slot=3;
 for(unsigned k=1;k<=3;k++){unsigned candidate=(t->slot+k)%3;if(!t->tex[candidate]->IsBusy()){slot=candidate;break;}}
 if(slot==3){partyStats[2]++;return false;} /* Retain the last real image; never stall local gameplay. */
 D3DLOCKED_RECT l;if(FAILED(t->tex[slot]->LockRect(0,&l,NULL,0)))return false;
 if(!l.pBits||l.Pitch<(int)(columns*4)){t->tex[slot]->UnlockRect(0);return false;}
 int streamed=0;for(unsigned y=0;y<rows;y++)streamed|=ta_video_copy_row((uint8_t*)l.pBits+y*l.Pitch,(const uint8_t*)(image+(sourceY+y)*stride+x),columns*4);
 ta_video_copy_fence(streamed);t->tex[slot]->UnlockRect(0);t->slot=slot;t->revision=revision;t->lastUpload=now;t->have=true;partyStats[kind]++;partyStats[3]+=columns*rows*4;return true;
}
static void partyQuad(PartyTexture*t,TaPartyRect p,unsigned sourceWidth,unsigned sourceH=224,unsigned sourceY=0){
 if(!t->have)return;dev->SetTexture(0,t->tex[t->slot]);
 V q[4]={{p.left-0.5f,p.top-0.5f,0,1,0,(float)sourceY},{p.right-0.5f,p.top-0.5f,0,1,(float)sourceWidth,(float)sourceY},{p.left-0.5f,p.bottom-0.5f,0,1,0,(float)(sourceY+sourceH)},{p.right-0.5f,p.bottom-0.5f,0,1,(float)sourceWidth,(float)(sourceY+sourceH)}};
 dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(V));
}
static void partyBackdrop(TaPartyLayout layout){
 if(partyScenerySource&&!partyScenery.have&&partyCreate(&partyScenery,256,128))partyUpload(&partyScenery,partyScenerySource,512,16,192,1,GetTickCount(),0,0,88,24);
 if(!partyScenery.have)return;
 /* Four pieces of one cover-scaled original scene surround the local board.
    Drawing after the local shader avoids restoring its multi-texture state. */
 float pa=(displayWidescreen?16.0f/9.0f:4.0f/3.0f)*(float)height/(float)width;
 float sw=192,sh=88,screenAspect=(float)width*pa/(float)height,sourceAspect=(192.0f*7.0f/12.0f)/88.0f;
 if(screenAspect>sourceAspect)sh=192.0f*(7.0f/12.0f)/screenAspect;else sw=88.0f*screenAspect/(7.0f/12.0f);
 float sx=(192-sw)*.5f,sy=(88-sh)*.5f;
 TaPartyRect a[4]={{0,0,layout.local.left,(float)height},{layout.local.right,0,(float)width,(float)height},{layout.local.left,0,layout.local.right,layout.local.top},{layout.local.left,layout.local.bottom,layout.local.right,(float)height}};
 dev->SetTexture(0,partyScenery.tex[partyScenery.slot]);
 for(unsigned i=0;i<4;i++){TaPartyRect b=a[i];float u0=sx+b.left*sw/width,u1=sx+b.right*sw/width,v0=sy+b.top*sh/height,v1=sy+b.bottom*sh/height;
 V q[4]={{b.left-.5f,b.top-.5f,0,1,u0,v0},{b.right-.5f,b.top-.5f,0,1,u1,v0},{b.left-.5f,b.bottom-.5f,0,1,u0,v1},{b.right-.5f,b.bottom-.5f,0,1,u1,v1}};dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(V));}
}
static void populationOverlay(bool menu){
 if(!partyCreate(&populationTexture,512,32))return;
 if(populationMenu!=(unsigned)menu||!populationTexture.have||populationTexture.revision!=population.revision){populationMenu=menu;populationWidth=ta_population_image(populationPixels,512,&population,menu);populationTexture.revision=~0u;}
 dev->SetPixelShader(0);for(unsigned stage=1;stage<4;stage++){dev->SetTexture(stage,NULL);dev->SetTextureStageState(stage,D3DTSS_COLOROP,D3DTOP_DISABLE);}
 dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);dev->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_DISABLE);dev->SetVertexShader(FVF);
 unsigned hudHeight=ta_population_height(menu);
 partyUpload(&populationTexture,populationPixels,512,0,populationWidth,population.revision,GetTickCount(),0,1,hudHeight);
 float pa=(displayWidescreen?16.0f/9.0f:4.0f/3.0f)*(float)height/(float)width,scale=(float)height/480.0f;
 TaPartyRect box;box.left=menu?((float)width-populationWidth*scale/pa)*.5f:(float)width*.98f-populationWidth*scale/pa;box.right=box.left+populationWidth*scale/pa;box.top=menu?6*scale:2*scale;box.bottom=box.top+hudHeight*scale;
 partyQuad(&populationTexture,box,populationWidth,hudHeight);
}
static void partyOverlay(void){
 if(!partyVisual)return;unsigned peers=partyPeers();TaPartyLayout layout=ta_party_layout(width,height,displayWidescreen,peers);uint32_t now=GetTickCount();
 dev->SetPixelShader(0);for(unsigned stage=1;stage<4;stage++){dev->SetTexture(stage,NULL);dev->SetTextureStageState(stage,D3DTSS_COLOROP,D3DTOP_DISABLE);}
 dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);dev->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_DISABLE);dev->SetVertexShader(FVF);
 partyBackdrop(layout);
 partyUpload(&partyNames,partyVisual->roster,512,0,96,partyVisual->rosterRevision,now,0,1,32);
 unsigned labelIndex=1;partyQuad(&partyNames,layout.label[0],96,8,partyVisual->self*8);
 for(unsigned seat=0;seat<4;seat++)if(seat!=partyVisual->self&&(partyVisual->members&(1u<<seat)))partyQuad(&partyNames,layout.label[labelIndex++],96,8,seat*8);
 /* Spread expensive copies across local frames. A slow/busy image does not
    consume the upload budget or prevent another opponent refreshing. */
 for(unsigned k=0;k<4;k++){unsigned s=(partyUploadTurn+k)%4;if(s==partyVisual->self||!(partyVisual->members&(1u<<s)))continue;
  if(partyUpload(partyBoard+s,partyVisual->pixels[s],224,0,224,partyVisual->revision[s],now,peers>1?125u:66u,0)){partyUploadTurn=(s+1)%4;break;}}
 unsigned index=0;for(unsigned s=0;s<4;s++)if(s!=partyVisual->self&&(partyVisual->members&(1u<<s)))partyQuad(partyBoard+s,layout.peer[index++],224);
}
static void partyShutdown(void){for(unsigned s=0;s<7;s++){PartyTexture*t=s<4?partyBoard+s:s==4?&partyNames:s==5?&partyScenery:&populationTexture;for(unsigned k=0;k<3;k++)if(t->tex[k]){t->tex[k]->Release();t->tex[k]=NULL;}partyInvalidate(t);}partyVisual=NULL;}
