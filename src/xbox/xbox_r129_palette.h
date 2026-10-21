/* R129 single-pass indexed palette presentation for Mode2 with no color math.
 * Full eight-bit atlas is independent of the R127/R128 subtraction atlas. */
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
#include "ta_r129_palette.h"
static LPDIRECT3DTEXTURE8 r129Atlas[3];
static TaR128AtlasCache r129Cache[3];
static DWORD r129PS;
static unsigned long r129Frames,r129Tiles,r129Noops;
static bool r129Upload(unsigned slot,const TaR127Palette*pal,unsigned count){
 if(!count||count>32)return false;
 uint16_t dirty[1024];unsigned n=ta_r128_plan(r129Cache+slot,pal,count,dirty);
 if(!n){R126_TALLY(r129Noops);return true;}
 r129Atlas[slot]->BlockUntilNotBusy();D3DLOCKED_RECT l;
 if(FAILED(r129Atlas[slot]->LockRect(0,&l,NULL,0)))return false;
 uint32_t tile[64] __attribute__((aligned(16)));
 for(unsigned i=0;i<n;i++){
  unsigned v=dirty[i]/32,t=dirty[i]%32;ta_r129_tile(pal+v,v,t,tile);
  unsigned offset=ta_r127_spread(t*8)|(ta_r127_spread(v*8)<<1);memcpy((uint32_t*)l.pBits+offset,tile,sizeof tile);
 }
 __asm__ volatile("sfence":::"memory");
 if(FAILED(r129Atlas[slot]->UnlockRect(0))){memset(r129Cache[slot].valid,0,sizeof r129Cache[slot].valid);return false;}
 ta_r128_commit(r129Cache+slot,pal,dirty,n);
#if !defined(TA_NO_LOG) || !TA_NO_LOG
 r129Tiles+=n;
#endif
 return true;
}
static void r129Bind(unsigned slot){dev->SetTexture(1,r129Atlas[slot]);dev->SetTexture(2,NULL);dev->SetPixelShader(r129PS);}
static bool r129QualifyPass(const TaR127Palette*pal){
 if(!r129Upload(0,pal,32))return false;
 D3DLOCKED_RECT l;if(FAILED(r126Main[0]->LockRect(0,&l,NULL,0)))return false;
 for(unsigned y=0;y<256;y++)for(unsigned x=0;x<256;x++)((uint32_t*)((uint8_t*)l.pBits+y*l.Pitch))[x]=ta_r129_test_pixel(y*256+x);
 __asm__ volatile("sfence":::"memory");r126Main[0]->UnlockRect(0);
 LPDIRECT3DSURFACE8 old=NULL,surface=NULL;D3DVIEWPORT8 viewport,small={0,0,256,256,0,1};dev->GetViewport(&viewport);
 if(FAILED(dev->GetRenderTarget(&old))||FAILED(r126Check->GetSurfaceLevel(0,&surface))){if(old)old->Release();return false;}
 bool ok=SUCCEEDED(dev->SetRenderTarget(surface,NULL));
 if(ok){dev->SetViewport(&small);dev->SetTexture(0,r126Main[0]);r129Bind(0);ok=r126Quad(256);}
 r126Cleanup();dev->SetRenderTarget(old,NULL);dev->SetViewport(&viewport);old->Release();surface->Release();
 if(!ok)return false;r126Check->BlockUntilNotBusy();if(FAILED(r126Check->LockRect(0,&l,NULL,0)))return false;
 for(unsigned y=0;y<256&&ok;y++)for(unsigned x=0;x<256;x++){
  unsigned i=y*256+x;uint32_t want=pal[(i>>8)&31].color[i&255];
  if((((uint32_t*)((uint8_t*)l.pBits+y*l.Pitch))[x]&0xffffffu)!=want){ok=false;break;}
 }
 r126Check->UnlockRect(0);return ok;
}
static bool r129Init(void){
 memset(r129Cache,0,sizeof r129Cache);r129Frames=r129Tiles=r129Noops=0;
 for(unsigned i=0;i<3;i++)if(FAILED(dev->CreateTexture(256,256,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&r129Atlas[i])))return false;
 D3DPIXELSHADERDEF ps;r126Shader(&ps,false);
 ps.PSTextureModes=PS_TEXTUREMODES(PS_TEXTUREMODES_PROJECT2D,PS_TEXTUREMODES_DPNDNT_AR,PS_TEXTUREMODES_NONE,PS_TEXTUREMODES_NONE);
 ps.PSRGBInputs[0]=PS_COMBINERINPUTS(PS_REGISTER_T1,PS_REGISTER_ONE,PS_REGISTER_ZERO,PS_REGISTER_ZERO);
 if(FAILED(dev->CreatePixelShader(&ps,&r129PS)))return false;
 static TaR127Palette pal[32];for(unsigned v=0;v<32;v++)ta_r129_test_palette(v,pal+v);
 if(!r129QualifyPass(pal)||!r129QualifyPass(pal))return false;
 for(unsigned v=0;v<32;v++)for(unsigned x=8;x<16;x++)pal[v].color[x]^=0x00a51f73;
 if(!r129QualifyPass(pal))return false;
 r129Tiles=r129Noops=0;return true;
}
extern "C" void TA_Video_R129Counts(unsigned long out[4]){out[0]=r129Frames;out[1]=r129GpuOK;out[2]=r129Tiles;out[3]=r129Noops;}
#endif
