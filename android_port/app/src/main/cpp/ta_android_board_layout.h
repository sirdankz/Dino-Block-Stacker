#pragma once
#include <cmath>
/* Normalized centers/heights keep layouts portable across landscape screens.
   IDs: ranked 1v1, LAN 1v1, then AUTO/FOCUS for parties of 2, 3, 4. */
struct MobileBoardLayout {bool custom=false,full=false;float pose[12]={};unsigned revision=0;};
static MobileBoardLayout mobileLayouts[9];
static std::atomic<unsigned> activeBoardProfile{0};
static unsigned duelProfile=1;
static float boardClamp(float v,float a,float b){return v<a?a:v>b?b:v;}
static unsigned boardCount(unsigned id){return id<=2?2:2+(id-3)/2;}
static unsigned onlineRows(void){int w=viewportWidth.load(),h=viewportHeight.load();unsigned n=w>0&&h>0?(unsigned)(512LL*h/w):288u;if(n<128)n=128;if(n>384)n=384;if(n==224)n=226;return n;}
static float boardViewAspect(bool full){int w=viewportWidth.load(),h=viewportHeight.load();return full&&w>0&&h>0?(float)w/h:4.0f/3.0f;}
static TaPartyRect boardPoseRect(const float*pose,unsigned slot,unsigned id,unsigned rows,bool full){
 float aspect=(7.0f/12.0f)*((id>2&&slot==0)?224.0f/208.0f:1.0f),view=boardViewAspect(full);
 float header=id>2?fmaxf(.025f,8.0f/rows):0.0f;
 float h=boardClamp(pose[slot*3+2],.12f,1.0f-header);h=fminf(h,view/aspect);
 float w=h*aspect/view,cx=boardClamp(pose[slot*3],w*.5f,1-w*.5f),cy=boardClamp(pose[slot*3+1],header+h*.5f,1-h*.5f);
 TaPartyRect r={512*(cx-w*.5f),rows*(cy-h*.5f),512*(cx+w*.5f),rows*(cy+h*.5f)};return r;
}
static void boardDefaultPoses(unsigned id,unsigned width,unsigned height,bool full,float*out){
 memset(out,0,12*sizeof(float));if(!width||!height){width=1920;height=1080;}
 if(id<=2){
  out[0]=.21875f;out[1]=.5f;out[2]=1;out[3]=.78125f;out[4]=.5f;out[5]=1;
  if(full){out[0]=.25f;out[3]=.75f;out[2]=out[5]=.94f;}return;
 }
 unsigned rows=(unsigned)(512ULL*height/width);if(rows<128)rows=128;if(rows>384)rows=384;if(rows==224)rows=226;
 TaPartyLayout l=ta_party_layout_ex(512,rows,width*100ULL>=height*155ULL,boardCount(id)-1,1);
 for(unsigned i=0;i<boardCount(id);i++){TaPartyRect r=i?l.peer[i-1]:l.local;out[i*3]=(r.left+r.right)/1024;out[i*3+1]=(r.top+r.bottom)/(2*rows);out[i*3+2]=(r.bottom-r.top)/rows;}
}
extern "C" JNIEXPORT jfloatArray JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeBoardDefaults(JNIEnv*env,jclass,jint id,jint w,jint h,jboolean full){
 if(id<1||id>8)return NULL;float pose[12];boardDefaultPoses((unsigned)id,w>0?(unsigned)w:1920,h>0?(unsigned)h:1080,full,pose);jfloatArray a=env->NewFloatArray(12);if(a)env->SetFloatArrayRegion(a,0,12,pose);return a;
}
extern "C" JNIEXPORT void JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeBoardLayout(JNIEnv*env,jclass,jint id,jboolean custom,jboolean full,jfloatArray data){
 if(id<1||id>8||!data||env->GetArrayLength(data)!=12)return;float pose[12];env->GetFloatArrayRegion(data,0,12,pose);if(env->ExceptionCheck())return;
 for(unsigned i=0;i<12;i++)if(!std::isfinite(pose[i]))return;
 std::lock_guard<std::mutex> lock(frameMutex);MobileBoardLayout&l=mobileLayouts[id];l.custom=custom;l.full=id>2||full;memcpy(l.pose,pose,sizeof pose);++l.revision;partyCacheValid=false;legacyBackdropValid=false;
}
extern "C" JNIEXPORT jint JNICALL Java_org_dinoblockstacker_app_GameActivity_nativeBoardProfile(JNIEnv*,jclass){return (jint)activeBoardProfile.load();}
extern "C" void TA_Video_SetDuelMode(unsigned id){duelProfile=id==2?2:1;}
static void rememberLocalScene(const uint32_t*local){if(!localSceneValid){memcpy(localScene,local,sizeof localScene);localSceneValid=true;}}
static TaPartyLayout mobilePartyLayout(unsigned id,unsigned rows,unsigned peers){
 unsigned w=viewportWidth.load(),h=viewportHeight.load();TaPartyLayout l=ta_party_layout_ex(512,rows,h&&w*100ULL>=h*155ULL,peers,1);
 if(!mobileLayouts[id].custom)return l;
 for(unsigned i=0;i<=peers;i++){
  TaPartyRect r=boardPoseRect(mobileLayouts[id].pose,i,id,rows,true);if(i)l.peer[i-1]=r;else l.local=r;
  l.label[i]=ta_party_header(r,fmaxf(8,rows*.025f),1);
 }
 return l;
}
static unsigned partyCompose(const uint32_t*local){
 static uint32_t canvas[512*384],lastAt[4];static unsigned uploadTurn,lastRevision[4],lastRoster,lastRows,lastProfile,lastLayout;static bool have[4];
 unsigned peers=0;for(unsigned s=0;s<4;s++)if(s!=partyVisual->self&&(partyVisual->members&(1u<<s)))peers++;
 unsigned id=3+(peers>0?peers-1:0)*2+(partyVisual->mode?1:0);if(id>8)id=8;
 unsigned rows=onlineRows();TaPartyLayout layout=mobilePartyLayout(id,rows,peers);uint32_t now=GetTickCount();rememberLocalScene(local);activeBoardProfile=id;
 unsigned indexFor[4]={0},index=0;for(unsigned s=0;s<4;s++)if(s!=partyVisual->self&&(partyVisual->members&(1u<<s)))indexFor[s]=index++;
 bool rebuild=!partyCacheValid||lastRows!=rows||lastProfile!=id||lastLayout!=mobileLayouts[id].revision;
 if(rebuild){
  ta_mobile_backdrop(canvas,512,rows,localScene,512);memset(lastRevision,0,sizeof lastRevision);memset(have,0,sizeof have);lastRoster=~0u;lastRows=rows;lastProfile=id;lastLayout=mobileLayouts[id].revision;partyCacheValid=true;
  /* Geometry edits reuse the latest real images immediately; no three-frame holes. */
  for(unsigned s=0;s<4;s++)if(s!=partyVisual->self&&(partyVisual->members&(1u<<s))&&partyVisual->revision[s]){partyBlit(canvas,rows,layout.peer[indexFor[s]],partyVisual->pixels[s],224,0,224);lastRevision[s]=partyVisual->revision[s];lastAt[s]=now;have[s]=true;}
 }
 for(unsigned k=0;k<4;k++){unsigned s=(uploadTurn+k)%4;if(s==partyVisual->self||!(partyVisual->members&(1u<<s)))continue;
  if(ta_party_upload_due(have[s],lastRevision[s],partyVisual->revision[s],now,lastAt[s],peers>1?125u:66u)){partyBlit(canvas,rows,layout.peer[indexFor[s]],partyVisual->pixels[s],224,0,224);lastRevision[s]=partyVisual->revision[s];lastAt[s]=now;have[s]=true;uploadTurn=(s+1)%4;partyStats[0]++;partyStats[3]+=224*224*4;break;}
 }
 if(lastRoster!=partyVisual->rosterRevision||rebuild){partyBlit(canvas,rows,layout.label[0],partyVisual->roster,512,0,96,8,partyVisual->self*8);
  for(unsigned s=0;s<4;s++)if(s!=partyVisual->self&&(partyVisual->members&(1u<<s)))partyBlit(canvas,rows,layout.label[indexFor[s]+1],partyVisual->roster,512,0,96,8,s*8);
  lastRoster=partyVisual->rosterRevision;partyStats[1]++;partyStats[3]+=96*32*4;
 }
 memcpy(displayFrame,canvas,512*rows*4);partyBlit(displayFrame,rows,layout.local,local,512,0,224,224-layout.localSourceY,layout.localSourceY);return rows;
}
static unsigned duelCompose(const uint32_t*local){
 unsigned id=duelProfile;activeBoardProfile=id;MobileBoardLayout&l=mobileLayouts[id];rememberLocalScene(local);
 if(!l.custom){
  /* Exact original offline canvas: local left, remote in the original right slot. */
  /* 1v1 streams the 192x192 field only. Keep the original right-side
     frame/scenery and replace exactly the same inner field as the Xbox path. */
  for(unsigned y=24;y<216;y++)memcpy(displayFrame+y*512+304,opponent+y*224+16,192*4);return 224;
 }
 unsigned rows=l.full?onlineRows():224;static uint32_t canvas[512*384],peerImage[224*224];static unsigned lastRows,lastProfile,lastRevision=~0u,lastLayout=~0u,lastViewport=~0u;
 TaPartyRect peer=boardPoseRect(l.pose,1,id,rows,l.full),own=boardPoseRect(l.pose,0,id,rows,l.full);rememberLocalScene(local);
 bool rebuild=!legacyBackdropValid||lastRows!=rows||lastProfile!=id||lastLayout!=l.revision||lastViewport!=boardViewportRevision.load();
 if(rebuild){ta_mobile_backdrop(canvas,512,rows,localScene,512);lastRows=rows;lastProfile=id;lastLayout=l.revision;lastViewport=boardViewportRevision.load();legacyBackdropValid=true;lastRevision=~opponentRevision;}
 if(lastRevision!=opponentRevision||rebuild){
  for(unsigned y=0;y<224;y++)memcpy(peerImage+y*224,local+y*512+288,224*4);
  for(unsigned y=24;y<216;y++)memcpy(peerImage+y*224+16,opponent+y*224+16,192*4);
  partyBlit(canvas,rows,peer,peerImage,224,0,224);lastRevision=opponentRevision;
 }
 memcpy(displayFrame,canvas,512*rows*4);partyBlit(displayFrame,rows,own,local,512,0,224);return rows;
}
static void present(const uint32_t*p,unsigned height){
 if(stopped)throw AndroidStop();{
 std::lock_guard<std::mutex> lock(frameMutex);memcpy(displayFrame,p,512*height*4);
 if(partyVisual&&height==224)height=partyCompose(p);
 else if(opponent&&height==224)height=duelCompose(p);
 else activeBoardProfile=0;
 static uint32_t hud[512*TA_POPULATION_MAX_HEIGHT];unsigned menu=height==480;
 unsigned hw=ta_population_image(hud,512,&population,menu),hh=ta_population_height(menu);unsigned hx=menu?(512-hw)/2:512-hw-5,hy=menu?TA_POPULATION_MENU_TOP:1;
 for(unsigned y=0;y<hh;y++){
  uint32_t*dest=displayFrame+(hy+y)*512+hx;const uint32_t*source=hud+y*512;
  for(unsigned x=0;x<hw;x++)if(source[x]>>24)dest[x]=source[x]&0x00ffffffu;
 }
 frameHeight=height;frameRevision++;}
 auto now=Clock::now();if(nextFrame<now-std::chrono::milliseconds(100))nextFrame=now;nextFrame+=std::chrono::nanoseconds(1000000000LL/60);std::this_thread::sleep_until(nextFrame);
}
