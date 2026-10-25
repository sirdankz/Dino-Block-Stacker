#include "ta_menu_touch.h"
/* N22 menus only. Account requests never run emulation or wait on live input. */
#ifndef TA_N12_CACHE_DIR
#define TA_N12_CACHE_DIR "U:\\00000001"
#endif
static struct{char name[16];uint8_t token[32];}savedProfiles[8];
static unsigned profileCount,profileActive;static bool profilesRead;
static TaAccountProfile ownProfile,otherProfile,ladder[8];
static unsigned ladderCount,ladderOffset,ladderTotal;
static struct{uint32_t id,rating;char name[17];unsigned speed;}namedRooms[8];
static unsigned namedCount;
static char accountNotice[64]="CREATE A PROFILE OR SIGN IN";
static int accountLastStatus=-1;
static bool accountSavedLocally;
static unsigned optionChoice,resultChoice;
static unsigned reportOutcome,reportDone;static uint32_t reportEpoch,reportAt;
static void accountPath(char*out,unsigned size,const char*name){snprintf(out,size,"%s/%s",TA_N12_CACHE_DIR,name);}
#ifndef TA_VS_PROFILE_DIR
#define TA_VS_PROFILE_DIR TA_N12_CACHE_DIR
#endif
#include "ta_vs_profile_location.h"
static void accountProfiles(bool write){
 uint8_t blob[TA_PS_PAYLOAD];DWORD error=0;
 if(write){memcpy(blob,"TAP14",5);blob[5]=1;blob[6]=(uint8_t)profileCount;blob[7]=(uint8_t)profileActive;memcpy(blob+8,savedProfiles,sizeof savedProfiles);
  accountSavedLocally=ta_profile_store_save(TA_VS_PROFILE_DIR,blob,&error)!=0;
  TA_Log("[TA N22 ACCOUNT] saved_local=%u native_io=1 verified=%u profiles=%u error=%lu location=%s\n",(unsigned)accountSavedLocally,(unsigned)accountSavedLocally,profileCount,(unsigned long)error,TA_VS_PROFILE_DIR);
  if(!accountSavedLocally)strcpy(accountNotice,"SIGNED IN - LOCAL SAVE UNAVAILABLE");memset(blob,0,sizeof blob);return;
 }
 if(profilesRead)return;unsigned migrated=0;
#if defined(TA_ANDROID)
 int source=ta_profile_location_load(TA_VS_PROFILE_DIR,TA_VS_PROFILE_DIR,blob,&error,&migrated);
#else
 int source=ta_profile_location_load(TA_VS_PROFILE_DIR,"U:\\TA_NATIVE_VS",blob,&error,&migrated);
#endif
 #if !defined(TA_ANDROID)
 if(!source)source=ta_profile_location_load(TA_VS_PROFILE_DIR,"E:\\TA_NATIVE_VS",blob,&error,&migrated);
#endif
 profilesRead=source||error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND;
 if(source){profileCount=blob[6];profileActive=blob[7];memcpy(savedProfiles,blob+8,sizeof savedProfiles);accountSavedLocally=migrated!=2;}
 TA_Log("[TA N22 ACCOUNT] loaded_profiles=%u source=%d native_io=1 error=%lu migration=%u location=%s\n",profileCount,source,(unsigned long)error,migrated,TA_VS_PROFILE_DIR);memset(blob,0,sizeof blob);
}
static void accountRect(int x,int y,int w,int h,uint32_t color){for(int j=y;j<y+h&&j<480;j++)for(int i=x;i<x+w&&i<512;i++)if(i>=0&&j>=0)screen[j*512+i]=color;}
static const uint32_t uiText=0x00f1f3f8,uiMuted=0x00bdc7d5,uiGold=0x00f1cb72,uiInk=0x00101928;
/* The framebuffer is opaque XRGB, so a translucent selector must be
   alpha-composited into its existing pixels before the label is rendered.
   This is menu-only work and never touches the 60 FPS SNES frame path. */
