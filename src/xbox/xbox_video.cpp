/* R6: guarded one-copy PPU -> linear D3D8 texture path; preserve R5 fallback. */
#include "ta_music.h"
#include "ta_platform.h"
extern "C" {
#include "ppu.h"
}
#include <xgraphics.h>
#if defined(TA_VS_NATIVE_VIEW) && TA_VS_NATIVE_VIEW
#include "ta_vs_view.h"
static bool displayWidescreen=false;
#endif
#if defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
#include "ta_video_copy.h"
#endif
static LPDIRECT3D8 d3d=NULL;
static LPDIRECT3DDEVICE8 dev=NULL;
static LPDIRECT3DTEXTURE8 tex=NULL;
static LPDIRECT3DTEXTURE8 activeTex=NULL;
static LPDIRECT3DTEXTURE8 nativeTex=NULL;
static unsigned long s_uploadMs=0, s_drawMs=0, s_presentMs=0, s_videoFrames=0;
static bool linear=false, activeLinear=false;
static unsigned long s_lastAfterUpload=0;
static int width=640,height=480;
#if defined(TA_R35_PROFILE) && TA_R35_PROFILE
static uint64_t s_lockTicks=0,s_copyTicks=0,s_unlockTicks=0;
#define UPLOAD_START() uint64_t uploadTick=__builtin_readcyclecounter()
#define UPLOAD_PART(counter) do {uint64_t now=__builtin_readcyclecounter();counter+=now-uploadTick;uploadTick=now;} while(0)
#else
#define UPLOAD_START() ((void)0)
#define UPLOAD_PART(counter) ((void)0)
#endif
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
static bool r126Init(void);
static void r126Shutdown(void);
#endif
struct V {float x,y,z,rhw,u,v;};
#define FVF (D3DFVF_XYZRHW|D3DFVF_TEX1)
extern "C" HRESULT TA_Video_Init(void){
  DWORD vf=XGetVideoFlags(),av=XGetAVPack();
  bool hd=(av==XC_AV_PACK_HDTV),wide=(vf&XC_VIDEO_FLAGS_WIDESCREEN)!=0;
  DWORD flags;
  if(hd&&(vf&XC_VIDEO_FLAGS_HDTV_720p)){width=1280;height=720;flags=D3DPRESENTFLAG_PROGRESSIVE|D3DPRESENTFLAG_WIDESCREEN;}
  else if(vf&XC_VIDEO_FLAGS_HDTV_480p){flags=D3DPRESENTFLAG_PROGRESSIVE|(wide?D3DPRESENTFLAG_WIDESCREEN:0);}
  else{flags=D3DPRESENTFLAG_INTERLACED|(wide?D3DPRESENTFLAG_WIDESCREEN:0);}
#if defined(TA_VS_NATIVE_VIEW) && TA_VS_NATIVE_VIEW
  displayWidescreen=(flags&D3DPRESENTFLAG_WIDESCREEN)!=0;
#endif
  d3d=Direct3DCreate8(D3D_SDK_VERSION);if(!d3d)return E_FAIL;
  D3DPRESENT_PARAMETERS p;memset(&p,0,sizeof(p));
  p.BackBufferWidth=width;p.BackBufferHeight=height;p.BackBufferFormat=D3DFMT_X8R8G8B8;
  p.BackBufferCount=1;p.MultiSampleType=D3DMULTISAMPLE_NONE;
  p.SwapEffect=D3DSWAPEFFECT_DISCARD;p.Windowed=FALSE;
  p.EnableAutoDepthStencil=FALSE;p.FullScreen_RefreshRateInHz=60;
  p.FullScreen_PresentationInterval=D3DPRESENT_INTERVAL_ONE;p.Flags=flags;
  HRESULT hr=d3d->CreateDevice(0,D3DDEVTYPE_HAL,NULL,D3DCREATE_HARDWARE_VERTEXPROCESSING,&p,&dev);
  if(FAILED(hr))return hr;
  dev->SetRenderState(D3DRS_LIGHTING,FALSE);dev->SetRenderState(D3DRS_ZENABLE,FALSE);
  dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
  dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
  dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
  dev->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
  dev->SetTextureStageState(0,D3DTSS_MINFILTER,D3DTEXF_POINT);
  dev->SetTextureStageState(0,D3DTSS_MAGFILTER,D3DTEXF_POINT);
  dev->SetTextureStageState(0,D3DTSS_MIPFILTER,D3DTEXF_NONE);
  dev->SetTextureStageState(0,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);
  dev->SetTextureStageState(0,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);
  hr=dev->CreateTexture(512,480,1,0,D3DFMT_LIN_X8R8G8B8,D3DPOOL_DEFAULT,&tex);
  if(SUCCEEDED(hr))linear=true;
  else {hr=dev->CreateTexture(512,512,1,0,D3DFMT_X8R8G8B8,D3DPOOL_DEFAULT,&tex);linear=false;}
  if(FAILED(hr))return hr; // The R4 fallback is always available.
  hr=dev->CreateTexture(512,256,1,0,D3DFMT_LIN_X8R8G8B8,D3DPOOL_DEFAULT,&activeTex);
  if(SUCCEEDED(hr))activeLinear=true;
  else {hr=dev->CreateTexture(512,256,1,0,D3DFMT_X8R8G8B8,D3DPOOL_DEFAULT,&activeTex);activeLinear=false;}
  if(FAILED(hr))activeTex=NULL; // Low-memory or unsupported formats: R4 mode.
#if defined(TA_R39_NATIVE256) && TA_R39_NATIVE256
  hr=dev->CreateTexture(256,256,1,0,D3DFMT_LIN_X8R8G8B8,D3DPOOL_DEFAULT,&nativeTex);
  if(FAILED(hr))nativeTex=NULL; // Existing 512-wide paths remain available.
#endif
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  r126Init(); // failure keeps the complete old upload/CPU renderer available
#endif
  return S_OK;
}
extern "C" bool TA_Video_ActiveAvailable(void){return dev && activeTex;}
static bool upload(LPDIRECT3DTEXTURE8 target, bool isLinear,
                   const unsigned char *pixels, int rows, int textureH){
  D3DLOCKED_RECT l;
  UPLOAD_START();
  dev->SetTexture(0,NULL);
  if(FAILED(target->LockRect(0,&l,NULL,0)))return false;
  UPLOAD_PART(s_lockTicks);
  if(isLinear){for(int y=0;y<rows;y++)memcpy((uint8_t*)l.pBits+y*l.Pitch,pixels+y*2048,2048);}
  else if(rows==224){
    static uint8_t paddedActive[512*256*4];
    memcpy(paddedActive,pixels,512*224*4);
    memset(paddedActive+512*224*4,0,512*32*4);
    XGSwizzleRect(paddedActive,0,NULL,l.pBits,512,256,NULL,4);
  } else {
    static uint8_t padded[512*512*4];
    memcpy(padded,pixels,512*480*4);
    memset(padded+512*480*4,0,512*32*4);
    XGSwizzleRect(padded,0,NULL,l.pBits,512,textureH,NULL,4);
  }
  UPLOAD_PART(s_copyTicks);
  target->UnlockRect(0);
  UPLOAD_PART(s_unlockTicks);
  return true;
}
#if defined(TA_VS_NATIVE_GAME) && TA_VS_NATIVE_GAME
#if defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
/* Opponent pixels only. No HUD canvas or CPU intermediate copy. Three small
   textures let the GPU finish its previous reads before a slot is reused. */
