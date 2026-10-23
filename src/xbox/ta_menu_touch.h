#pragma once
/* Hit regions use the same safe artwork bounds as the rendered controls. */
#ifdef TA_ANDROID
extern "C" void TA_Menu_Reset(void);
extern "C" void TA_Menu_Begin(const char*);
extern "C" void TA_Menu_Region(int,int,int,int,int,unsigned,unsigned);
extern "C" unsigned TA_Menu_Read(int*);
#else
static void TA_Menu_Reset(void){}
static void TA_Menu_Begin(const char*){}
static void TA_Menu_Region(int,int,int,int,int,unsigned,unsigned){}
static unsigned TA_Menu_Read(int*d){*d=0;return 0;}
#endif
static bool menuTouchEnabled=true,menuTouchPressed;
static unsigned menuTouchRowIndex,menuTouchKey=~0u;
static int menuTouchDelta;
static void menuTouchChoice(int x,int y,int w,int h,unsigned index,bool selected){if(menuTouchEnabled){if(y<395&&h>=26&&h<32){y-=(32-h)/2;h=32;}TA_Menu_Region(x,y,w,h,(int)index,selected?1u:0u,0);}}
static void menuTouchAction(int x,int y,int w,int h,unsigned action){if(menuTouchEnabled)TA_Menu_Region(x,y,w,h,-1,0,action);}
static void menuTouchFooter(int x,int y,const char*text,unsigned advance){
 if(!text)return;
 for(unsigned i=0;text[i];){unsigned start=i;while(text[i]&&!(text[i]==' '&&text[i+1]==' '))i++;unsigned end=i;while(text[i]==' ')i++;
  unsigned action=0;const char*s=text+start;
  if(s[0]=='A'&&s[1]==' ')action=1;else if(s[0]=='B'&&s[1]==' ')action=0x100;
  else if(!strncmp(s,"BACK ",5))action=4;else if(!strncmp(s,"START ",6))action=8;
  else if(s[0]=='X'&&s[1]==' ')action=2;else if(s[0]=='Y'&&s[1]==' ')action=0x200;
  else if(!strncmp(s,"LEFT ",5))action=0x40;else if(!strncmp(s,"RIGHT ",6))action=0x80;
  if(action){int top=y>=430?430:y-5;int height=y>=430?18:24;menuTouchAction(x+(int)(start*advance)-3,top,(int)((end-start)*advance)+6,height,action);}
 }
}