static void accountHighlight(int x,int y,int w,int h){
 const unsigned alpha=104,keep=256-alpha;
 const unsigned tintR=0x67,tintG=0xb8,tintB=0xe8;
 TaMenuRect bounds=ta_menu_highlight_region(y+h/2);
 const int left=x<bounds.left?bounds.left:x,top=y<bounds.top?bounds.top:y,right=x+w>bounds.right?bounds.right:x+w,bottom=y+h>bounds.bottom?bounds.bottom:y+h;
 for(int yy=top;yy<bottom;yy++)for(int xx=left;xx<right;xx++){
  uint32_t*p=&screen[yy*512+xx],c=*p;
  const unsigned r=((((c>>16)&255)*keep)+(tintR*alpha))>>8;
  const unsigned g=((((c>>8)&255)*keep)+(tintG*alpha))>>8;
  const unsigned b=(((c&255)*keep)+(tintB*alpha))>>8;
  *p=(r<<16)|(g<<8)|b;
 }
}
static void accountSelected(int y,int h){accountHighlight(35,y,442,h);}
static void accountButtonLine(int x,int y,const char*line,bool actions=true){
 if(!line)return;unsigned count=(unsigned)strlen(line),scale=2,advance=12;
 if(x<TA_MENU_LEFT)x=TA_MENU_LEFT;
 if(count&&count*advance-scale>(unsigned)(TA_MENU_RIGHT-x))advance=11;
 if(count&&count*advance-scale>(unsigned)(TA_MENU_RIGHT-x)){scale=1;advance=6;}
 if(actions)menuTouchFooter(x,y,line,advance);
 int h=(int)scale*7;if(y<TA_MENU_FOOTER_TEXT_TOP)y=TA_MENU_FOOTER_TEXT_TOP;if(y+h>TA_MENU_FOOTER_TEXT_BOTTOM)y=TA_MENU_FOOTER_TEXT_BOTTOM-h;
 for(unsigned i=0;line[i]&&x+(int)(5*scale)<=TA_MENU_RIGHT;i++,x+=(int)advance){char text[2]={line[i],0};bool button=actions&&(!i||(i>=2&&line[i-1]==' '&&line[i-2]==' '))&&line[i+1]==' '&&(line[i]=='A'||line[i]=='B'||line[i]=='X'||line[i]=='Y');
  uint32_t c=button?(line[i]=='A'?0x0096e3b2:line[i]=='B'?0x00f1a0aa:line[i]=='X'?0x009cbdff:uiGold):uiMuted;
  ta_vs_lan_text_box(screen,x,y,(int)advance,h,text,c,2);
 }
}
static void accountFrame(const char*title,const char*sub){
 TA_Menu_Begin(title);menuTouchRowIndex=0;
 ta_n17_backdrop();ta_menu_text(screen,36,34,"DINO BLOCK STACKER",0x007dd9e8,3);ta_menu_text(screen,406,34,"V1.1.7",uiMuted,2);ta_vs_lan_text_box(screen,36,65,440,24,title,uiText,4);
 if(sub)ta_vs_lan_text_box(screen,36,97,440,32,sub,uiMuted,2);
 /* The shared skin already includes the content and footer surfaces. */
}
/* Right-aligned chat legend: a native pixel triangle avoids unsupported UTF-8 glyphs. */
static void accountChatHint(int y){
 const int x=270;ta_vs_lan_text_box(screen,x,y,120,14,"OPEN CHAT:",uiMuted,2);
 static const unsigned widths[7]={1,2,3,5,3,2,1};
 for(unsigned row=0;row<7;row++)accountRect(x+122+10-(int)(2*widths[row]),y+(int)row*2,(int)widths[row]*2,2,0x00000000u);
 ta_vs_lan_text_box(screen,x+136,y,70,14,"BUTTON",uiMuted,2);
 menuTouchAction(x-4,y>=430?430:y-5,210,y>=430?18:24,4u);
}
static void accountFooter(const char*primaryLine,const char*secondaryLine,unsigned chatLine=0,bool secondaryControls=false){
 accountButtonLine(36,TA_MENU_FOOTER_PRIMARY,primaryLine);if(secondaryLine)accountButtonLine(36,TA_MENU_FOOTER_SECONDARY,secondaryLine,secondaryControls);
 if(chatLine)accountChatHint(chatLine==2?TA_MENU_FOOTER_SECONDARY:TA_MENU_FOOTER_PRIMARY);
 TA_Video_Submit((const uint8_t*)screen);
}
static bool accountOpen(void){
 accountAuthenticated=false;memset(&accountLink,0,sizeof accountLink);memset(&ownProfile,0,sizeof ownProfile);memset(&otherProfile,0,sizeof otherProfile);namedCount=0;reportDone=reportOutcome=reportEpoch=0;accountProfiles(false);
 uint8_t key[32],secret[32];memcpy(key,ta_n17_account_key,sizeof key);
 if(XNetRandom(secret,sizeof secret)){strcpy(accountNotice,"ACCOUNT NETWORK SETUP FAILED");return false;}int result=ta_account_init(&accountLink,secret,key);memset(secret,0,sizeof secret);return result==0;
}
static const char*accountStatus(unsigned s){return s==2?"USERNAME ALREADY TAKEN":s==3?"SIGN IN FAILED - CHECK PASSWORD":s==4?"PLEASE WAIT THEN TRY AGAIN":s==5?"PLEASE SIGN IN FIRST":s==6?"MATCH NOT ELIGIBLE FOR RANKING":s==7?"WAITING FOR BOTH MATCH RESULTS":s==8?"RESULTS DISAGREE - STATS UNCHANGED":s==10?"MESSAGE BLOCKED - HATE SPEECH":s==11?"SLOW DOWN - TRY AGAIN":s==12?"INVALID CHAT MESSAGE":"CHECK NAME AND PASSWORD";}
static bool accountCall(unsigned command,const void*data,unsigned bytes,const char*label,bool quiet=false){
 accountLastStatus=-1;
 if(!accountLink.ready){strcpy(accountNotice,"UPDATE SERVER AND COPY ACCOUNT KEY");return false;}
 if(ta_account_request(&accountLink,&publicRoom,command,data,bytes)){strcpy(accountNotice,"CONNECTING - PLEASE TRY AGAIN");return false;}
 uint32_t start=GetTickCount();uint16_t pads[2];TA_Input_Poll(pads);uint16_t previous=pads[0];
 while(accountLink.pending&&(uint32_t)(GetTickCount()-start)<6000){
  if(pump(false)){accountLink.pending=0;strcpy(accountNotice,"ROOM SERVER UNAVAILABLE");return false;}
  if(!accountLink.pending)break;
  TA_Input_Poll(pads);unsigned pressed=pads[0]&~previous;previous=pads[0];int ignoredTouch=0;pressed|=TA_Menu_Read(&ignoredTouch)&0x100u;if(pressed&0x100){accountLink.pending=0;accountLastStatus=-2;return false;}
  if(quiet)TA_Video_Submit((const uint8_t*)screen);else{accountFrame(label,"SECURE ACCOUNT CONNECTION");ta_menu_text(screen,44,230,"PLEASE WAIT",uiText,3);accountFooter("B CANCEL",NULL);}
 }
 if(accountLink.pending){accountLink.pending=0;strcpy(accountNotice,"SERVER TIMEOUT - PLEASE RETRY");return false;}
 if(accountLink.replyBytes<2||accountLink.reply[0]!=command){strcpy(accountNotice,"INVALID SERVER RESPONSE");return false;}
 accountLastStatus=accountLink.reply[1];if(accountLastStatus){strcpy(accountNotice,accountStatus((unsigned)accountLastStatus));if(accountLastStatus==5)accountAuthenticated=false;return false;}return true;
}
/* Uniform menu input: A selects, B returns, held D-pad repeats without
   repeating A/B or leaking the opening A press into the next menu. */
