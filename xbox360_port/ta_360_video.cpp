#include "ta_360_platform.h"
#include "ta_music.h"
#include <d3dx9.h>
extern "C" {
#include "ppu.h"
}
#include "ta_vs_mobile_view.h"
#include "ta_vs_population.h"
static IDirect3D9*d3d;
static IDirect3DDevice9*device;
static IDirect3DVertexShader9*vs;
static IDirect3DPixelShader9*ps;
static IDirect3DVertexDeclaration9*decl;
struct Image {IDirect3DTexture9*texture;unsigned width,height;};
static Image frames[3],remote[4],labels,scene,hud;
static unsigned frameIndex,turn;
static const uint32_t*opponent;
static const TaPartyVisual*party;
static unsigned have[4],revision[4],lastAt[4],rosterRevision=~0u;
static unsigned long stats[4];
static bool sceneValid;
static TaPopulation population;
static uint32_t frame[512*480],backdrop[512*288],hudPixels[512*TA_POPULATION_MAX_HEIGHT];
static HRESULT makeImage(Image&i,unsigned w,unsigned h){
 i.width=w;i.height=h;return device->CreateTexture(w,h,1,0,D3DFMT_LIN_A8R8G8B8,D3DPOOL_DEFAULT,&i.texture,NULL);
}
static bool upload(Image&i,const uint32_t*p,unsigned stride,unsigned w,unsigned h){
 D3DLOCKED_RECT locked;if(!i.texture||!p||w>i.width||h>i.height||FAILED(i.texture->LockRect(0,&locked,NULL,0)))return false;
 for(unsigned y=0;y<h;y++)memcpy((BYTE*)locked.pBits+y*locked.Pitch,p+y*stride,w*4);
 i.texture->UnlockRect(0);return true;
}
struct Vertex {float x,y,u,v;};
static void quad(Image&i,TaPartyRect r,float x,float y,float w,float h,bool alpha=false){
 if(!i.texture)return;
 float left=2*r.left/1280-1,right=2*r.right/1280-1,top=1-2*r.top/720,bottom=1-2*r.bottom/720;
 float u=x/i.width,v=y/i.height,ur=(x+w)/i.width,vb=(y+h)/i.height;
 Vertex vertices[6]={{left,top,u,v},{right,top,ur,v},{left,bottom,u,vb},{left,bottom,u,vb},{right,top,ur,v},{right,bottom,ur,vb}};
 float a[4]={alpha?1.0f:0.0f,alpha?0.0f:1.0f,0,0};device->SetPixelShaderConstantF(0,a,1);
 device->SetRenderState(D3DRS_ALPHABLENDENABLE,alpha);device->SetTexture(0,i.texture);device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,vertices,sizeof(Vertex));
}
static TaPartyRect rect(float x,float y,float w,float h){TaPartyRect r={x,y,x+w,y+h};return r;}
static void release(Image&i){if(i.texture){i.texture->Release();i.texture=NULL;}}
extern "C" HRESULT TA_Video_Init(void){
 d3d=Direct3DCreate9(D3D_SDK_VERSION);if(!d3d)return E_FAIL;
 D3DPRESENT_PARAMETERS pp;memset(&pp,0,sizeof pp);pp.BackBufferWidth=1280;pp.BackBufferHeight=720;pp.BackBufferFormat=D3DFMT_A8R8G8B8;pp.FrontBufferFormat=D3DFMT_LE_X8R8G8B8;pp.MultiSampleType=D3DMULTISAMPLE_NONE;pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.PresentationInterval=D3DPRESENT_INTERVAL_ONE;
 HRESULT hr=d3d->CreateDevice(0,D3DDEVTYPE_HAL,NULL,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device);if(FAILED(hr))return hr;
 const char*shader="struct V{float2 p:POSITION;float2 uv:TEXCOORD0;};struct O{float4 p:POSITION;float2 uv:TEXCOORD0;};O vertex(V v){O o;o.p=float4(v.p,0,1);o.uv=v.uv;return o;}sampler image:register(s0);float4 alpha:register(c0);float4 pixel(O v):COLOR{float4 c=tex2D(image,v.uv);return float4(c.rgb,c.a*alpha.x+alpha.y);}";
 ID3DXBuffer*code=NULL;ID3DXBuffer*errors=NULL;
 hr=D3DXCompileShader(shader,(UINT)strlen(shader),NULL,NULL,"vertex","vs_2_0",0,&code,&errors,NULL);if(errors)errors->Release();if(FAILED(hr))return hr;
 hr=device->CreateVertexShader((DWORD*)code->GetBufferPointer(),&vs);code->Release();if(FAILED(hr))return hr;
 errors=NULL;hr=D3DXCompileShader(shader,(UINT)strlen(shader),NULL,NULL,"pixel","ps_2_0",0,&code,&errors,NULL);if(errors)errors->Release();if(FAILED(hr))return hr;
 hr=device->CreatePixelShader((DWORD*)code->GetBufferPointer(),&ps);code->Release();if(FAILED(hr))return hr;
 const D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},{0,8,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
 hr=device->CreateVertexDeclaration(elements,&decl);if(FAILED(hr))return hr;
 for(unsigned i=0;i<3;i++){hr=makeImage(frames[i],512,512);if(FAILED(hr))return hr;}
 for(unsigned i=0;i<4;i++){hr=makeImage(remote[i],256,256);if(FAILED(hr))return hr;}
 if(FAILED(hr=makeImage(labels,128,32))||FAILED(hr=makeImage(scene,512,288))||FAILED(hr=makeImage(hud,512,32)))return hr;
 device->SetVertexDeclaration(decl);device->SetVertexShader(vs);device->SetPixelShader(ps);device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_HALFPIXELOFFSET,TRUE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
 device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
 device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
 return S_OK;
}
extern "C" void TA_Video_SetPopulation(unsigned a,unsigned g,unsigned o,unsigned known){population.accounts=a;population.ingame=g;population.online=o;population.known=known;}
extern "C" void TA_Video_SetDuelMode(unsigned){}
extern "C" void TA_Video_SetPartyBackdrop(const uint32_t*p){if(p&&!sceneValid){ta_mobile_backdrop(backdrop,512,288,p,512);sceneValid=upload(scene,backdrop,512,512,288);}}
extern "C" bool TA_Video_SetLanOverlay(const uint32_t*p,unsigned){if(opponent!=p)sceneValid=false;opponent=p;return true;}
extern "C" bool TA_Video_SetPartyVisual(const TaPartyVisual*p){
 if(!party||!p||party->self!=p->self){memset(have,0,sizeof have);memset(revision,0,sizeof revision);rosterRevision=~0u;sceneValid=false;}party=p;return true;
}
extern "C" bool TA_Video_SetPartyOverlay(const uint32_t*p,unsigned){if(!p){TA_Video_SetPartyVisual(NULL);return true;}return false;}
extern "C" void TA_Video_PartyStats(unsigned long out[4]){memcpy(out,stats,sizeof stats);memset(stats,0,sizeof stats);}
static void present(const uint32_t*p,unsigned height){
 if(!device)return;bool menu=height==480;memcpy(frame,p,512*height*4);
 bool multiple=party&&!menu;
 if(opponent&&!menu&&!multiple)for(unsigned y=24;y<216;y++)memcpy(frame+y*512+304,opponent+y*224+16,192*4);
 Image&current=frames[frameIndex++%3];if(!upload(current,frame,512,512,height))return;
 device->Clear(0,NULL,D3DCLEAR_TARGET,0xff14283c,1,0);
 TaPartyRect screen=rect(160,0,960,720);
 if(multiple){
  if(!sceneValid)TA_Video_SetPartyBackdrop(p);quad(scene,rect(0,0,1280,720),0,0,512,288);
  unsigned peers=0;for(unsigned s=0;s<4;s++)if(s!=party->self&&(party->members&(1u<<s)))peers++;
  TaPartyLayout layout=ta_party_layout(1280,720,1,peers);unsigned now=GetTickCount();
  /* Remote animation uploads have a separate budget from simulation and input. */
  for(unsigned k=0;k<4;k++){unsigned s=(turn+k)%4;if(s==party->self||!(party->members&(1u<<s)))continue;
   if(ta_party_upload_due(have[s],revision[s],party->revision[s],now,lastAt[s],peers>1?125:66)&&upload(remote[s],party->pixels[s],224,224,224)){
    have[s]=1;revision[s]=party->revision[s];lastAt[s]=now;turn=(s+1)%4;stats[0]++;stats[3]+=224*224*4;break;
   }
  }
  if(rosterRevision!=party->rosterRevision&&upload(labels,party->roster,512,96,32)){rosterRevision=party->rosterRevision;stats[1]++;stats[3]+=96*32*4;}
  quad(current,layout.local,0,(float)layout.localSourceY,224,(float)(224-layout.localSourceY));quad(labels,layout.label[0],0,(float)(party->self*8),96,8);
  unsigned index=0;for(unsigned s=0;s<4;s++)if(s!=party->self&&(party->members&(1u<<s))){if(have[s])quad(remote[s],layout.peer[index],0,0,224,224);quad(labels,layout.label[index+1],0,(float)(s*8),96,8);index++;}
 }else quad(current,screen,0,0,512,(float)height);
 unsigned hw=ta_population_image(hudPixels,512,&population,menu),hh=ta_population_height(menu);
 if(upload(hud,hudPixels,512,hw,hh)){
  float sx=menu?960.0f/512.0f:1.6f,sy=menu?720.0f/480.0f:1.6f;
  float hx=menu?160+(512-hw)*sx*.5f:1280-hw*sx-10,hy=menu?TA_POPULATION_MENU_TOP*sy:2;
  quad(hud,rect(hx,hy,hw*sx,hh*sy),0,0,(float)hw,(float)hh,true);
 }
 device->Present(NULL,NULL,NULL,NULL);
}
extern "C" void TA_Video_Submit(const unsigned char*p){TA_Music_MenuFrame();present((const uint32_t*)p,480);}
extern "C" bool TA_Video_SubmitActive224(const unsigned char*p){TA_Music_SetScene(TA_MUSIC_GAME);present((const uint32_t*)p,224);return device!=NULL;}
extern "C" bool TA_Video_ActiveAvailable(void){return device!=NULL;}
extern "C" bool TA_Video_SubmitActive224Direct(const Ppu*p){TA_Music_SetScene(TA_MUSIC_GAME);static uint32_t pixels[512*224];if(!ppu_taCopyActive224(p,(uint8_t*)pixels,sizeof pixels))return false;return TA_Video_SubmitActive224((const unsigned char*)pixels);}
/* NV2A-specific acceleration selects the portable PPU renderer on Xenos. */
extern "C" bool TA_Video_SubmitNative224Direct(const Ppu*){TA_Music_SetScene(TA_MUSIC_GAME);return false;}
extern "C" void TA_Video_BeginR126(Ppu*){TA_Music_SetScene(TA_MUSIC_GAME);}
extern "C" bool TA_Video_EndR126(Ppu*){return false;}
extern "C" void TA_Video_ReleaseLanPpu(Ppu*){sceneValid=false;}
extern "C" void TA_Video_R126Counts(unsigned long out[4]){memset(out,0,4*sizeof *out);}
extern "C" void TA_Video_ReadReset(unsigned long*a,unsigned long*b,unsigned long*c,unsigned long*d){*a=*b=*c=*d=0;}
extern "C" void TA_Video_ReadResetUploadTicks(uint64_t ticks[3]){memset(ticks,0,3*sizeof *ticks);}
extern "C" void TA_Video_Shutdown(void){
 for(unsigned i=0;i<3;i++)release(frames[i]);for(unsigned i=0;i<4;i++)release(remote[i]);release(labels);release(scene);release(hud);
 if(decl)decl->Release();if(vs)vs->Release();if(ps)ps->Release();if(device)device->Release();if(d3d)d3d->Release();decl=NULL;vs=NULL;ps=NULL;device=NULL;d3d=NULL;
}
