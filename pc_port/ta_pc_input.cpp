#include "ta_pc_internal.h"
#include "ta_music.h"
#include "ta_controls_impl.h"
#include <algorithm>
#include <fstream>
#include <sstream>
static SDL_Gamepad*pads[2];
static void (*backgroundTick)(void);
static int stickX,stickY;
bool pcTextMode,pcTextSubmit,pcTextCancel,pcTextErase;
std::string pcTyped;
static bool leaderboardMode;
static int leaderboardScroll;
static unsigned leaderboardActions;
struct Region {int x,y,w,h,index;unsigned selected,action;};
static Region draft[64],regions[64];
static unsigned draftCount,regionCount,titleHash,queuedAction,deferredAction;
static int draftSelected=-1,selected=-1,queuedDelta;
static bool menuReady;
static SDL_Scancode keys[2][14];
static const char*actions[14]={"A","X","BACK","START","UP","DOWN","LEFT","RIGHT","B","Y","L","R","TARGET","AUX"};
static const SDL_Scancode defaults[2][14]={
 {SDL_SCANCODE_Z,SDL_SCANCODE_X,SDL_SCANCODE_BACKSPACE,SDL_SCANCODE_RETURN,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_S,SDL_SCANCODE_A,SDL_SCANCODE_Q,SDL_SCANCODE_W,SDL_SCANCODE_C,SDL_SCANCODE_E},
 {SDL_SCANCODE_KP_1,SDL_SCANCODE_KP_3,SDL_SCANCODE_KP_MINUS,SDL_SCANCODE_KP_ENTER,SDL_SCANCODE_KP_8,SDL_SCANCODE_KP_2,SDL_SCANCODE_KP_4,SDL_SCANCODE_KP_6,SDL_SCANCODE_KP_PERIOD,SDL_SCANCODE_KP_9,SDL_SCANCODE_KP_7,SDL_SCANCODE_KP_5,SDL_SCANCODE_KP_0,SDL_SCANCODE_KP_DIVIDE}};
