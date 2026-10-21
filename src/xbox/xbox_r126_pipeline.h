/* R126 native output lifetime, rotating textures, exact NV2A color math.
 * GPU qualification is exhaustive per channel plus mixed-channel cases.
 * Any allocation/API/qualification failure keeps the CPU RGB path. */
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
static LPDIRECT3DTEXTURE8 r126Main[3],r126Sub[3],r126Math,r126Red,r126GB,r126Check;
static DWORD r126SubtractPS,r126ExpandPS;
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
static LPDIRECT3DTEXTURE8 r127Atlas[3];
static DWORD r127IndexedPS;
static bool r127GpuOK,r127UseIndexed;
static unsigned long r127IndexedFrames;
static bool r127Init(void);
#if defined(TA_R128_GPU_ATLAS_CACHE) && TA_R128_GPU_ATLAS_CACHE
static void r128ResetCounts(void);
#endif
#endif
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
static bool r129Init(void);
static bool r129GpuOK;
#endif
static unsigned r126Slot;
static bool r126Locked,r126SubLocked,r126GpuOK;
static unsigned long r126DirectFrames,r126GpuFrames,r126FallbackFrames;
#if !defined(TA_NO_LOG) || !TA_NO_LOG
#define R126_TALLY(value) (++(value))
#else
#define R126_TALLY(value) ((void)0)
#endif
static D3DLOCKED_RECT r126MainLock,r126SubLock;
static Ppu* r126Owner;
static bool r126Texture(LPDIRECT3DTEXTURE8*t,DWORD usage=0){return SUCCEEDED(dev->CreateTexture(256,256,1,usage,D3DFMT_LIN_A8R8G8B8,D3DPOOL_DEFAULT,t));}
static void r126Sampler(unsigned stage){
 dev->SetTextureStageState(stage,D3DTSS_MINFILTER,D3DTEXF_POINT);dev->SetTextureStageState(stage,D3DTSS_MAGFILTER,D3DTEXF_POINT);
 dev->SetTextureStageState(stage,D3DTSS_MIPFILTER,D3DTEXF_NONE);dev->SetTextureStageState(stage,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);dev->SetTextureStageState(stage,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);dev->SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,0);
}
static void r126Shader(D3DPIXELSHADERDEF*p,bool expand){
 memset(p,0,sizeof(*p));
 p->PSTextureModes=expand?PS_TEXTUREMODES(PS_TEXTUREMODES_PROJECT2D,PS_TEXTUREMODES_DPNDNT_AR,PS_TEXTUREMODES_DPNDNT_GB,PS_TEXTUREMODES_NONE):PS_TEXTUREMODES(PS_TEXTUREMODES_PROJECT2D,PS_TEXTUREMODES_PROJECT2D,PS_TEXTUREMODES_NONE,PS_TEXTUREMODES_NONE);
 p->PSInputTexture=PS_INPUTTEXTURE(0,0,0,0);
 p->PSCombinerCount=PS_COMBINERCOUNT(1,PS_COMBINERCOUNT_SAME_C0|PS_COMBINERCOUNT_SAME_C1);
 p->PSRGBInputs[0]=PS_COMBINERINPUTS(expand?PS_REGISTER_T1:PS_REGISTER_T0,PS_REGISTER_ONE,expand?PS_REGISTER_T2:PS_REGISTER_T1,expand?PS_REGISTER_ONE:PS_REGISTER_NEGATIVE_ONE);
 p->PSRGBOutputs[0]=PS_COMBINEROUTPUTS(PS_REGISTER_DISCARD,PS_REGISTER_DISCARD,PS_REGISTER_R0,PS_COMBINEROUTPUT_IDENTITY|PS_COMBINEROUTPUT_AB_CD_SUM);
 p->PSAlphaInputs[0]=PS_COMBINERINPUTS(PS_REGISTER_ZERO,PS_REGISTER_ZERO,PS_REGISTER_ZERO,PS_REGISTER_ZERO);
 p->PSAlphaOutputs[0]=PS_COMBINEROUTPUTS(PS_REGISTER_DISCARD,PS_REGISTER_DISCARD,PS_REGISTER_DISCARD,0);
 p->PSFinalCombinerInputsABCD=PS_COMBINERINPUTS(PS_REGISTER_ZERO,PS_REGISTER_ZERO,PS_REGISTER_ZERO,PS_REGISTER_R0);
 p->PSFinalCombinerInputsEFG=PS_COMBINERINPUTS(PS_REGISTER_ZERO,PS_REGISTER_ZERO,PS_REGISTER_ONE,0);
}
static void r126BindExpand(void){dev->SetTexture(0,r126Math);dev->SetTexture(1,r126Red);dev->SetTexture(2,r126GB);dev->SetPixelShader(r126ExpandPS);}
static void r126Cleanup(void){dev->SetPixelShader(0);dev->SetTexture(0,NULL);dev->SetTexture(1,NULL);dev->SetTexture(2,NULL);dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);dev->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);}
static bool r126Quad(unsigned h){
 V q[4]={{-0.5f,-0.5f,0,1,0,0},{255.5f,-0.5f,0,1,256,0},{-0.5f,(float)h-0.5f,0,1,0,(float)h},{255.5f,(float)h-0.5f,0,1,256,(float)h}};
 if(FAILED(dev->BeginScene()))return false;dev->SetVertexShader(FVF);HRESULT hr=dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(V));dev->EndScene();return SUCCEEDED(hr);
}
static bool r126MathPass(unsigned slot,unsigned h){
 LPDIRECT3DSURFACE8 old=NULL,surface=NULL;D3DVIEWPORT8 viewport,small={0,0,256,256,0,1};
 if(FAILED(dev->GetRenderTarget(&old)))return false;dev->GetViewport(&viewport);
 bool ok=SUCCEEDED(r126Math->GetSurfaceLevel(0,&surface));
 if(ok){dev->SetTexture(0,NULL);dev->SetTexture(1,NULL);dev->SetTexture(2,NULL);ok=SUCCEEDED(dev->SetRenderTarget(surface,NULL));}
 if(ok){dev->SetViewport(&small);dev->SetTexture(0,r126Main[slot]);dev->SetTexture(1,r126Sub[slot]);dev->SetPixelShader(r126SubtractPS);
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
 if(r127UseIndexed){dev->SetTexture(1,r127Atlas[slot]);dev->SetTexture(2,r127Atlas[slot]);dev->SetPixelShader(r127IndexedPS);}
#endif
 ok=r126Quad(h);}
 dev->SetTexture(0,NULL);dev->SetTexture(1,NULL);dev->SetRenderTarget(old,NULL);dev->SetViewport(&viewport);
 if(surface)surface->Release();old->Release();return ok;
}
#include "ta_r126_gpu_vectors.h"
static bool r126Qualify(void){
 D3DLOCKED_RECT a,b,c;if(FAILED(r126Main[0]->LockRect(0,&a,NULL,0)))return false;
 if(FAILED(r126Sub[0]->LockRect(0,&b,NULL,0))){r126Main[0]->UnlockRect(0);return false;}
 for(unsigned y=0;y<256;y++)for(unsigned x=0;x<256;x++){
  uint32_t m,s;ta_r126_gpu_vector((y<<8)|x,&m,&s);
  ((uint32_t*)((uint8_t*)a.pBits+y*a.Pitch))[x]=m;((uint32_t*)((uint8_t*)b.pBits+y*b.Pitch))[x]=s;
 }
 __asm__ volatile("sfence":::"memory");r126Main[0]->UnlockRect(0);r126Sub[0]->UnlockRect(0);
 if(!r126MathPass(0,256))return false;
 LPDIRECT3DSURFACE8 old=NULL,surface=NULL;D3DVIEWPORT8 viewport,small={0,0,256,256,0,1};dev->GetViewport(&viewport);
 if(FAILED(dev->GetRenderTarget(&old))||FAILED(r126Check->GetSurfaceLevel(0,&surface))){if(old)old->Release();return false;}
 dev->SetRenderTarget(surface,NULL);dev->SetViewport(&small);r126BindExpand();bool ok=r126Quad(256);r126Cleanup();dev->SetRenderTarget(old,NULL);dev->SetViewport(&viewport);old->Release();surface->Release();
 if(!ok)return false;r126Check->BlockUntilNotBusy();if(FAILED(r126Check->LockRect(0,&c,NULL,0)))return false;
 for(unsigned y=0;y<256&&ok;y++)for(unsigned x=0;x<256;x++)if((((uint32_t*)((uint8_t*)c.pBits+y*c.Pitch))[x]&0xffffffu)!=ta_r126_gpu_reference((y<<8)|x)){ok=false;break;}
 r126Check->UnlockRect(0);return ok;
}
static bool r126Init(void){
 for(unsigned i=0;i<3;i++)if(!r126Texture(&r126Main[i]))return false;
#if defined(TA_R126_GPU_SUBTRACT) && TA_R126_GPU_SUBTRACT
 bool ok=true;for(unsigned i=0;i<3;i++)ok=ok&&r126Texture(&r126Sub[i]);
 ok=ok&&r126Texture(&r126Math,D3DUSAGE_RENDERTARGET)&&r126Texture(&r126Check,D3DUSAGE_RENDERTARGET);
 if(ok){D3DPIXELSHADERDEF ps;r126Shader(&ps,false);ok=SUCCEEDED(dev->CreatePixelShader(&ps,&r126SubtractPS));r126Shader(&ps,true);ok=ok&&SUCCEEDED(dev->CreatePixelShader(&ps,&r126ExpandPS));}
 static uint32_t red[65536],gb[65536];
 if(ok){
  for(unsigned y=0;y<256;y++)for(unsigned x=0;x<256;x++){unsigned rr=y>>3,g=x>>3,b=y>>3;red[y*256+x]=((rr<<3)|(rr>>2))<<16;gb[y*256+x]=(((g<<3)|(g>>2))<<8)|((b<<3)|(b>>2));}
  ok=SUCCEEDED(dev->CreateTexture(256,256,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&r126Red))&&SUCCEEDED(dev->CreateTexture(256,256,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&r126GB));
  LPDIRECT3DTEXTURE8 lut[2]={r126Red,r126GB};uint32_t*data[2]={red,gb};
  for(unsigned i=0;i<2&&ok;i++){D3DLOCKED_RECT l;ok=SUCCEEDED(lut[i]->LockRect(0,&l,NULL,0));if(ok){XGSwizzleRect(data[i],0,NULL,l.pBits,256,256,NULL,4);__asm__ volatile("sfence":::"memory");lut[i]->UnlockRect(0);}}
 }
 for(unsigned i=0;i<3;i++)r126Sampler(i);
 r126GpuOK=ok&&r126Qualify();r126Cleanup();
 TA_Log("[TA R126 GPU QUALIFY] exact=%u vectors=65536 fallback=cpu_rgb\n",(unsigned)r126GpuOK);
#endif
 #if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
 r127GpuOK=r126GpuOK&&r127Init();r126Cleanup();
 TA_Log("[TA R127 GPU QUALIFY] exact=%u vectors=65536 fallback=r126_native_pipeline\n",(unsigned)r127GpuOK);
#if defined(TA_R128_GPU_ATLAS_CACHE) && TA_R128_GPU_ATLAS_CACHE
 TA_Log("[TA R128 GPU QUALIFY] exact=%u phases=3 vectors=196608 fallback=r126_native_pipeline\n",(unsigned)r127GpuOK);
 r128ResetCounts();
#endif
#endif
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
 r129GpuOK=r127GpuOK&&r129Init();r126Cleanup();
 TA_Log("[TA R129 GPU QUALIFY] exact=%u phases=3 vectors=196608 fallback=cpu_rgb\n",(unsigned)r129GpuOK);
#endif
 return true;
}
#include "xbox_r127_indexed.h"
#include "xbox_r129_palette.h"
extern "C" void TA_Video_BeginR126(Ppu*p){
 r126Locked=r126SubLocked=false;r126Owner=p;unsigned slot=r126Slot=(r126Slot+1)%3;
 if(!r126Main[slot])return;r126Main[slot]->BlockUntilNotBusy();dev->SetTexture(0,NULL);dev->SetTexture(1,NULL);
 if(FAILED(r126Main[slot]->LockRect(0,&r126MainLock,NULL,0)))return;
 r126Locked=true;ppu_r126Detach(p,(const uint8_t*)r126MainLock.pBits,256*r126MainLock.Pitch);
 if(r126GpuOK){r126Sub[slot]->BlockUntilNotBusy();if(SUCCEEDED(r126Sub[slot]->LockRect(0,&r126SubLock,NULL,0))){r126SubLocked=true;ppu_r126Detach(p,(const uint8_t*)r126SubLock.pBits,256*r126SubLock.Pitch);}}
 #if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
 ppu_r127SetIndexedTarget(p,r127GpuOK&&r126SubLocked,slot);
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
 ppu_r129SetDirectTarget(r129GpuOK&&r126SubLocked);
#endif
#endif
 ppu_r126SetGpuTarget(r126SubLocked?(uint8_t*)r126SubLock.pBits:NULL,r126SubLocked?(unsigned)r126SubLock.Pitch:0);
 if(!ppu_xboxBeginNativeTarget(p,(uint8_t*)r126MainLock.pBits,r126MainLock.Pitch)){
  ppu_xboxCancelNativeTarget();r126Main[slot]->UnlockRect(0);if(r126SubLocked)r126Sub[slot]->UnlockRect(0);r126Locked=r126SubLocked=false;
 }
}
extern "C" bool TA_Video_EndR126(Ppu*p){
 if(!r126Locked)return false;int w=0,h=0;bool ok=ppu_xboxFinishNativeTarget(p,&w,&h),gpu=ok&&ppu_r126GpuReady();
 __asm__ volatile("sfence":::"memory");r126Main[r126Slot]->UnlockRect(0);if(r126SubLocked)r126Sub[r126Slot]->UnlockRect(0);r126Locked=r126SubLocked=false;
 if(!ok){R126_TALLY(r126FallbackFrames);return false;}
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
 if(gpu&&ppu_r129DirectReady()){
  unsigned count;const TaR127Palette*pal=ppu_r127PaletteData(&count);
  if(!r129Upload(r126Slot,pal,count)){r129GpuOK=false;ppu_r126ResolveAll(p);R126_TALLY(r126FallbackFrames);return false;}
  r129Bind(r126Slot);s_lastAfterUpload=GetTickCount();draw(r126Main[r126Slot],true,true,256,true);r126Cleanup();
  R126_TALLY(r129Frames);R126_TALLY(r126GpuFrames);return true;
 }
#endif
 if(gpu){
  #if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
  r127UseIndexed=ppu_r127IndexedReady();
  if(r127UseIndexed){
   unsigned count;const TaR127Palette*pal=ppu_r127PaletteData(&count);
   if(!r127Upload(r126Slot,pal,count)){
    r127GpuOK=false;ppu_r126ResolveAll(p);R126_TALLY(r126FallbackFrames);return false;
   }
  }
#endif
  if(!r126MathPass(r126Slot,224)){r126GpuOK=false;ppu_r126ResolveAll(p);R126_TALLY(r126FallbackFrames);return false;}
  r126BindExpand();s_lastAfterUpload=GetTickCount();draw(r126Math,true,true,256,true);r126Cleanup();R126_TALLY(r126GpuFrames);
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
  if(r127UseIndexed)R126_TALLY(r127IndexedFrames);
#endif
 }else{s_lastAfterUpload=GetTickCount();draw(r126Main[r126Slot],true,true,256);R126_TALLY(r126DirectFrames);}
 return true;
}
extern "C" void TA_Video_R126Counts(unsigned long out[4]){out[0]=r126DirectFrames;out[1]=r126GpuFrames;out[2]=r126FallbackFrames;out[3]=r126GpuOK;}
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
extern "C" void TA_Video_R127Counts(unsigned long out[2]){out[0]=r127IndexedFrames;out[1]=r127GpuOK;}
#endif
static void r126Shutdown(void){
 if(r126Owner)ppu_r126ResolveAll(r126Owner);
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
 for(unsigned i=0;i<3;i++)if(r129Atlas[i])r129Atlas[i]->Release();
 if(r129PS)dev->DeletePixelShader(r129PS);
#endif
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
 for(unsigned i=0;i<3;i++)if(r127Atlas[i])r127Atlas[i]->Release();
 if(r127IndexedPS)dev->DeletePixelShader(r127IndexedPS);
#endif
 for(unsigned i=0;i<3;i++){if(r126Main[i])r126Main[i]->Release();if(r126Sub[i])r126Sub[i]->Release();}
 if(r126Math)r126Math->Release();if(r126Red)r126Red->Release();if(r126GB)r126GB->Release();if(r126Check)r126Check->Release();
 if(r126SubtractPS)dev->DeletePixelShader(r126SubtractPS);if(r126ExpandPS)dev->DeletePixelShader(r126ExpandPS);
}
#else
extern "C" void TA_Video_BeginR126(Ppu*p){(void)p;}
extern "C" bool TA_Video_EndR126(Ppu*p){(void)p;return false;}
extern "C" void TA_Video_R126Counts(unsigned long out[4]){memset(out,0,4*sizeof(*out));}
#endif