static LPDIRECT3DTEXTURE8 lanTex[3]={NULL,NULL,NULL};
static const uint32_t*lanImage=NULL;static unsigned lanRevision=0,lanUploaded=~0u,lanSlot=0;
extern "C" bool TA_Video_SetLanOverlay(const uint32_t*p,unsigned revision){
 if(p)for(unsigned k=0;k<3;k++)if(!lanTex[k]&&FAILED(dev->CreateTexture(256,256,1,0,D3DFMT_LIN_X8R8G8B8,D3DPOOL_DEFAULT,&lanTex[k])))return false;
 if(p!=lanImage)lanUploaded=~0u;lanImage=p;lanRevision=revision;return true;
}
static void lanOverlay(float x0,float x1){
 if(!lanImage)return;
 if(lanRevision!=lanUploaded){
  unsigned slot=(lanSlot+1)%3;LPDIRECT3DTEXTURE8 target=lanTex[slot];D3DLOCKED_RECT lock;
  dev->SetTexture(0,NULL);target->BlockUntilNotBusy();if(FAILED(target->LockRect(0,&lock,NULL,0)))return;
  int streamed=0;for(unsigned y=0;y<224;y++)streamed|=ta_video_copy_row((uint8_t*)lock.pBits+y*lock.Pitch,(const uint8_t*)(lanImage+y*224),224*4);
  ta_video_copy_fence(streamed);target->UnlockRect(0);lanSlot=slot;lanUploaded=lanRevision;
 }
 dev->SetPixelShader(0);for(unsigned stage=1;stage<4;stage++){dev->SetTexture(stage,NULL);dev->SetTextureStageState(stage,D3DTSS_COLOROP,D3DTOP_DISABLE);}
 dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);dev->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
 dev->SetTexture(0,lanTex[lanSlot]);dev->SetVertexShader(FVF);