static void openPad(SDL_JoystickID id){for(auto p:pads)if(p&&SDL_GetGamepadID(p)==id)return;for(auto&p:pads)if(!p){p=SDL_OpenGamepad(id);return;}}
extern "C" void TA_Input_Init(void){
 memcpy(keys,defaults,sizeof keys);std::string path=std::string(TA_PC_SaveDirectory())+"/keyboard.ini";std::ifstream in(pcWide(path));
 if(in){std::string line;while(std::getline(in,line)){if(line.empty()||line[0]=='#')continue;auto eq=line.find('=');if(eq==std::string::npos)continue;std::string key=line.substr(0,eq),value=line.substr(eq+1);if(!value.empty()&&value.back()=='\r')value.pop_back();SDL_Scancode scan=SDL_GetScancodeFromName(value.c_str());if(scan==SDL_SCANCODE_UNKNOWN)continue;for(unsigned p=0;p<2;p++)for(unsigned a=0;a<14;a++)if(key=="P"+std::to_string(p+1)+"_"+actions[a])keys[p][a]=scan;}}
 else{std::ofstream out(pcWide(path));out<<"# SDL scancode names. Changes apply on the next launch.\n";for(unsigned p=0;p<2;p++)for(unsigned a=0;a<14;a++)out<<"P"<<p+1<<"_"<<actions[a]<<"="<<SDL_GetScancodeName(keys[p][a])<<"\n";}
 int count=0;SDL_JoystickID*ids=SDL_GetGamepads(&count);for(int i=0;i<count;i++)openPad(ids[i]);SDL_free(ids);TA_Controls_Init(TA_PC_SaveDirectory());
}
extern "C" void TA_Input_Shutdown(void){for(auto&p:pads){if(p)SDL_CloseGamepad(p);p=NULL;}}
extern "C" void TA_Background_Set(void(*tick)(void)){backgroundTick=tick;}
extern "C" void TA_Menu_Reset(void){menuReady=false;draftCount=regionCount=0;queuedDelta=0;queuedAction=deferredAction=0;}
extern "C" void TA_Menu_Begin(const char*title){unsigned hash=2166136261u;for(auto p=(const unsigned char*)title;*p;p++)hash=(hash^*p)*16777619u;if(hash!=titleHash){TA_Menu_Reset();titleHash=hash;}draftCount=0;draftSelected=-1;}
extern "C" void TA_Menu_Region(int x,int y,int w,int h,int index,unsigned chosen,unsigned action){if(draftCount>=64||w<=0||h<=0)return;draft[draftCount++]={x,y,w,h,index,chosen,action};if(chosen)draftSelected=index;}
void pcMenuCommit(){memcpy(regions,draft,draftCount*sizeof(Region));regionCount=draftCount;selected=draftSelected;menuReady=true;}
extern "C" unsigned TA_Menu_Read(int*delta){*delta=queuedDelta;queuedDelta=0;unsigned action=queuedAction;queuedAction=0;if(*delta&&action){deferredAction=action;return 0;}if(!action&&!*delta){action=deferredAction;deferredAction=0;}return action;}
void pcMouse(float wx,float wy,unsigned button){
 if(!menuReady||!pcMenuFrame||queuedAction||deferredAction)return;if(button==SDL_BUTTON_RIGHT){queuedAction=0x100;return;}if(button!=SDL_BUTTON_LEFT)return;
 float x,y;pcMenuPoint(wx,wy,x,y);for(unsigned i=regionCount;i;i--){auto&r=regions[i-1];if(x<r.x||x>=r.x+r.w||y<r.y||y>=r.y+r.h)continue;if(r.index>=0){if(selected<0)return;queuedDelta=r.index-selected;queuedAction=r.action?r.action:1;}else queuedAction=r.action;return;}
}
void pcMouseWheel(int rows){if(leaderboardMode)leaderboardScroll=std::clamp(leaderboardScroll+rows,-64,64);else if(menuReady&&selected>=0)queuedDelta=std::clamp(queuedDelta+rows,-32,32);}
extern "C" void TA_Input_SetLeaderboard(unsigned enabled){leaderboardMode=enabled!=0;leaderboardScroll=0;leaderboardActions=0;}
extern "C" int TA_Input_LeaderboardScroll(void){int value=leaderboardScroll;leaderboardScroll=0;return value;}
extern "C" unsigned TA_Input_LeaderboardActions(void){unsigned value=leaderboardActions;leaderboardActions=0;return value;}
static bool pcExitPrompt;
static bool pcConfirmSessionExit(){if(pcExitPrompt)return false;struct Scope{Scope(){pcExitPrompt=true;}~Scope(){pcExitPrompt=false;}} scope;return TA_Lobby_ConfirmExit()!=0;}
void pcEvents(){
 SDL_Event event;while(SDL_PollEvent(&event))switch(event.type){
 case SDL_EVENT_QUIT:case SDL_EVENT_WINDOW_CLOSE_REQUESTED:if(pcConfirmSessionExit())throw PcRequest::Quit;break;
 case SDL_EVENT_WINDOW_RESIZED:case SDL_EVENT_WINDOW_MOVED:case SDL_EVENT_WINDOW_FOCUS_GAINED:case SDL_EVENT_WINDOW_FOCUS_LOST:pcVideoWindowChanged();break;
 case SDL_EVENT_GAMEPAD_ADDED:openPad(event.gdevice.which);break;
 case SDL_EVENT_GAMEPAD_REMOVED:for(auto&p:pads)if(p&&SDL_GetGamepadID(p)==event.gdevice.which){SDL_CloseGamepad(p);p=NULL;}break;
 case SDL_EVENT_MOUSE_BUTTON_DOWN:pcMouse(event.button.x,event.button.y,event.button.button);break;
 case SDL_EVENT_MOUSE_WHEEL:{int step=event.wheel.y>0?-1:event.wheel.y<0?1:0;if(event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED)step=-step;pcMouseWheel(step);break;}
 case SDL_EVENT_TEXT_INPUT:if(pcTextMode)pcTyped+=event.text.text;break;
 case SDL_EVENT_KEY_DOWN:{auto scan=event.key.scancode;bool ctrl=(event.key.mod&SDL_KMOD_CTRL)!=0,alt=(event.key.mod&SDL_KMOD_ALT)!=0;
  if(!event.key.repeat&&(scan==SDL_SCANCODE_F11||(scan==SDL_SCANCODE_RETURN&&alt))){TA_Options_Set(TA_OPT_FULLSCREEN,(SDL_GetWindowFlags(pcWindow)&SDL_WINDOW_FULLSCREEN)?0:1);TA_PC_ApplyDisplayOptions(0);TA_Options_Save();break;}
  if(!event.key.repeat&&ctrl&&scan==SDL_SCANCODE_O&&!pcTextMode&&pcConfirmSessionExit())throw PcRequest::Import;
  if(pcTextMode){if(scan==SDL_SCANCODE_RETURN&&!alt)pcTextSubmit=true;else if(scan==SDL_SCANCODE_ESCAPE)pcTextCancel=true;else if(scan==SDL_SCANCODE_BACKSPACE)pcTextErase=true;else if(ctrl&&scan==SDL_SCANCODE_V){char*text=SDL_GetClipboardText();if(text){pcTyped.append(text,std::min<size_t>(strlen(text),256));SDL_free(text);}}}
  break;}
 default:break;
 }
}
static uint16_t gamepad(SDL_Gamepad*p){if(!p||!SDL_GamepadConnected(p))return 0;uint16_t bits=0;
 static const SDL_GamepadButton names[14]={SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_WEST,SDL_GAMEPAD_BUTTON_BACK,SDL_GAMEPAD_BUTTON_START,SDL_GAMEPAD_BUTTON_DPAD_UP,SDL_GAMEPAD_BUTTON_DPAD_DOWN,SDL_GAMEPAD_BUTTON_DPAD_LEFT,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_NORTH,SDL_GAMEPAD_BUTTON_INVALID,SDL_GAMEPAD_BUTTON_INVALID,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER};
 for(unsigned i=0;i<14;i++)if(names[i]!=SDL_GAMEPAD_BUTTON_INVALID&&SDL_GetGamepadButton(p,names[i]))bits|=(uint16_t)(1u<<i);
 int x=SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_LEFTX),y=SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_LEFTY);if(x<-16000)bits|=0x40;if(x>16000)bits|=0x80;if(y<-16000)bits|=0x10;if(y>16000)bits|=0x20;
 if(SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)>8000)bits|=0x400;if(SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)>8000)bits|=0x800;return bits;
}
extern "C" void TA_Input_Poll(uint16_t out[2]){pcFrameStart();TA_Music_MenuFrame();
 pcEvents();if(backgroundTick)backgroundTick();const bool*down=SDL_GetKeyboardState(NULL);bool focused=(SDL_GetWindowFlags(pcWindow)&SDL_WINDOW_INPUT_FOCUS)!=0;
 for(unsigned p=0;p<2;p++){out[p]=gamepad(pads[p]);if(focused&&!pcTextMode){for(unsigned b=0;b<14;b++)if(down[keys[p][b]])out[p]|=(uint16_t)(1u<<b);}}
 if(focused){if(pcTextMode){if(down[SDL_SCANCODE_UP])out[0]|=0x10;if(down[SDL_SCANCODE_DOWN])out[0]|=0x20;if(down[SDL_SCANCODE_LEFT])out[0]|=0x40;if(down[SDL_SCANCODE_RIGHT])out[0]|=0x80;}
  else{if(pcMenuFrame&&down[SDL_SCANCODE_RETURN]&&!(SDL_GetModState()&SDL_KMOD_ALT)){out[0]&=~8u;out[0]|=1;}if(down[SDL_SCANCODE_ESCAPE])out[0]|=pcMenuFrame?0x100u:8u;if(pcMenuFrame&&down[SDL_SCANCODE_F5])out[0]|=8;if(down[SDL_SCANCODE_TAB])out[0]|=4;if(down[SDL_SCANCODE_F10])out[0]|=12;}}
 stickX=pads[0]?SDL_GetGamepadAxis(pads[0],SDL_GAMEPAD_AXIS_RIGHTX):0;stickY=pads[0]?-(int)SDL_GetGamepadAxis(pads[0],SDL_GAMEPAD_AXIS_RIGHTY):0;
}
extern "C" void TA_Input_RightStick(int*x,int*y){*x=stickX;*y=stickY;}