typedef struct{uint16_t previous,held;uint32_t repeated;bool repeating;}TaMenuPad;
static void menuBegin(TaMenuPad*p){TA_Menu_Reset();menuTouchDelta=0;menuTouchKey=~0u;menuTouchPressed=false;uint16_t pads[2];TA_Input_Poll(pads);p->previous=pads[0];p->held=pads[0]&0xf0;p->repeated=GetTickCount();p->repeating=false;}
static unsigned menuUpdate(TaMenuPad*p,uint16_t buttons){unsigned pressed=buttons&~p->previous;p->previous=buttons;uint16_t held=buttons&0xf0;uint32_t now=GetTickCount();if(held!=p->held){p->held=held;p->repeated=now;p->repeating=false;}else if(held&&(uint32_t)(now-p->repeated)>=(p->repeating?100u:320u)){pressed|=held;p->repeated=now;p->repeating=true;}int delta=0;unsigned touch=TA_Menu_Read(&delta);menuTouchDelta=delta;menuTouchPressed=touch!=0||delta!=0;menuTouchKey=(touch&0x1000000u)?(touch>>16)&255u:~0u;return pressed|(touch&0xffffu)|(delta?0x10000u:0u);}
static unsigned menuRead(TaMenuPad*p){uint16_t pads[2];TA_Input_Poll(pads);return menuUpdate(p,pads[0]);}
static unsigned menuMove(unsigned selected,unsigned count,unsigned pressed){if(!count)return 0;if(menuTouchDelta){int next=(int)selected+menuTouchDelta;selected=next<0?0u:next>=(int)count?count-1:(unsigned)next;menuTouchDelta=0;}if(pressed&0x10)selected=selected?selected-1:count-1;if(pressed&0x20)selected=(selected+1)%count;return selected;}
static void menuRow(unsigned y,const char*label,bool selected,unsigned scale=3){
 unsigned h=scale>=4?21u:14u;if(y<160u)y=160u;if(y+h>TA_MENU_BODY_TEXT_BOTTOM)y=TA_MENU_BODY_TEXT_BOTTOM-h;
 menuTouchChoice(TA_MENU_LEFT,(int)y-6,TA_MENU_RIGHT-TA_MENU_LEFT,(int)h+12,menuTouchRowIndex++,selected);
 if(selected)accountSelected((int)y-6,(int)h+12);
 ta_vs_lan_text_box(screen,58,(int)y,TA_MENU_RIGHT-58,(int)h,label,selected?uiGold:uiText,(int)scale);
}
static void menuActions(const char*const*labels,unsigned count,unsigned selected,bool submit=true,const char*hint="D-PAD CHOOSE  A SELECT  B BACK"){
 if(!count)return;unsigned width=(TA_MENU_RIGHT-TA_MENU_LEFT)/count;
 for(unsigned i=0;i<count;i++){int x=TA_MENU_LEFT+(int)(i*width);
  menuTouchChoice(x,404,(int)width-4,24,menuTouchRowIndex++,i==selected);
  if(i==selected)accountHighlight(x,407,(int)width-4,24);
  ta_vs_lan_text_box(screen,x+8,412,(int)width-16,14,labels[i],i==selected?uiGold:uiText,2);
 }
 accountButtonLine(36,TA_MENU_FOOTER_SECONDARY,hint);if(submit)TA_Video_Submit((const uint8_t*)screen);
}
static bool accountKeyboard(const char*title,const char*label,char*out,unsigned maximum,bool password,unsigned minimum=4){
 static const char keys[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";unsigned key=0,length=0;char value[33]={0};TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(menuTouchKey<40)key=menuTouchKey;if(pump(false))return false;
  if(pressed&0x100){memset(value,0,sizeof value);return false;}if(pressed&0x40)key=(key+39)%40;if(pressed&0x80)key=(key+1)%40;if(pressed&0x10)key=(key+32)%40;if(pressed&0x20)key=(key+8)%40;
  if((pressed&2)||((pressed&1)&&key==38)){if(length)value[--length]=0;}
  else if((pressed&1)&&key<38&&length<maximum){value[length++]=keys[key];value[length]=0;}
  if(((pressed&8)||((pressed&1)&&key==39))&&length>=minimum){memcpy(out,value,length+1);memset(value,0,sizeof value);return true;}
  char hint[80];snprintf(hint,sizeof hint,"%u TO %u CHARACTERS%s",minimum,maximum,password?" - PASSWORD HIDDEN":" - A-Z 0-9 - _");accountFrame(title,hint);char shown[40]={0};if(password)memset(shown,'X',length);else memcpy(shown,value,length);ta_menu_text(screen,42,156,label,uiMuted,2);ta_menu_text(screen,42,179,shown,uiText,3);
  for(unsigned k=0;k<40;k++){unsigned x=48+(k%8)*54,y=224+(k/8)*30;menuTouchAction(x-5,y-7,52,26,0x1000001u|(k<<16));if(k==key)accountHighlight(x-5,y-7,52,26);char ch[2]={k<38?keys[k]:' ',0};ta_vs_lan_text_box(screen,x,y,44,14,k==38?"DEL":k==39?"DONE":ch,k==key?uiGold:uiText,2);}
  accountFooter("D-PAD MOVE  A SELECT  B CANCEL","DEL: ERASE   DONE: CONTINUE");
 }
}
static bool accountAuthenticate(unsigned command,const char*name,const void*credential){
 uint8_t data[48]={0};memcpy(data,name,strlen(name));memcpy(data+16,credential,32);
 accountAuthenticated=false;
 bool ok=accountCall(command,data,sizeof data,command==1?"CREATING PROFILE":"SIGNING IN",command==3);memset(data,0,sizeof data);if(!ok)return false;
 TaAccountProfile authenticatedProfile;if(accountLink.replyBytes!=78||ta_account_profile(&authenticatedProfile,accountLink.reply+34,44)){strcpy(accountNotice,"INVALID PROFILE RESPONSE");return false;}
 ownProfile=authenticatedProfile;unsigned index=profileCount;for(unsigned i=0;i<profileCount;i++)if(!strcmp(savedProfiles[i].name,ownProfile.name))index=i;
 if(index==profileCount){if(profileCount==8){strcpy(accountNotice,"REMOVE A LOCAL PROFILE FIRST");accountAuthenticated=false;return false;}profileCount++;}
 memset(&savedProfiles[index],0,sizeof savedProfiles[index]);strcpy(savedProfiles[index].name,ownProfile.name);memcpy(savedProfiles[index].token,accountLink.reply+2,32);profileActive=index;accountAuthenticated=true;TA_Log("[TA N22 ACCOUNT] signed_in=1 profile=%s saved_profiles=%u\n",ownProfile.name,profileCount);strcpy(accountNotice,"SIGNED IN - READY TO PLAY");accountProfiles(true);return true;
}
static void accountRemoveMenu(void){
 unsigned selected=profileActive;TaMenuPad pad;menuBegin(&pad);for(;;){unsigned pressed=menuRead(&pad);if(pump(false)||pressed&0x100)return;selected=menuMove(selected,profileCount+1,pressed);
  if(pressed&1){if(selected==profileCount)return;if(accountAuthenticated&&!strcmp(ownProfile.name,savedProfiles[selected].name)){accountAuthenticated=false;memset(&ownProfile,0,sizeof ownProfile);}for(unsigned i=selected;i+1<profileCount;i++)savedProfiles[i]=savedProfiles[i+1];memset(&savedProfiles[--profileCount],0,sizeof savedProfiles[0]);if(profileActive==selected)profileActive=0;else if(profileActive>selected)profileActive--;if(profileActive>=profileCount)profileActive=0;accountProfiles(true);strcpy(accountNotice,"REMOVED FROM THIS XBOX ONLY");return;}
  accountFrame("REMOVE LOCAL PROFILE","ONLINE ACCOUNT AND STATS WILL BE KEPT");unsigned first=selected>5?selected-5:0;for(unsigned row=0;row<6&&first+row<profileCount+1;row++){unsigned i=first+row;menuRow(160+row*36,i<profileCount?savedProfiles[i].name:"BACK",i==selected);}accountFooter("D-PAD MOVE  A REMOVE  B BACK",NULL);
 }
}
static void accountProfileMenu(void){
 accountProfiles(false);unsigned selected=profileActive;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false)||pressed&0x100)return;selected=menuMove(selected,profileCount+4,pressed);
  if(pressed&1){
   if(selected<profileCount){
    if(accountAuthenticated&&!strcmp(ownProfile.name,savedProfiles[selected].name))return;
    if(accountAuthenticate(3,savedProfiles[selected].name,savedProfiles[selected].token))return;
    if(accountLastStatus==3){char pass[33]={0};bool ok=accountKeyboard("SIGN IN","PASSWORD",pass,32,true)&&accountAuthenticate(2,savedProfiles[selected].name,pass);memset(pass,0,sizeof pass);if(ok)return;}
   }else if(selected==profileCount+3)return;
   else if(selected==profileCount+2){accountRemoveMenu();selected=profileActive;}
   else if(profileCount==8)strcpy(accountNotice,"REMOVE A LOCAL PROFILE FIRST");
   else{char name[16]={0},pass[33]={0},confirm[33]={0};bool create=selected==profileCount;
    if(accountKeyboard(create?"CREATE PROFILE":"SIGN IN","USERNAME",name,15,false,create?4:3)&&accountKeyboard(create?"CREATE PROFILE":"SIGN IN","PASSWORD",pass,32,true)){
     if(!create||(accountKeyboard("CONFIRM PASSWORD","PASSWORD",confirm,32,true)&&!strcmp(pass,confirm))){bool ok=accountAuthenticate(create?1:2,name,pass);memset(pass,0,sizeof pass);memset(confirm,0,sizeof confirm);if(ok)return;}else strcpy(accountNotice,"PASSWORDS DO NOT MATCH");}
    memset(pass,0,sizeof pass);memset(confirm,0,sizeof confirm);
   }
   menuBegin(&pad);
  }
  accountFrame("PROFILES",accountNotice);unsigned first=selected>5?selected-5:0;
  for(unsigned row=0;row<6&&first+row<profileCount+4;row++){unsigned i=first+row;char text[64];if(i<profileCount)snprintf(text,sizeof text,"%s%s",savedProfiles[i].name,accountAuthenticated&&!strcmp(ownProfile.name,savedProfiles[i].name)?" - ACTIVE":"");else strcpy(text,i==profileCount?"CREATE PROFILE":i==profileCount+1?"SIGN IN / RECOVER":i==profileCount+2?"REMOVE LOCAL PROFILE":"BACK");menuRow(160+row*36,text,i==selected);}
  accountFooter("D-PAD MOVE  A SELECT  B BACK","SAVED PROFILES SIGN IN AUTOMATICALLY");
 }
}
static bool accountEnsure(void){
 if(accountAuthenticated)return true;
 if(!publicRoom.haveCookie){strcpy(accountNotice,"CONNECTING - PLEASE WAIT");return false;}
 if(profileCount){if(profileActive>=profileCount)profileActive=0;if(accountAuthenticate(3,savedProfiles[profileActive].name,savedProfiles[profileActive].token))return true;if(accountLastStatus!=3&&accountLastStatus!=5)return false;}
 strcpy(accountNotice,"PLEASE SIGN IN TO HOST OR JOIN");accountProfileMenu();return accountAuthenticated;
}
static void accountRefreshProfile(void){if(accountAuthenticated&&accountCall(4,NULL,0,"REFRESHING PROFILE",true)&&accountLink.replyBytes==46)ta_account_profile(&ownProfile,accountLink.reply+2,44);}
static void accountDirectory(void){
 if(!accountAuthenticated)return;if(!accountCall(8,NULL,0,"REFRESHING ROOMS",true))return;const uint8_t*b=accountLink.reply+2;unsigned bytes=accountLink.replyBytes-2;if(!bytes||b[0]>8||bytes!=1+b[0]*25)return;namedCount=b[0];
 for(unsigned i=0;i<namedCount;i++){const uint8_t*q=b+1+i*25;namedRooms[i].id=((uint32_t)q[0]<<24)|((uint32_t)q[1]<<16)|((uint32_t)q[2]<<8)|q[3];namedRooms[i].speed=q[4];memcpy(namedRooms[i].name,q+5,16);namedRooms[i].name[16]=0;namedRooms[i].rating=((uint32_t)q[21]<<24)|((uint32_t)q[22]<<16)|((uint32_t)q[23]<<8)|q[24];}
}
static bool partyLadder;
#include "ta_egg_ranks.h"
#include "ta_vs_ladder_scroll.h"
static TaLadderScroll ladderScroll;
static bool ladderLoading,ladderFailed;
static unsigned ladderRequested;
static uint32_t ladderRequestAt;
static void accountLadderTick(void){
 unsigned command=partyLadder?TA_ACCOUNT_PARTY_LADDER:TA_ACCOUNT_LADDER;
 uint32_t now=GetTickCount();
 if(ladderLoading){
  if(accountLink.pending){if((uint32_t)(now-ladderRequestAt)<6000u)return;accountLink.pending=0;ladderFailed=true;}
  else if(accountLink.replyBytes<2||accountLink.reply[0]!=command||accountLink.reply[1]||!ta_ladder_accept(&ladderScroll,ladderRequested,accountLink.reply+2,accountLink.replyBytes-2))ladderFailed=true;
  ladderLoading=false;
 }
 int missing=ta_ladder_missing(&ladderScroll);
 if(missing<0||ladderFailed||accountLink.pending||(uint32_t)(now-ladderRequestAt)<250u)return;
 uint8_t data[2]={(uint8_t)((unsigned)missing>>8),(uint8_t)missing};
 if(ta_account_request(&accountLink,&publicRoom,command,data,2)){ladderFailed=true;return;}
 ladderRequested=(unsigned)missing;ladderRequestAt=now;ladderLoading=true;
}
static void accountStats(void){
 if(!accountAuthenticated){ta_menu_text(screen,44,182,"YOUR VERSUS JOURNEY STARTS HERE",uiText,3);ta_menu_text(screen,44,228,accountNotice,uiMuted,2);ta_menu_text(screen,44,302,"CREATE PROFILE OR SIGN IN",uiGold,3);return;}
 char text[80];snprintf(text,sizeof text,"%s",ownProfile.name);ta_menu_text(screen,44,159,text,uiText,4);
 snprintf(text,sizeof text,"%s   %lu RATING",ta_account_tier(ownProfile.rating),(unsigned long)ownProfile.rating);eggEmblem(44,199,ownProfile.rating,1);ta_menu_text(screen,90,209,text,uiGold,2);
 if(ownProfile.rank)snprintf(text,sizeof text,"GLOBAL RANK %lu",(unsigned long)ownProfile.rank);else strcpy(text,"GLOBAL RANK - PLAY YOUR FIRST MATCH");ta_menu_text(screen,44,245,text,uiMuted,2);
 snprintf(text,sizeof text,"WINS %lu   LOSSES %lu",(unsigned long)ownProfile.wins,(unsigned long)ownProfile.losses);ta_menu_text(screen,44,280,text,uiText,3);
 snprintf(text,sizeof text,"BEST STREAK %lu",(unsigned long)ownProfile.best);ta_menu_text(screen,44,319,text,uiMuted,2);
 snprintf(text,sizeof text,"STREAK %ld   MATCHES %lu",(long)ownProfile.streak,(unsigned long)(ownProfile.wins+ownProfile.losses+ownProfile.draws));ta_menu_text(screen,44,350,text,uiMuted,2);
}
static uint32_t leaderboardColor(uint32_t place){static const uint32_t colors[5]={0x00ffd166,0x00cad5e8,0x00e8a174,0x0065d9d1,0x00b5a1ff};return place>=1&&place<=5?colors[place-1]:uiText;}
static void accountDrawLadder(void){
 ta_menu_text(screen,36,158,"POS",uiMuted,2);ta_menu_text(screen,108,158,"PLAYER",uiMuted,2);ta_menu_text(screen,272,158,"RANK",uiMuted,2);ta_menu_text(screen,TA_MENU_LADDER_RATING_X,158,"RATING",uiMuted,2);
 if(ladderScroll.known&&!ladderScroll.total)ta_menu_text(screen,44,222,"FINISH A PUBLIC MATCH TO RANK",uiText,3);
 for(unsigned row=0;row<TA_LADDER_VISIBLE&&ladderScroll.first+row<ladderScroll.total;row++){
  unsigned index=ladderScroll.first+row,y=TA_MENU_LADDER_TOP+row*TA_LADDER_ROW_HEIGHT;const TaAccountProfile*p=ta_ladder_row(&ladderScroll,index);
  if(!p){ta_menu_text(screen,108,y+10,ladderFailed?"UNAVAILABLE":"LOADING...",uiMuted,2);continue;}
  uint32_t color=leaderboardColor(p->rank);char text[24];if(!strcmp(p->name,ownProfile.name))accountHighlight(36,y+1,440,TA_LADDER_ROW_HEIGHT-2);
  snprintf(text,sizeof text,"%lu",(unsigned long)p->rank);ta_menu_text(screen,36,y+10,text,color,2);
  {char name[14];snprintf(name,sizeof name,"%.13s",p->name);ta_menu_text(screen,108,y+10,name,color,2);}
  eggEmblem(270,y,p->rating,1);ta_menu_text(screen,310,y+10,eggShort(p->rating),color,2);
  if(p->rating>=1000000u)snprintf(text,sizeof text,"%luM",(unsigned long)(p->rating/1000000u));else if(p->rating>=100000u)snprintf(text,sizeof text,"%luK",(unsigned long)(p->rating/1000u));else snprintf(text,sizeof text,"%lu",(unsigned long)p->rating);
  ta_menu_text(screen,TA_MENU_LADDER_RATING_X,y+10,text,color,2);
 }
 if(ladderScroll.total>TA_LADDER_VISIBLE){unsigned track=TA_LADDER_VISIBLE*TA_LADDER_ROW_HEIGHT,thumb=track*TA_LADDER_VISIBLE/ladderScroll.total;if(thumb<12)thumb=12;unsigned top=TA_MENU_LADDER_TOP+(track-thumb)*ladderScroll.first/ta_ladder_max_first(&ladderScroll);accountRect(TA_MENU_LADDER_SCROLL_X,TA_MENU_LADDER_TOP,2,track,0x00273852);accountRect(TA_MENU_LADDER_SCROLL_X,top,2,thumb,0x007dd9e8);}
 char status[64];
 if(ladderFailed)strcpy(status,"LOAD FAILED - A TO RETRY");
 else if(!ladderScroll.known)strcpy(status,"LOADING LEADERBOARD...");
 else snprintf(status,sizeof status,"%u-%u OF %u%s",ladderScroll.total?ladderScroll.first+1:0,ladderScroll.first+TA_LADDER_VISIBLE<ladderScroll.total?ladderScroll.first+TA_LADDER_VISIBLE:ladderScroll.total,ladderScroll.total,ladderLoading?" - LOADING":"");
 ta_menu_text(screen,36,119,status,uiMuted,2);
}
static void accountLadderView(void){
 if(!accountEnsure())return;
 struct LadderScope{LadderScope(){TA_Input_SetLeaderboard(1);}~LadderScope(){if(ladderLoading)accountLink.pending=0;ladderLoading=false;TA_Input_SetLeaderboard(0);}} scope;
 ta_ladder_reset(&ladderScroll);ladderLoading=ladderFailed=false;ladderRequestAt=GetTickCount()-250u;
 TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad)|TA_Input_LeaderboardActions();if(pump(false)||pressed&0x100)return;
  int rows=TA_Input_LeaderboardScroll();if(pressed&0x10)rows--;if(pressed&0x20)rows++;if(pressed&0x40)rows-=(int)TA_LADDER_VISIBLE;if(pressed&0x80)rows+=(int)TA_LADDER_VISIBLE;
  if(ta_ladder_move(&ladderScroll,rows))ladderFailed=false;
  if(pressed&1){if(ladderLoading)accountLink.pending=0;ladderLoading=ladderFailed=false;ta_ladder_refresh(&ladderScroll);ladderRequestAt=GetTickCount()-250u;}
  accountLadderTick();
  accountFrame(partyLadder?"PARTY LEADERBOARD":"1V1 LEADERBOARD",partyLadder?"3-4 PLAYERS - EGG RANKS":"RANKED 1V1 - EGG RANKS");accountDrawLadder();