#if defined(TA_VS_NATIVE_VIEW) && TA_VS_NATIVE_VIEW
 /* Original right-hand border/center artwork stay in the base image. Replace
    only its 6x12 interior with the peer's actual left-hand playfield. */
 TaVsViewField p=ta_vs_view_peer(ta_vs_view((unsigned)width,(unsigned)height,displayWidescreen));
 V q[4]={{p.left-0.5f,p.top-0.5f,0,1,p.u0,p.v0},{p.right-0.5f,p.top-0.5f,0,1,p.u1,p.v0},{p.left-0.5f,p.bottom-0.5f,0,1,p.u0,p.v1},{p.right-0.5f,p.bottom-0.5f,0,1,p.u1,p.v1}};
#else
 float left=x0+(x1-x0)*(288.0f/512.0f);V q[4]={{left,0,0,1,0,0},{x1,0,0,1,224,0},{left,(float)height,0,1,0,224},{x1,(float)height,0,1,224,224}};
#endif
 dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(V));
}
#else
static LPDIRECT3DTEXTURE8 lanTex=NULL;
static const uint32_t*lanImage=NULL;static unsigned lanRevision=0,lanUploaded=~0u;
extern "C" bool TA_Video_SetLanOverlay(const uint32_t*p,unsigned revision){
 if(p&&!lanTex&&FAILED(dev->CreateTexture(512,256,1,0,D3DFMT_LIN_X8R8G8B8,D3DPOOL_DEFAULT,&lanTex)))return false;
 if(p!=lanImage)lanUploaded=~0u;lanImage=p;lanRevision=revision;return true;
}
static void lanOverlay(float x0,float x1){
 if(!lanImage)return;
 if(!lanTex&&FAILED(dev->CreateTexture(512,256,1,0,D3DFMT_LIN_X8R8G8B8,D3DPOOL_DEFAULT,&lanTex)))return;
 if(lanRevision!=lanUploaded){D3DLOCKED_RECT lock;dev->SetTexture(0,NULL);
  if(FAILED(lanTex->LockRect(0,&lock,NULL,0)))return;
  for(unsigned y=0;y<224;y++)memcpy((uint8_t*)lock.pBits+y*lock.Pitch+224*4,lanImage+y*512+224,288*4);
  lanTex->UnlockRect(0);lanUploaded=lanRevision;
 }
 dev->SetPixelShader(0);for(unsigned stage=1;stage<4;stage++){dev->SetTexture(stage,NULL);dev->SetTextureStageState(stage,D3DTSS_COLOROP,D3DTOP_DISABLE);}
 dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);dev->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
 dev->SetTexture(0,lanTex);dev->SetVertexShader(FVF);
 float left=x0+(x1-x0)*(224.0f/512.0f);V q[4]={{left,0,0,1,224,0},{x1,0,0,1,512,0},{left,(float)height,0,1,224,224},{x1,(float)height,0,1,512,224}};
 dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(V));
}
#endif
#endif
#include "xbox_party_overlay.h"
static void draw(LPDIRECT3DTEXTURE8 target, bool isLinear, bool active, int textureW=512,bool customShader=false){
  if(!customShader)dev->SetPixelShader(0);
  float dh=(float)height,dw=dh*(4.0f/3.0f);if(dw>(float)width)dw=(float)width;
  float x0=((float)width-dw)*0.5f,x1=x0+dw;
  float u0=0,u1=isLinear?(float)textureW:1.0f;
  /* Menus use all 480 rows: cropping the top margin overlaps the population header. */
  float v0=0.0f;
  float v1=active?(isLinear?224.0f:224.0f/256.0f):
                  (isLinear?480.0f:480.0f/512.0f);
  float y0=0,y1=(float)height;
#if defined(TA_VS_NATIVE_VIEW) && TA_VS_NATIVE_VIEW
  if(lanImage||partyVisual){TaVsView v=ta_vs_view((unsigned)width,(unsigned)height,displayWidescreen);x0=v.left;x1=v.right;y0=v.top;y1=v.bottom;}
#endif
  V q[4]={{x0,y0,0,1,u0,v0},{x1,y0,0,1,u1,v0},
    {x0,y1,0,1,u0,v1},{x1,y1,0,1,u1,v1}};
#if defined(TA_VS_NATIVE_VIEW) && TA_VS_NATIVE_VIEW
  if(lanImage||partyVisual)for(unsigned k=0;k<4;k++){q[k].x-=0.5f;q[k].y-=0.5f;}
#elif defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
  if(lanImage||partyVisual){q[1].x=q[3].x=x0+dw*(224.0f/512.0f);q[1].u=q[3].u=u1*(224.0f/512.0f);}
#endif
  if(partyVisual){
    TaPartyLayout layout=ta_party_layout(width,height,displayWidescreen,partyPeers());
    q[0].x=q[2].x=layout.local.left-0.5f;q[1].x=q[3].x=layout.local.right-0.5f;
    q[0].y=q[1].y=layout.local.top-0.5f;q[2].y=q[3].y=layout.local.bottom-0.5f;
    q[1].u=q[3].u=u0+(u1-u0)*(224.0f/512.0f);
    q[0].v=q[1].v=v0+(v1-v0)*(layout.localSourceY/224.0f);
  }
  dev->Clear(0,NULL,D3DCLEAR_TARGET,D3DCOLOR_XRGB(0,0,0),1.0f,0);
  if(SUCCEEDED(dev->BeginScene())){dev->SetTexture(0,target);dev->SetVertexShader(FVF);
    dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(V));
#if defined(TA_VS_NATIVE_GAME) && TA_VS_NATIVE_GAME
    lanOverlay(x0,x1);partyOverlay();populationOverlay(!active);
#endif
    dev->EndScene();}
  unsigned long beforePresent=GetTickCount();
  s_drawMs+=(unsigned long)(beforePresent - s_lastAfterUpload);
  dev->Present(NULL,NULL,NULL,NULL);
  s_presentMs+=(unsigned long)(GetTickCount()-beforePresent);
  ++s_videoFrames;
}
#include "xbox_r126_pipeline.h"
#if defined(TA_VS_NATIVE_GAME) && TA_VS_NATIVE_GAME
extern "C" void TA_Video_ReleaseLanPpu(Ppu*p){
 if(r126Owner!=p)return;
 if(r126Locked){ppu_xboxCancelNativeTarget();r126Main[r126Slot]->UnlockRect(0);if(r126SubLocked)r126Sub[r126Slot]->UnlockRect(0);r126Locked=r126SubLocked=false;}
 ppu_r126ResolveAll(p);r126Owner=NULL;
}
#endif

