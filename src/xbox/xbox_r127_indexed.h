/* R127 NV2A palette lookup and subtraction, followed by R126 exact expansion.
 * No additional GPU pass. Failure leaves the R126 shader and CPU path available. */
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
#if defined(TA_R128_GPU_ATLAS_CACHE) && TA_R128_GPU_ATLAS_CACHE
#include "ta_r128_atlas.h"
static TaR128AtlasCache r128AtlasCache[3];
static unsigned long r128Calls,r128Noop,r128Tiles,r128OldTiles;
static void r128ResetCounts(void){r128Calls=r128Noop=r128Tiles=r128OldTiles=0;}
static bool r127Upload(unsigned slot,const TaR127Palette*pal,unsigned count){
 if(!count||count>TA_R127_PALETTES)return false;
 uint16_t dirty[1024];unsigned n=ta_r128_plan(r128AtlasCache+slot,pal,count,dirty);
#if !defined(TA_NO_LOG) || !TA_NO_LOG
 ++r128Calls;r128OldTiles+=(count+(count<32?1u:0u))*32;
#endif
 if(!n){R126_TALLY(r128Noop);return true;}
 r127Atlas[slot]->BlockUntilNotBusy();D3DLOCKED_RECT l;
 if(FAILED(r127Atlas[slot]->LockRect(0,&l,NULL,0)))return false;
 uint32_t tile[64] __attribute__((aligned(16)));
 for(unsigned i=0;i<n;i++){
  unsigned v=dirty[i]/32,t=dirty[i]%32;
  ta_r127_tile(pal+v,v,t,tile);
  unsigned offset=ta_r127_spread(t*8)|(ta_r127_spread(v*8)<<1);
  memcpy((uint32_t*)l.pBits+offset,tile,sizeof tile);
 }
 __asm__ volatile("sfence":::"memory");
 if(FAILED(r127Atlas[slot]->UnlockRect(0))){memset(r128AtlasCache[slot].valid,0,sizeof r128AtlasCache[slot].valid);return false;}
 ta_r128_commit(r128AtlasCache+slot,pal,dirty,n);
#if !defined(TA_NO_LOG) || !TA_NO_LOG
 r128Tiles+=n;
#endif
 return true;
}
extern "C" void TA_Video_R128Counts(unsigned long out[4]){out[0]=r128Calls;out[1]=r128Noop;out[2]=r128Tiles;out[3]=r128OldTiles;}
#else
static bool r127Upload(unsigned slot,const TaR127Palette*pal,unsigned count){
 if(!count||count>TA_R127_PALETTES)return false;
 r127Atlas[slot]->BlockUntilNotBusy();D3DLOCKED_RECT l;
 if(FAILED(r127Atlas[slot]->LockRect(0,&l,NULL,0)))return false;
 uint32_t tile[64] __attribute__((aligned(16)));
 for(unsigned k=0;k<count+(count<32?1u:0u);k++)for(unsigned t=0;t<32;t++){
  unsigned v=k<count?k:31;
  ta_r127_tile(pal+v,v,t,tile);
  unsigned offset=ta_r127_spread(t*8)|(ta_r127_spread(v*8)<<1);
  memcpy((uint32_t*)l.pBits+offset,tile,sizeof tile);
 }
 __asm__ volatile("sfence":::"memory");r127Atlas[slot]->UnlockRect(0);return true;
}
#endif
static bool r127QualifyPass(const TaR127Palette*pal){
 if(!r127Upload(0,pal,TA_R127_PALETTES))return false;
 D3DLOCKED_RECT l;if(FAILED(r126Main[0]->LockRect(0,&l,NULL,0)))return false;
 for(unsigned y=0;y<256;y++)for(unsigned x=0;x<256;x++)
  ((uint32_t*)((uint8_t*)l.pBits+y*l.Pitch))[x]=ta_r127_test_pixel(y*256+x);
 __asm__ volatile("sfence":::"memory");r126Main[0]->UnlockRect(0);
 r127UseIndexed=true;bool ok=r126MathPass(0,256);r127UseIndexed=false;
 if(!ok)return false;
 LPDIRECT3DSURFACE8 old=NULL,surface=NULL;D3DVIEWPORT8 viewport,small={0,0,256,256,0,1};dev->GetViewport(&viewport);
 if(FAILED(dev->GetRenderTarget(&old))||FAILED(r126Check->GetSurfaceLevel(0,&surface))){if(old)old->Release();return false;}
 ok=SUCCEEDED(dev->SetRenderTarget(surface,NULL));
 if(ok){dev->SetViewport(&small);r126BindExpand();ok=r126Quad(256);}
 r126Cleanup();dev->SetRenderTarget(old,NULL);dev->SetViewport(&viewport);old->Release();surface->Release();
 if(!ok)return false;r126Check->BlockUntilNotBusy();if(FAILED(r126Check->LockRect(0,&l,NULL,0)))return false;
 for(unsigned y=0;y<256&&ok;y++)for(unsigned x=0;x<256;x++){
  unsigned i=y*256+x;uint32_t want=ta_r127_decode(ta_r127_test_pixel(i),pal+((i>>8)&31));
  if((((uint32_t*)((uint8_t*)l.pBits+y*l.Pitch))[x]&0xffffffu)!=want){ok=false;break;}
 }
 r126Check->UnlockRect(0);return ok;
}
static bool r127Qualify(void){
 static TaR127Palette pal[TA_R127_PALETTES];
 for(unsigned i=0;i<TA_R127_PALETTES;i++)ta_r127_test_palette(i,pal+i);
 if(!r127QualifyPass(pal))return false;
#if defined(TA_R128_GPU_ATLAS_CACHE) && TA_R128_GPU_ATLAS_CACHE
 /* Exercise a genuine no-write reuse, then a 33-tile partial update, on NV2A. */
 if(!r127QualifyPass(pal))return false;
 for(unsigned v=0;v<32;v++)for(unsigned x=8;x<16;x++)pal[v].color[x]^=0x00080808u;
 for(unsigned x=240;x<248;x++)pal[31].color[x]^=0x00a0d0b8u;
 if(!r127QualifyPass(pal))return false;
#endif
 return true;
}
static bool r127Init(void){
#if defined(TA_R128_GPU_ATLAS_CACHE) && TA_R128_GPU_ATLAS_CACHE
 memset(r128AtlasCache,0,sizeof r128AtlasCache);
#endif
 for(unsigned i=0;i<3;i++)if(FAILED(dev->CreateTexture(256,256,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&r127Atlas[i])))return false;
 D3DPIXELSHADERDEF ps;r126Shader(&ps,false);
 ps.PSTextureModes=PS_TEXTUREMODES(PS_TEXTUREMODES_PROJECT2D,PS_TEXTUREMODES_DPNDNT_AR,PS_TEXTUREMODES_DPNDNT_GB,PS_TEXTUREMODES_NONE);
 ps.PSRGBInputs[0]=PS_COMBINERINPUTS(PS_REGISTER_T1,PS_REGISTER_ONE,PS_REGISTER_T2,PS_REGISTER_NEGATIVE_ONE);
 return SUCCEEDED(dev->CreatePixelShader(&ps,&r127IndexedPS))&&r127Qualify();
}
#endif