#if defined(TA_ANDROID)
  accountFooter("A REFRESH           B BACK","SWIPE OR D-PAD TO SCROLL");
#else
  accountFooter("A REFRESH           B BACK","D-PAD / LEFT STICK TO SCROLL");
#endif
 }
}
static void accountLadderMenu(void){
 unsigned choice=0;TaMenuPad pad;menuBegin(&pad);const char*items[]={"1V1 RANKED","PARTY 3-4 PLAYERS","BACK"};
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false)||pressed&0x100)return;choice=menuMove(choice,3,pressed);
  if(pressed&1){if(choice==2)return;partyLadder=choice==1;accountLadderView();menuBegin(&pad);}
  accountFrame("LEADERBOARDS","INDEPENDENT RATINGS - SIX EGG RANKS");for(unsigned i=0;i<3;i++)menuRow(175+i*60,items[i],choice==i,3);accountFooter("A SELECT  B BACK",NULL);
 }
}
static void accountStatsMenu(void){
 if(!accountEnsure())return;accountRefreshProfile();unsigned choice=0;TaMenuPad pad;menuBegin(&pad);const char*items[]={"PROFILES","REFRESH","BACK"};
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false)||pressed&0x100)return;unsigned moved=pressed;if(pressed&0x40)moved|=0x10;if(pressed&0x80)moved|=0x20;choice=menuMove(choice,3,moved);
  if(pressed&1){if(choice==2)return;if(choice==0)accountProfileMenu();else accountRefreshProfile();menuBegin(&pad);}
  accountFrame("MY PROFILE",accountAuthenticated?ownProfile.name:accountNotice);accountStats();menuActions(items,3,choice);
 }
}
/* Discovery uses the existing online socket and the deployed party relay.
   Party matches retain the encrypted account session for world and lobby chat. */
