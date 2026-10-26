#include "ta_pc_internal.h"
extern "C" {
#include "ta_vs_lan_screen.h"
}
#include "ta_menu_theme.h"
#include <algorithm>
struct TextScope{TextScope(){TA_Menu_Reset();pcTyped.clear();pcTextSubmit=pcTextCancel=pcTextErase=false;pcTextMode=true;SDL_StartTextInput(pcWindow);}~TextScope(){SDL_StopTextInput(pcWindow);pcTextMode=false;pcTyped.clear();TA_Menu_Reset();}};
extern "C" int TA_PC_TextInput(char*out,unsigned capacity,const char*title,const char*label,bool password,unsigned minimum,int(*tick)(void)){
 if(!out||capacity<2||capacity>33)return 0;TextScope scope;char value[33]={0};unsigned length=0,key=0;bool chat=minimum==1;
 static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ .!?";unsigned keyCount=chat?44:40,deleteKey=keyCount-2,doneKey=keyCount-1;
 static uint32_t pixels[512*480];uint16_t pads[2];TA_Input_Poll(pads);uint16_t previous=pads[0],held=0;uint32_t repeated=0;bool repeating=false;
 for(;;){TA_Input_Poll(pads);if(tick&&tick())return 0;unsigned pressed=pads[0]&~previous;previous=pads[0];unsigned direction=pads[0]&0xf0;uint32_t now=GetTickCount();if(direction!=held){held=(uint16_t)direction;repeated=now;repeating=false;}else if(held&&(uint32_t)(now-repeated)>=(repeating?100u:320u)){pressed|=held;repeated=now;repeating=true;}
  int delta=0;unsigned mouse=TA_Menu_Read(&delta);if(mouse&0x1000000u){unsigned k=(mouse>>16)&255u;if(k<keyCount){key=k;pressed|=1;}}else pressed|=mouse&0xffffu;
  if(pcTextCancel||(pressed&0x100)){SecureZeroMemory(value,sizeof value);return 0;}
  unsigned row=key/8,col=key%8,lastRow=(keyCount-1)/8;
  if(pressed&0x40)col=col?col-1:std::min(7u,keyCount-1-row*8);
  if(pressed&0x80)col=col<std::min(7u,keyCount-1-row*8)?col+1:0;
  if(pressed&0x10)row=row?row-1:lastRow;if(pressed&0x20)row=row<lastRow?row+1:0;col=std::min(col,std::min(7u,keyCount-1-row*8));key=row*8+col;
  if(pcTextErase||(pressed&2u)||((pressed&1)&&key==deleteKey)){if(length)value[--length]=0;}pcTextErase=false;
  auto append=[&](unsigned char c){if(length+1>=capacity)return;if(password){if(c<33||c>126)return;}else if(chat){if(c<32||c>126)return;}else{if(c>='a'&&c<='z')c-=32;if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return;}value[length++]=(char)c;value[length]=0;};
  for(unsigned char c:pcTyped)append(c);if(!pcTyped.empty())SecureZeroMemory(pcTyped.data(),pcTyped.size());pcTyped.clear();if((pressed&1)&&key<deleteKey)append(alphabet[key]);if(chat&&(pressed&0x200))append(' ');
  if((pcTextSubmit||(pressed&8)||((pressed&1)&&key==doneKey))&&length>=minimum){memcpy(out,value,length+1);SecureZeroMemory(value,sizeof value);return 1;}pcTextSubmit=false;
  TA_Menu_Begin(title);ta_menu_art_draw(pixels,now);ta_vs_lan_text_box(pixels,36,34,350,14,"DINO BLOCK STACKER",0x007dd9e8,2);ta_vs_lan_text_box(pixels,406,34,70,14,"V1.2.5",0x00bdc7d5,2);ta_vs_lan_text_box(pixels,36,65,440,24,title,0x00f1f3f8,4);
  char hint[80];snprintf(hint,sizeof hint,"%u-%u CHARACTERS - TYPE OR USE GRID",minimum,capacity-1);ta_vs_lan_text_box(pixels,36,97,440,28,hint,0x00bdc7d5,2);ta_vs_lan_text_box(pixels,42,156,432,14,label,0x00bdc7d5,2);
  char shown[33]={0};if(password)memset(shown,'X',length);else memcpy(shown,value,length);ta_vs_lan_text_box(pixels,42,179,432,18,shown,0x00f1f3f8,3);
  for(unsigned k=0;k<keyCount;k++){int x=48+(k%8)*54,y=218+(k/8)*29;TA_Menu_Region(x-5,y-6,52,26,-1,0,0x1000001u|(k<<16));if(k==key)for(int yy=y-5;yy<y+19;yy++)for(int xx=x-4;xx<x+47;xx++){auto&p=pixels[yy*512+xx];p=((p&0xfefefe)>>1)+0x00335c74;}
   char ch[2]={k<deleteKey?alphabet[k]:' ',0};const char*name=k==deleteKey?"DEL":k==doneKey?(chat?"SEND":"DONE"):k==38&&chat?"SPACE":ch;ta_vs_lan_text_box(pixels,x,y,44,14,name,k==key?0x00f1cb72:0x00f1f3f8,2);}
  ta_vs_lan_text_box(pixels,36,416,440,14,chat?"ENTER SEND  ESC CANCEL  CTRL+V PASTE":"ENTER DONE  ESC CANCEL  CTRL+V PASTE",0x00bdc7d5,2);ta_vs_lan_text_box(pixels,36,438,440,14,"A TYPE  X DELETE  START DONE  B BACK",0x00bdc7d5,2);
  TA_Menu_Region(32,433,76,18,-1,0,1);TA_Menu_Region(128,433,112,18,-1,0,2);TA_Menu_Region(248,433,136,18,-1,0,8);TA_Menu_Region(392,433,84,18,-1,0,0x100);TA_Video_Submit((const unsigned char*)pixels);
 }
}
extern "C" int ta_pc_chat_input(char*out,unsigned capacity,const char*title,int(*tick)(void)){return TA_PC_TextInput(out,capacity,title,"MESSAGE",false,1,tick);}
