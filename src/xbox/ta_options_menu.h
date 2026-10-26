#ifndef TA_OPTIONS_MENU_H
#define TA_OPTIONS_MENU_H
#include "ta_options.h"
static bool optionsSaved=true;
static void optionsCommit(){optionsSaved=TA_Options_Save()!=0;}
static const char*optionsSaveStatus(){return optionsSaved?"SAVED ON THIS DEVICE":"SAVE FAILED - ACTIVE THIS SESSION";}
static void optionsAdjustRegion(unsigned row,unsigned y,unsigned selected){
 TA_Menu_Region(340,(int)y-5,54,32,(int)row,row==selected,0x40);
 TA_Menu_Region(414,(int)y-5,54,32,(int)row,row==selected,0x80);
 ta_menu_text(screen,358,y,"-",uiText,2);ta_menu_text(screen,432,y,"+",uiText,2);
}
static void optionsChip(int x,int y,unsigned block,uint32_t color,unsigned size=20){
 static const unsigned char marks[6][5]={{10,31,31,14,4},{14,17,17,17,14},{4,14,14,31,31},{4,31,14,31,17},{31,31,14,14,4},{4,14,31,14,4}};
 accountRect(x,y,size,size,0x000b1420);accountRect(x+1,y+1,size-2,size-2,color);accountRect(x+2,y+2,size-4,1,0x00f1f3f8);
 unsigned scale=size>=26?3:2;int left=x+(int)(size-5*scale)/2,top=y+(int)(size-5*scale)/2;
 for(unsigned yy=0;yy<5;yy++)for(unsigned xx=0;xx<5;xx++)if(marks[block][yy]&(1u<<(4-xx)))accountRect(left+xx*scale,top+yy*scale,scale,scale,0x00212b3c);
}
static void optionsVolumeMenu(){
 static const char*labels[]={"MASTER","MENU/RESULTS MUSIC","GAME AUDIO"};unsigned choice=0;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pressed&0x100)return;choice=menuMove(choice,5,pressed);
  if(choice<3&&(pressed&(1u|0xc0u))){int value=(int)TA_Options_Get(choice)+(pressed&0x40?-5:5);if(value<0)value=0;if(value>100)value=100;TA_Options_Set(choice,(unsigned)value);optionsCommit();}
  else if(pressed&1){if(choice==3){TA_Options_ResetVolume();optionsCommit();}else if(choice==4)return;}
  accountFrame("VOLUME OPTIONS","LEFT/RIGHT OR TAP -/+ TO ADJUST");
  for(unsigned i=0;i<3;i++){unsigned y=164+i*53;char text[64];snprintf(text,sizeof text,"%u%%",TA_Options_Get(i));menuRow(y,labels[i],choice==i,2);ta_menu_text(screen,280,y,text,uiText,2);optionsAdjustRegion(i,y,choice);accountRect(58,y+21,260,4,0x00212b3c);accountRect(58,y+21,TA_Options_Get(i)*260/100,4,0x0067b8e8);}
  menuRow(329,"RESTORE VOLUME DEFAULTS",choice==3,2);menuRow(367,"BACK",choice==4,2);
  accountFooter("A INCREASE  B BACK",optionsSaveStatus());
 }
}
static void optionsEditColors(){
 static const char*shapes[]={"HEART","CIRCLE","TRIANGLE","STAR","UPSIDE-DOWN TRIANGLE","DIAMOND"};unsigned choice=0;int picked=-1;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pressed&0x100)return;choice=menuMove(choice,10,pressed);unsigned scheme=TA_Options_Get(TA_OPT_SCHEME);
  if(choice<6&&(pressed&0xc0u)){unsigned value=(TA_Options_Assignment(scheme,choice)+(pressed&0x40?5:1))%6;for(unsigned i=0;i<6;i++)if(TA_Options_Assignment(scheme,i)==value){TA_Options_Swap(choice,i);break;}picked=-1;optionsCommit();}
  if(pressed&1){if(choice<6){if(picked<0)picked=(int)choice;else{TA_Options_Swap((unsigned)picked,choice);picked=-1;optionsCommit();}}
   else if(choice<8){TA_Options_Rotate(choice==6?-1:1);picked=-1;optionsCommit();}else if(choice==8){TA_Options_ResetScheme();picked=-1;optionsCommit();}else return;}
  char sub[80];snprintf(sub,sizeof sub,"%s - %s",TA_Options_SchemeName(scheme),picked<0?"PICK TWO PANELS TO SWAP":"SELECT THE OTHER PANEL");accountFrame("EDIT PANEL COLORS",sub);
  unsigned first=choice>5?choice-5:0;
  for(unsigned row=0;row<6&&first+row<10;row++){unsigned i=first+row,y=161+row*37;char label[72];
   if(i<6)snprintf(label,sizeof label,"%s%s",picked==(int)i?"> ":"",shapes[i]);else strcpy(label,i==6?"ROTATE COLORS LEFT":i==7?"ROTATE COLORS RIGHT":i==8?"RESET THIS SCHEME":"BACK");
   menuRow(y,i<6?"":label,i==choice,2);if(i<6){ta_vs_lan_text_box(screen,58,y,230,14,label,i==choice?uiGold:uiText,2);optionsChip(302,y-3,i,TA_Options_PreviewColor(scheme,i),26);optionsAdjustRegion(row,y,choice-first);}
  }
  accountFooter("A PICK / SWAP  B BACK",optionsSaved?"SCROLL FOR ROTATE / RESET":optionsSaveStatus());
 }
}
static void optionsColorMenu(){
 unsigned choice=TA_Options_Get(TA_OPT_SCHEME);TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pressed&0x100)return;choice=menuMove(choice,8,pressed);
  if(pressed&1){if(choice<6){TA_Options_Set(TA_OPT_SCHEME,choice);optionsCommit();}else if(choice==6){optionsEditColors();menuBegin(&pad);}else return;}
  accountFrame("CUSTOM BLOCK COLOR SCHEME","CHOOSE A PRESET OR REASSIGN ITS COLORS");unsigned first=choice>5?choice-5:0;
  for(unsigned row=0;row<6&&first+row<8;row++){unsigned i=first+row,y=161+row*37;char label[64];
   if(i<6)snprintf(label,sizeof label,"%s%s",TA_Options_Get(TA_OPT_SCHEME)==i?"* ":"",TA_Options_SchemeName(i));else strcpy(label,i==6?"EDIT ACTIVE SCHEME":"BACK");menuRow(y,label,i==choice,2);
   if(i<6)for(unsigned b=0;b<6;b++)optionsChip(302+b*27,y-3,b,TA_Options_PreviewColor(i,b));
  }
  accountFooter("A APPLY / SELECT  B BACK",optionsSaved?"SCROLL FOR EDIT AND BACK":optionsSaveStatus());
 }
}
#if defined(TA_PC)
static void optionsGraphicsMenu(){
 static const char*names[]={"AUTOMATIC","OPENGL","DIRECT3D 11","DIRECT3D 12","SOFTWARE"};
 unsigned choice=TA_PC_RendererChoice();TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pressed&0x100)return;choice=menuMove(choice,6,pressed);
  if(pressed&1u){if(choice==5)return;TA_PC_SetRenderer(choice);}
  char sub[80];snprintf(sub,sizeof sub,"CURRENT: %s",TA_PC_RendererName());accountFrame("GRAPHICS OPTIONS",sub);
  for(unsigned i=0;i<5;i++){char text[64];snprintf(text,sizeof text,"%s%s",TA_PC_RendererChoice()==i?"* ":"",names[i]);menuRow(165+i*37,text,choice==i,2);}
  menuRow(362,"BACK",choice==5,2);
  unsigned status=TA_PC_RendererStatus();const char*note=status==1?"APPLYING GRAPHICS OPTION":status==3?"UNAVAILABLE - CURRENT KEPT":status==4?"APPLIED - SAVE FAILED":status==5?"LAUNCH OVERRIDE ACTIVE":status==6?"UNAVAILABLE - USING FALLBACK":TA_PC_RendererChoice()==0?"AUTO REMEMBERS STUTTER RECOVERY":"SAVED - AUTO RECOVERY OFF";
  accountFooter("A APPLY  B BACK",note);
 }
}
static void optionsFullscreenMenu(){
 static const char*sizes[]={"960 X 720","1280 X 720","1280 X 960","1920 X 1080"};unsigned choice=0;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pressed&0x100)return;choice=menuMove(choice,4,pressed);
  if(pressed&(1u|0xc0u)){if(choice==0){TA_Options_Set(TA_OPT_FULLSCREEN,TA_Options_Get(TA_OPT_FULLSCREEN)^1u);TA_PC_ApplyDisplayOptions(0);optionsCommit();}
   else if(choice==1){TA_Options_Set(TA_OPT_WINDOW_SIZE,(TA_Options_Get(TA_OPT_WINDOW_SIZE)+(pressed&0x40?3:1))%4);TA_PC_ApplyDisplayOptions(1);optionsCommit();}
   else if(pressed&1){if(choice==2){TA_Options_Set(TA_OPT_FULLSCREEN,0);TA_Options_Set(TA_OPT_WINDOW_SIZE,0);TA_PC_ApplyDisplayOptions(1);optionsCommit();}else return;}}
  accountFrame("FULLSCREEN OPTIONS","DISPLAY SETTINGS - F11 ALSO TOGGLES MODE");char label[80];snprintf(label,sizeof label,"MODE: %s",TA_Options_Get(TA_OPT_FULLSCREEN)?"FULLSCREEN":"WINDOWED");menuRow(172,label,choice==0,2);
  snprintf(label,sizeof label,"WINDOW SIZE: %s",sizes[TA_Options_Get(TA_OPT_WINDOW_SIZE)]);menuRow(225,label,choice==1,2);menuRow(298,"RESTORE DISPLAY DEFAULTS",choice==2,2);menuRow(358,"BACK",choice==3,2);accountFooter("A CHANGE  B BACK",optionsSaveStatus());
 }
}
#endif
static void accountOptionsMenu(){
 static const char*items[]={"CONTROLS",
#if defined(TA_PC)
 "FULLSCREEN OPTIONS","GRAPHICS OPTIONS",
#endif
 "VOLUME OPTIONS","CUSTOM BLOCK COLOR SCHEME","BACK"};
 const unsigned count=sizeof items/sizeof items[0];unsigned choice=0;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pressed&0x100)return;choice=menuMove(choice,count,pressed);if(pressed&1){
  if(choice==count-1)return;if(choice==0)accountControlsMenu();
#if defined(TA_PC)
  else if(choice==1)optionsFullscreenMenu();
  else if(choice==2)optionsGraphicsMenu();
#endif
  else if(choice==count-3)optionsVolumeMenu();else optionsColorMenu();menuBegin(&pad);
 }
 accountFrame("OPTIONS","PERSONAL SETTINGS SAVED ON THIS DEVICE");for(unsigned i=0;i<count;i++)menuRow(163+i*(count>5?39:43),items[i],choice==i,2);accountFooter("A SELECT  B BACK",optionsSaveStatus());
 }
}
#endif