static void accountPartyEndpoint(uint32_t*address,uint16_t*port){
 *address=publicRoom.serverAddress?publicRoom.serverAddress:ta_vs_public_ipv4(TA_N17_SERVER);*port=TA_PARTY_PORT;
#if !defined(TA_ANDROID)
 FILE*f=fopen("D:\\ta-party-server.txt","rb");if(f){char ip[64];unsigned value;if(fscanf(f,"%63s %u",ip,&value)==2&&ta_vs_public_ipv4(ip)&&value&&value<65536){*address=ta_vs_public_ipv4(ip);*port=(uint16_t)value;}fclose(f);}
#endif
}
static void accountPartyDirectoryBegin(void){
 TaVsIdentity id=lobby.identity;static const uint8_t rules[]="DINO_PARTY_V4_EGG_RANKS";ta_rom_sha256(rules,sizeof rules-1,id.rules);
 uint64_t nonce=__builtin_readcyclecounter()^publicRoom.nonce;if(!nonce)nonce=1;
 ta_vs_party_init(&publicPartyDirectory,&id,nonce,profileCount?savedProfiles[profileActive].name:"PLAYER",GetTickCount());
 accountPartyEndpoint(&publicPartyAddress,&publicPartyPort);
 publicPartyBrowsing=true;publicPartyJoinRoom=0;publicPartyHostMode=-1;
}
static void taSocialChatMenu(int (*tick)(void)=NULL);
static int taSocialBrowseTick(void){return publicRoom.haveRoom?1:0;}
static void taSocialLobbyMenu(int (*tick)(void),unsigned kind,uint32_t room,uint64_t nonce);
static int accountBrowseRooms(void){
 if(accountAuthenticated)accountDirectory();uint32_t directoryAt=GetTickCount();
 strcpy(accountNotice,"PARTY ROOMS ACCEPT WAITING PLAYERS");
 uint32_t touchRoomList=0;unsigned choice=0;uint32_t selectedRoom=0;bool selectedParty=false;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);int browseError=pump(false);if(browseError){if(publicRoom.mode==TA_PUB_JOINING&&(browseError==-202||browseError==-203||browseError==-205||browseError==-206)){publicRoom.error=0;publicRoom.mode=TA_PUB_BROWSE;publicRoom.requested=0;strcpy(accountNotice,"ROOM UNAVAILABLE - LIST REFRESHED");}else return -1;}if(pressed&0x100)return 1;if(publicRoom.haveRoom)return 0;
  if(pressed&4u){taSocialChatMenu(taSocialBrowseTick);menuBegin(&pad);continue;}
  uint32_t now=GetTickCount();if(accountAuthenticated&&(uint32_t)(now-directoryAt)>=5000){accountDirectory();directoryAt=GetTickCount();}
  unsigned legacyCount=publicRoom.roomCount,partyCount=publicPartyDirectory.error||(uint32_t)(now-publicPartyDirectory.lastServer)>15000u?0:publicPartyDirectory.roomCount;
  unsigned count=legacyCount+partyCount;uint32_t signature=2166136261u;
  for(unsigned k=0;k<legacyCount;k++)signature=(signature^publicRoom.rooms[k].id)*16777619u;
  signature=(signature^legacyCount)*16777619u;for(unsigned k=0;k<partyCount;k++)signature=(signature^publicPartyDirectory.rooms[k].id)*16777619u;
  if(signature!=touchRoomList){touchRoomList=signature;TA_Menu_Reset();if(menuTouchPressed){menuTouchDelta=0;pressed&=~(1u|0x10000u);}}
  if(choice>=count+2)choice=count+1;choice=menuMove(choice,count+2,pressed);
  /* Navigation and selection in the same poll must not use the room ID
     cached for the previously displayed row. Render the new selection first. */
  if((pressed&1)&&!(pressed&0x10030u)){
   if(choice==count+1)return 1;
   if(choice==count){publicRoom.requested=0;accountPartyDirectoryBegin();if(accountAuthenticated)accountDirectory();}
   else if(selectedRoom){
    bool exists=false;if(selectedParty){for(unsigned n=0;n<partyCount;n++)if(publicPartyDirectory.rooms[n].id==selectedRoom)exists=true;}
    else{for(unsigned n=0;n<legacyCount;n++)if(publicRoom.rooms[n].id==selectedRoom)exists=true;}
    if(!exists)strcpy(accountNotice,"ROOM CLOSED - CHOOSE ANOTHER ROOM");
    else if(selectedParty){if(accountEnsure()){publicPartyJoinRoom=selectedRoom;publicPartyHostMode=-1;return 3;}}
    else{bool full=false;for(unsigned n=0;n<legacyCount;n++)if(publicRoom.rooms[n].id==selectedRoom&&publicRoom.rooms[n].players>=2)full=true;if(full)strcpy(accountNotice,"1V1 ROOM FULL - TRY AN OPEN ROOM");else if(accountEnsure())ta_vs_public_join(&publicRoom,selectedRoom);}
   }
   menuBegin(&pad);
  }
  accountFrame("BROWSE ROOMS","1V1 AND 3-4 PLAYER PUBLIC ROOMS");unsigned first=choice>3?choice-3:0;
  for(unsigned row=0;row<4&&first+row<count+2;row++){
   unsigned i=first+row,y=160+row*56;menuTouchChoice(36,(int)y-6,440,47,row,i==choice);char text[80];if(i==choice)accountSelected((int)y-6,47);
   if(i<legacyCount){const char*name=NULL;uint32_t rating=0;for(unsigned n=0;n<namedCount;n++)if(namedRooms[n].id==publicRoom.rooms[i].id){name=namedRooms[n].name;rating=namedRooms[n].rating;}
    if(name)snprintf(text,sizeof text,"1V1  %.15s  %s",name,ta_account_tier(rating));else snprintf(text,sizeof text,"1V1  ROOM %06lu",(unsigned long)publicRoom.rooms[i].id);
    ta_menu_text(screen,48,(int)y,text,i==choice?uiGold:uiText,2);
    snprintf(text,sizeof text,"%06lu SPEED %u %u/2 %s",(unsigned long)publicRoom.rooms[i].id,publicRoom.rooms[i].speed,publicRoom.rooms[i].players,publicRoom.rooms[i].players>=2?(publicRoom.rooms[i].phase==3?"IN GAME":"FULL"):"OPEN");ta_menu_text(screen,48,(int)y+21,text,uiMuted,2);
   }else if(i<count){const TaPartyRoom*r=publicPartyDirectory.rooms+i-legacyCount;
    snprintf(text,sizeof text,"3-4P %s  %.8s",r->mode?"FOCUS":"AUTO",r->name);ta_menu_text(screen,48,(int)y,text,i==choice?uiGold:uiText,2);
    snprintf(text,sizeof text,"%06lu SPEED %u %u/4 %s",(unsigned long)r->id,(unsigned)r->speed,(unsigned)r->count,r->count>=4?"FULL":r->phase?"IN GAME":"LOBBY");ta_menu_text(screen,48,(int)y+21,text,uiMuted,2);
   }else{strcpy(text,i==count?"REFRESH ROOMS":"BACK");ta_menu_text(screen,48,(int)y,text,i==choice?uiGold:uiText,2);}
  }
  selectedParty=choice>=legacyCount&&choice<count;selectedRoom=choice<legacyCount?publicRoom.rooms[choice].id:selectedParty?publicPartyDirectory.rooms[choice-legacyCount].id:0;
  const char*status=!accountAuthenticated?"PLEASE SIGN IN TO HOST OR JOIN":publicRoom.mode==TA_PUB_JOINING?"JOINING 1V1 ROOM":publicPartyDirectory.error==-304?"UPDATE PARTY RELAY TO R140":publicPartyDirectory.error||(uint32_t)(now-publicPartyDirectory.lastServer)>15000u?"3-4 PLAYER RELAY UNAVAILABLE":!publicPartyDirectory.haveCookie?"CHECKING 3-4 PLAYER RELAY":accountNotice;
  accountFooter(choice<count?"A JOIN  B EXIT":choice==count?"A REFRESH  B EXIT":"A EXIT  B EXIT",status,1);
 }
}
static int accountHostPublicMenu(void){
 unsigned choice=0;TaMenuPad pad;menuBegin(&pad);
 const char*items[]={"1V1 - RANKED","3-4 PLAYERS - AUTO","3-4 PLAYERS - FOCUS","BACK"};
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false))return -10060;if(pressed&0x100)return 1;if(publicRoom.haveRoom)return 0;choice=menuMove(choice,4,pressed);
  if(pressed&1){if(choice==3)return 1;if(choice){if(accountEnsure()){publicPartyJoinRoom=0;publicPartyHostMode=(int)choice-1;return 3;}menuBegin(&pad);continue;}if(accountEnsure()){lobby.role=0;ta_vs_public_host(&publicRoom);return 1;}menuBegin(&pad);}
  accountFrame("HOST PUBLIC ROOM","CHOOSE YOUR MATCH TYPE");for(unsigned i=0;i<4;i++)menuRow(160+i*54,items[i],i==choice,2);
  accountFooter("D-PAD MOVE  A HOST  B BACK","PARTY RANKS REQUIRE 3 VERIFIED ACCOUNTS");
 }
}
#include "ta_n22_social.h"
static int accountPublicRoomsMenu(void){
 unsigned choice=0;TaMenuPad pad;menuBegin(&pad);
 const char*items[]={"BROWSE ROOMS","HOST PUBLIC ROOM","BACK"};
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false))return -10060;
  if(pressed&0x100)return (publicRoom.haveRoom||publicRoom.mode==TA_PUB_HOST||publicRoom.mode==TA_PUB_JOINING)?2:1;
  if(publicRoom.haveRoom)return 0;
  choice=menuMove(choice,3,pressed);
  if(pressed&1){if(choice==2)return 1;
   if(choice==0){int result=accountBrowseRooms();if(result<=0||result==3)return result;}
   else{int result=accountHostPublicMenu();if(result<=0||result==3)return result;}
   menuBegin(&pad);
  }
  accountFrame("PUBLIC ROOMS",publicRoom.mode==TA_PUB_HOST?"CREATING ROOM":"BROWSE OR HOST A MATCH");
  for(unsigned i=0;i<3;i++)menuRow(172+i*62,items[i],i==choice);
  accountFooter("D-PAD MOVE  A SELECT  B BACK",NULL);
 }
}
static int accountBrowser(uint32_t*localIp,uint16_t previous,const uint8_t*rom,unsigned bytes){
 (void)previous;accountPartyDirectoryBegin();accountFrame("ONLINE","CONNECTING TO ONLINE SERVICE");accountFooter("B BACK",NULL);
 unsigned choice=0;bool triedResume=false;uint32_t refreshed=0;
 TaMenuPad pad;menuBegin(&pad);
 const char*items[]={"PUBLIC ROOMS","PROFILE","WORLD CHAT","LEADERBOARDS","BACK"};
 taSocialReset();
 for(;;){
  unsigned pressed=menuRead(&pad);
  if(pressed&0x100){taSocialCancel();leavePublic();ta_vs_udp_close(&transport);return 1;}
  XNADDR address;memset(&address,0,sizeof address);DWORD status=XNetGetTitleXnAddr(&address);
  *localIp=address.ina.s_addr;
  if(status&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_AUTO|XNET_GET_XNADDR_PPPOE))publicRoom.localAddress=*localIp;
  int error=pump(false);if(error){taSocialCancel();return error;}
  if(publicRoom.haveRoom){taSocialCancel();return 0;}
  if(publicRoom.haveCookie&&!triedResume){triedResume=true;if(!accountAuthenticated&&profileCount&&accountLink.ready)accountAuthenticate(3,savedProfiles[profileActive].name,savedProfiles[profileActive].token);}
  if(accountAuthenticated)taSocialTick(GetTickCount(),true);
  choice=menuMove(choice,5,pressed);
  if(pressed&1){
   taSocialCancel();
   if(choice==4){leavePublic();ta_vs_udp_close(&transport);return 1;}
   if(choice==0){int result=accountPublicRoomsMenu();if(result==0)return 0;if(result<0)return result;
    if(result==3){publicPartyBrowsing=false;return 3;}
    if(result==2){leavePublic();ta_vs_udp_close(&transport);int reconnect=beginLobby(rom,bytes,1,true);if(reconnect)return reconnect;triedResume=false;taSocialReset();accountPartyDirectoryBegin();} }
   else if(choice==1)accountStatsMenu();
   else if(choice==2)taSocialChatMenu();
   else accountLadderMenu();
   menuBegin(&pad);
  }
  char sub[100];if(accountAuthenticated)snprintf(sub,sizeof sub,"SIGNED IN: %s",ownProfile.name);
  else if(publicAutoAttempted&&!publicClock.armed)strcpy(sub,"GETTING NETWORK ADDRESS");
  else strcpy(sub,accountNotice);
  accountFrame("ONLINE",sub);
  for(unsigned i=0;i<5;i++)menuRow(160+i*43,items[i],choice==i);

  accountFooter("D-PAD MOVE  A SELECT  B BACK",NULL);
 }
}
static bool accountCharacterMenu(unsigned*value){
 unsigned choice=*value;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false)||roundState.peerLeft||pressed&0x100)return false;choice=menuMove(choice,13,pressed);if(pressed&1){*value=choice;return true;}accountFrame("CHARACTER","HIGHLIGHT A CHARACTER THEN SELECT");unsigned first=choice>5?choice-5:0;for(unsigned row=0;row<6&&first+row<13;row++){unsigned i=first+row,y=160+row*35;menuTouchChoice(36,(int)y-6,324,29,row,i==choice);if(i==choice)accountHighlight(35,y-8,324,29);ta_menu_text(screen,46,y,ta_vs_character_name(i),i==choice?uiGold:uiText,2);}ta_vs_preview_draw(&previewAtlas,choice,screen,374,211);accountFooter("D-PAD MOVE  A CHOOSE  B CANCEL",NULL);}
}
static bool accountSpeedMenu(unsigned*value){
 unsigned choice=*value-1;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false)||roundState.peerLeft||pressed&0x100)return false;choice=menuMove(choice,10,pressed);if(pressed&1){*value=choice+1;return true;}accountFrame("MATCH SPEED","THE HOST SETS THE SAME SPEED FOR BOTH");unsigned first=choice>5?choice-5:0;for(unsigned row=0;row<6&&first+row<10;row++){unsigned i=first+row;char text[24];snprintf(text,sizeof text,"SPEED %u",i+1);unsigned y=160+row*36;menuTouchChoice(36,(int)y-6,440,32,row,i==choice);if(i==choice)accountSelected(y-6,26);ta_menu_text(screen,58,y,text,i==choice?uiGold:uiText,2);}accountFooter("D-PAD MOVE  A CHOOSE  B CANCEL",NULL);}
}
static void accountFetchPeer(void){memset(&otherProfile,0,sizeof otherProfile);if(publicMode&&accountAuthenticated&&accountCall(9,NULL,0,"GETTING OPPONENT PROFILE")&&accountLink.replyBytes==46)ta_account_profile(&otherProfile,accountLink.reply+2,44);}
static void accountArm(void){
 reportDone=reportOutcome=0;reportEpoch=roundState.epoch;reportAt=0;if(!publicMode||!accountAuthenticated)return;
 uint8_t epoch[4]={(uint8_t)(reportEpoch>>24),(uint8_t)(reportEpoch>>16),(uint8_t)(reportEpoch>>8),(uint8_t)reportEpoch};
 if(!accountCall(6,epoch,4,"PREPARING RANKED MATCH")){reportDone=1;TA_Log("[TA N22 ACCOUNT] rank_arm_failed status=%u\n",accountLink.replyBytes>=2?accountLink.reply[1]:255);}
}
static void accountReport(bool lost){
 if(!publicMode||!accountAuthenticated||reportDone)return;
 if(!reportOutcome)reportOutcome=lost?2:1;
 if(accountLink.pending)return;
 if(accountLink.replyBytes&&accountLink.reply[0]==7){unsigned status=accountLink.reply[1];if(accountLink.replyBytes==46)ta_account_profile(&ownProfile,accountLink.reply+2,44);if(status!=7){reportDone=1;strcpy(accountNotice,status?accountStatus(status):"MATCH RECORDED - PROFILE UPDATED");TA_Log("[TA N22 ACCOUNT] result_epoch=%lu status=%u rating=%lu wins=%lu losses=%lu\n",(unsigned long)reportEpoch,status,(unsigned long)ownProfile.rating,(unsigned long)ownProfile.wins,(unsigned long)ownProfile.losses);return;}accountLink.replyBytes=0;}
 if(reportAt&&(uint32_t)(GetTickCount()-reportAt)<700)return;
 uint8_t data[13];uint64_t nonce=publicRoom.roomNonce;for(unsigned i=0;i<8;i++)data[7-i]=(uint8_t)(nonce>>(8*i));data[8]=(uint8_t)(reportEpoch>>24);data[9]=(uint8_t)(reportEpoch>>16);data[10]=(uint8_t)(reportEpoch>>8);data[11]=(uint8_t)reportEpoch;data[12]=(uint8_t)reportOutcome;
 if(!ta_account_request(&accountLink,&publicRoom,7,data,sizeof data))reportAt=GetTickCount();
}
static void accountDrainReport(void){if(!publicMode||!reportOutcome||reportDone)return;uint32_t start=GetTickCount();while(!reportDone&&(uint32_t)(GetTickCount()-start)<1500){accountReport(reportOutcome!=1);if(pump(false))break;accountFrame("SAVING MATCH RESULT","YOUR ROOM STAYS CONNECTED");accountFooter("UPDATING PROFILE",NULL);}accountLink.pending=0;}

