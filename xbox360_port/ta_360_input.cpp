#include "ta_360_platform.h"
#include "ta_music.h"
#include "ta_controls_impl.h"
static void (*backgroundTick)(void);
static int rightX,rightY;
extern "C" void TA_Background_Set(void (*tick)(void)){backgroundTick=tick;}
extern "C" void TA_Input_Init(void){rightX=rightY=0;}
extern "C" void TA_Input_Shutdown(void){}
extern "C" void TA_Input_RightStick(int*x,int*y){*x=rightX;*y=rightY;}
extern "C" void TA_Input_SetLeaderboard(unsigned){}
extern "C" int TA_Input_LeaderboardScroll(void){return 0;}
extern "C" unsigned TA_Input_LeaderboardActions(void){return 0;}
extern "C" void TA_Input_Poll(uint16_t out[2]){TA_Music_MenuFrame();
 if(backgroundTick)backgroundTick();out[0]=out[1]=0;rightX=rightY=0;
 /* Physical controller indices stay stable when a controller disconnects. */
 for(unsigned slot=0;slot<2;slot++){
  XINPUT_STATE state;memset(&state,0,sizeof state);if(XInputGetState(slot,&state)!=ERROR_SUCCESS)continue;
  const XINPUT_GAMEPAD&g=state.Gamepad;WORD b=g.wButtons;uint16_t value=0;
  if((b&XINPUT_GAMEPAD_DPAD_UP)||g.sThumbLY>12000)value|=0x10;
  if((b&XINPUT_GAMEPAD_DPAD_DOWN)||g.sThumbLY< -12000)value|=0x20;
  if((b&XINPUT_GAMEPAD_DPAD_LEFT)||g.sThumbLX< -12000)value|=0x40;
  if((b&XINPUT_GAMEPAD_DPAD_RIGHT)||g.sThumbLX>12000)value|=0x80;
  if(b&XINPUT_GAMEPAD_A)value|=1;if(b&XINPUT_GAMEPAD_X)value|=2;
  if(b&XINPUT_GAMEPAD_B)value|=0x100;if(b&XINPUT_GAMEPAD_Y)value|=0x200;
  if(b&XINPUT_GAMEPAD_START)value|=8;if(b&XINPUT_GAMEPAD_BACK)value|=4;
  if(g.bLeftTrigger>40)value|=0x400;if(g.bRightTrigger>40)value|=0x800;
  if(b&XINPUT_GAMEPAD_RIGHT_SHOULDER)value|=0x1000;if(b&XINPUT_GAMEPAD_LEFT_SHOULDER)value|=0x2000;
  out[slot]=value;if(!slot){rightX=g.sThumbRX;rightY=g.sThumbRY;}
 }
}