extern "C" void TA_Video_Submit(const unsigned char *pixels){TA_Music_MenuFrame();
  if(!dev||!tex||!pixels)return;
  unsigned long start=GetTickCount();
  if(!upload(tex,linear,pixels,480,512))return;
  unsigned long afterUpload=GetTickCount();s_uploadMs+=(unsigned long)(afterUpload-start);
  s_lastAfterUpload=afterUpload;
  draw(tex,linear,false);
}
extern "C" bool TA_Video_SubmitNative224Direct(const Ppu* ppu){TA_Music_SetScene(TA_MUSIC_GAME);
  if(!dev || !nativeTex || !ppu_taCanCopyNative224(ppu))return false;
  unsigned long start=GetTickCount();
  UPLOAD_START();
  dev->SetTexture(0,NULL);
  D3DLOCKED_RECT l;
  if(FAILED(nativeTex->LockRect(0,&l,NULL,0)))return false;
  UPLOAD_PART(s_lockTicks);
  bool ok=false;
  if(l.pBits && l.Pitch>=1024 && l.Pitch<=16384){
    unsigned pitch=(unsigned)l.Pitch;
    ok=ppu_taCopyNative224Strided(ppu,(uint8_t*)l.pBits,pitch,223u*pitch+1024u);
  }
  UPLOAD_PART(s_copyTicks);
  nativeTex->UnlockRect(0);
  UPLOAD_PART(s_unlockTicks);
  if(!ok)return false;
  unsigned long afterUpload=GetTickCount();
  s_uploadMs+=(unsigned long)(afterUpload-start);s_lastAfterUpload=afterUpload;
  draw(nativeTex,true,true,256);
  return true;
}
extern "C" bool TA_Video_SubmitActive224Direct(const Ppu* ppu){TA_Music_SetScene(TA_MUSIC_GAME);
  /* D3D8 linear textures only. Swizzled textures continue to use R5's
     staging+XGSwizzleRect code; never directly write linear pixels to them. */
  if(!TA_Video_ActiveAvailable() || !activeLinear ||
     !ppu_taCanCopyActive224(ppu)) return false;
  unsigned long start=GetTickCount();
  UPLOAD_START();
  dev->SetTexture(0,NULL);
  D3DLOCKED_RECT l;
  if(FAILED(activeTex->LockRect(0,&l,NULL,0)))return false;
  UPLOAD_PART(s_lockTicks);
  bool ok=false;
  if(l.pBits && l.Pitch>=2048 && l.Pitch<=16384){
    unsigned pitch=(unsigned)l.Pitch;
    ok=ppu_taCopyActive224Strided(ppu,(uint8_t*)l.pBits,pitch,
                                 223u*pitch+2048u);
  }
  UPLOAD_PART(s_copyTicks);
  activeTex->UnlockRect(0);
  UPLOAD_PART(s_unlockTicks);
  if(!ok)return false;
  unsigned long afterUpload=GetTickCount();
  s_uploadMs+=(unsigned long)(afterUpload-start);
  s_lastAfterUpload=afterUpload;
  draw(activeTex,true,true);
  return true;
}