static void accountRenderOptions(bool ready){
 accountFrame(publicMode?"ONLINE VERSUS":"LAN VERSUS","CHOOSE CHARACTER SPEED OR READY");char text[32];
 menuTouchChoice(36,154,440,88,0,optionChoice==0);menuTouchChoice(36,249,440,43,1,optionChoice==1);menuTouchRowIndex=2;
 ta_n17_panel(36,145,440,99);if(optionChoice==0)accountSelected(154,88);uint32_t c=optionChoice==0?uiGold:uiText;
 ta_menu_text(screen,58,156,publicMode&&accountAuthenticated?ownProfile.name:"YOUR CHARACTER",c,2);ta_menu_text(screen,58,181,ta_vs_character_name(ownCharacter),uiText,2);ta_menu_text(screen,58,216,"CHOOSE CHARACTER",uiMuted,2);ta_vs_preview_draw_size(&previewAtlas,ownCharacter,screen,390,158,80);
 ta_n17_panel(36,249,440,43);if(optionChoice==1)accountSelected(249,43);ta_menu_text(screen,58,253,"MATCH SPEED",optionChoice==1?uiGold:uiText,2);snprintf(text,sizeof text,"%u",sharedSpeed);ta_menu_text(screen,360,253,text,uiGold,5);ta_menu_text(screen,58,275,lobby.role?"HOST CHOOSES MATCH SPEED":"CHOOSE MATCH SPEED",uiMuted,2);
 ta_n17_panel(36,298,440,97);ta_menu_text(screen,46,307,publicMode&&otherProfile.name[0]?otherProfile.name:"OPPONENT",0x00b5a1ff,2);ta_menu_text(screen,46,330,peerSettings?ta_vs_character_name(peerCharacter):"CHOOSING CHARACTER",uiText,2);ta_menu_text(screen,46,367,peerReady?"READY":"CHOOSING",peerReady?0x0096e3b2:uiMuted,2);if(peerSettings)ta_vs_preview_draw_size(&previewAtlas,peerCharacter,screen,390,302,80);
 const char*items[]={ready?"READY - WAITING":"READY","BACK"};menuActions(items,2,optionChoice>=2?optionChoice-2:~0u,false,publicMode?"A SELECT":"D-PAD CHOOSE  A SELECT  B BACK");if(publicMode)accountChatHint(TA_MENU_FOOTER_SECONDARY);
}

