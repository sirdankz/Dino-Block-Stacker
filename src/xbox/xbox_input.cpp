/* OG Xbox gamepads -> SNES joypad bits; retain existing button mapping. */
#include "ta_platform.h"
#include "ta_controls_impl.h"
static HANDLE pads[4];
static DWORD lastError[4];
extern "C" void TA_Input_SetLeaderboard(unsigned){}
extern "C" int TA_Input_LeaderboardScroll(void){return 0;}
extern "C" unsigned TA_Input_LeaderboardActions(void){return 0;}
static bool sawState[4],sawButtons[4];
static unsigned retryFrames;static int rightX,rightY;
extern "C" void TA_Input_RightStick(int*x,int*y){*x=rightX;*y=rightY;}
#define THRESH 40
#define RETRY_FRAMES 60
static void close_pad(DWORD port){
  if(pads[port])XInputClose(pads[port]);
  pads[port]=NULL;sawState[port]=sawButtons[port]=false;
}
static void open_pad(DWORD port){
  if(pads[port])return;
  pads[port]=XInputOpen(XDEVICE_TYPE_GAMEPAD,port,XDEVICE_NO_SLOT,NULL);
  if(pads[port]){
    lastError[port]=0;sawState[port]=sawButtons[port]=false;
    TA_Log("[TA R45 INPUT] port=%lu opened=1\n",(unsigned long)port);
  }else{
    const DWORD error=GetLastError();
    if(lastError[port]!=error || error==0)
      TA_Log("[TA R45 INPUT] port=%lu opened=0 error=%lu retry=enabled\n",(unsigned long)port,(unsigned long)error);
    lastError[port]=error;
  }
}
extern "C" void TA_Input_Init(void){
  memset(pads,0,sizeof(pads));memset(lastError,0,sizeof(lastError));
  memset(sawState,0,sizeof(sawState));memset(sawButtons,0,sizeof(sawButtons));retryFrames=0;
  XDEVICE_PREALLOC_TYPE p[]={{XDEVICE_TYPE_GAMEPAD,4}};
  XInitDevices(1,p);Sleep(50);
  DWORD inserted=0,removed=0;
  XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD,&inserted,&removed);
  const DWORD connected=XGetDevices(XDEVICE_TYPE_GAMEPAD);
  TA_Log("[TA R45 INPUT] fix=1 connected_mask=%lX retry_interval_frames=60\n",(unsigned long)connected);
  for(DWORD i=0;i<4;i++)if(connected&(1u<<i))open_pad(i);
}
static void (*backgroundTick)(void);
extern "C" void TA_Background_Set(void (*tick)(void)){backgroundTick=tick;}
extern "C" void TA_Input_Poll(uint16_t out[2]){
  if(backgroundTick)backgroundTick();
  out[0]=out[1]=0;rightX=rightY=0;
  DWORD inserted=0,removed=0;
  if(XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD,&inserted,&removed))for(DWORD i=0;i<4;i++){
    if(removed&(1u<<i))close_pad(i);
    if(inserted&(1u<<i))open_pad(i);
  }
  /* An insertion event can precede a successful open. Recheck presence at a
     bounded cadence so a connected pad can recover without another event. */
  if(++retryFrames>=RETRY_FRAMES){
    retryFrames=0;const DWORD connected=XGetDevices(XDEVICE_TYPE_GAMEPAD);
    for(DWORD i=0;i<4;i++){
      if(!(connected&(1u<<i)))close_pad(i);
      else if(!pads[i])open_pad(i);
    }
  }
  int pl=0;
  for(int i=0;i<4&&pl<2;i++)if(pads[i]){
    XINPUT_STATE s;memset(&s,0,sizeof(s));
    const DWORD error=XInputGetState(pads[i],&s);
    if(error!=ERROR_SUCCESS){
      if(lastError[i]!=error)TA_Log("[TA R45 INPUT] port=%u read_error=%lu\n",(unsigned)i,(unsigned long)error);
      lastError[i]=error;
      if(error==ERROR_DEVICE_NOT_CONNECTED)close_pad((DWORD)i);
      continue;
    }
    lastError[i]=0;
    if(!sawState[i]){sawState[i]=true;TA_Log("[TA R45 INPUT] port=%u state=ok packet=%lu\n",(unsigned)i,(unsigned long)s.dwPacketNumber);}
    WORD b=s.Gamepad.wButtons;BYTE *a=s.Gamepad.bAnalogButtons;uint16_t j=0;
    if((b&XINPUT_GAMEPAD_DPAD_UP)||s.Gamepad.sThumbLY>12000)j|=1u<<4;
    if((b&XINPUT_GAMEPAD_DPAD_DOWN)||s.Gamepad.sThumbLY< -12000)j|=1u<<5;
    if((b&XINPUT_GAMEPAD_DPAD_LEFT)||s.Gamepad.sThumbLX< -12000)j|=1u<<6;
    if((b&XINPUT_GAMEPAD_DPAD_RIGHT)||s.Gamepad.sThumbLX>12000)j|=1u<<7;
    if(b&XINPUT_GAMEPAD_START)j|=1u<<3;
    if(b&XINPUT_GAMEPAD_BACK)j|=1u<<2;
    if(a[XINPUT_GAMEPAD_A]>THRESH)j|=1u<<0;
    if(a[XINPUT_GAMEPAD_X]>THRESH)j|=1u<<1;
    if(a[XINPUT_GAMEPAD_B]>THRESH)j|=1u<<8;
    if(a[XINPUT_GAMEPAD_Y]>THRESH)j|=1u<<9;
    if(a[XINPUT_GAMEPAD_WHITE]>THRESH)j|=0x1000u;
    if(a[XINPUT_GAMEPAD_BLACK]>THRESH)j|=0x2000u;
    if(a[XINPUT_GAMEPAD_LEFT_TRIGGER]>THRESH)j|=1u<<10;
    if(a[XINPUT_GAMEPAD_RIGHT_TRIGGER]>THRESH)j|=1u<<11;
    if(j&&!sawButtons[i]){sawButtons[i]=true;TA_Log("[TA R45 INPUT] port=%u player=%u buttons_seen=1 snes=%04X digital=%04X\n",(unsigned)i,(unsigned)(pl+1),(unsigned)j,(unsigned)b);}
    if(pl==0){rightX=s.Gamepad.sThumbRX;rightY=s.Gamepad.sThumbRY;}out[pl++]=j;
  }
}
extern "C" void TA_Input_Shutdown(void){for(DWORD i=0;i<4;i++)close_pad(i);}