extern "C" bool TA_Video_SubmitActive224(const unsigned char *pixels){TA_Music_SetScene(TA_MUSIC_GAME);
  if(!TA_Video_ActiveAvailable()||!pixels)return false;
  unsigned long start=GetTickCount();
  if(!upload(activeTex,activeLinear,pixels,224,256))return false;
  unsigned long afterUpload=GetTickCount();s_uploadMs+=(unsigned long)(afterUpload-start);
  s_lastAfterUpload=afterUpload;
  draw(activeTex,activeLinear,true);
  return true;
}
extern "C" void TA_Video_ReadReset(unsigned long *upload_ms, unsigned long *draw_ms, unsigned long *present_ms, unsigned long *frames){
  if(upload_ms)*upload_ms=s_uploadMs;
  if(draw_ms)*draw_ms=s_drawMs;
  if(present_ms)*present_ms=s_presentMs;
  if(frames)*frames=s_videoFrames;
  s_uploadMs=s_drawMs=s_presentMs=s_videoFrames=0;
}
extern "C" void TA_Video_ReadResetUploadTicks(uint64_t ticks[3]){
#if defined(TA_R35_PROFILE) && TA_R35_PROFILE
  ticks[0]=s_lockTicks;ticks[1]=s_copyTicks;ticks[2]=s_unlockTicks;
  s_lockTicks=s_copyTicks=s_unlockTicks=0;
#else
  ticks[0]=ticks[1]=ticks[2]=0;
#endif
}
extern "C" void TA_Video_Shutdown(void){partyShutdown();
#if defined(TA_VS_NATIVE_GAME) && TA_VS_NATIVE_GAME
#if defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
  for(unsigned k=0;k<3;k++)if(lanTex[k]){lanTex[k]->Release();lanTex[k]=NULL;}lanImage=NULL;
#else
  if(lanTex){lanTex->Release();lanTex=NULL;}lanImage=NULL;
#endif
#endif
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  r126Shutdown();
#endif
  if(nativeTex){nativeTex->Release();nativeTex=NULL;}
  if(activeTex){activeTex->Release();activeTex=NULL;}
  if(tex){tex->Release();tex=NULL;}
  if(dev){dev->Release();dev=NULL;}
  if(d3d){d3d->Release();d3d=NULL;}
}