static const char*controlPhysical(unsigned mask){
 static char text[48];text[0]=0;static const unsigned bits[]={1,0x100,2,0x200,0x400,0x800,8,4,0x10,0x20,0x40,0x80,0x1000,0x2000};
 static const char*names[]={"A","B","X","Y","L","R","START","BACK","UP","DOWN","LEFT","RIGHT",
#if defined(TA_ANDROID)
 "R1/TARGET","AUX"};
#else
 "WHITE","BLACK"};
#endif
 for(unsigned i=0;i<14;i++)if(mask&bits[i]){if(text[0])strcat(text,"/");if(strlen(text)+strlen(names[i])<sizeof text-1)strcat(text,names[i]);}return text;
}
static void accountControlsMenu(void){
 static const char*actions[]={"UP","DOWN","LEFT","RIGHT","SWAP","RAISE","PAUSE","CYCLE JUNK"};
 unsigned player=0,choice=0;int binding=-1;bool released=false,saved=true;TaMenuPad pad;menuBegin(&pad);
 for(;;){uint16_t raw[2];TA_Input_Poll(raw);unsigned pressed=menuUpdate(&pad,raw[0]);
  if(binding>=0){unsigned value=raw[player];if(value&0x4000u)value=(value&~0x4000u)|0x1000u;if(!value)released=true;
   if((raw[0]&4u)||(released&&(value&4u))||(menuTouchPressed&&(pressed&0x100u))){binding=-1;menuBegin(&pad);}
   else if(released&&value){unsigned bit=value&(~value+1u);TA_Controls_Bind(player,(unsigned)binding,bit);saved=TA_Controls_Save();binding=-1;menuBegin(&pad);}
  }else{
   if(pressed&0x100)return;choice=menuMove(choice,11,pressed);
   if(pressed&1){if(choice==0)player^=1;else if(choice<=8){binding=(int)choice-1;released=false;}else if(choice==9){TA_Controls_Reset(player);saved=TA_Controls_Save();}else return;}
  }
  char title[48];snprintf(title,sizeof title,"CONTROLS - PLAYER %u",player+1);accountFrame(title,"GAMEPLAY BINDS - MENU BUTTONS STAY A/B");
  unsigned first=choice>5?choice-5:0;for(unsigned row=0;row<6&&first+row<11;row++){unsigned i=first+row;char label[80];
   if(i==0)snprintf(label,sizeof label,"EDIT PLAYER %u",player+1);else if(i<=8)snprintf(label,sizeof label,"%-10s %s",actions[i-1],controlPhysical(TA_Controls_Get(player,i-1)));else strcpy(label,i==9?"RESET TO DEFAULTS":"BACK");menuRow(160+row*36,label,i==choice,2);
  }
  accountFooter(binding>=0?"B CANCEL BINDING":"A REBIND  B BACK",binding>=0?"BACK BUTTON CANCELS BINDING":saved?"SAVED ON THIS DEVICE":"SAVE FAILED - ACTIVE THIS SESSION");
 }
}
#include "ta_n22_presence.h"
