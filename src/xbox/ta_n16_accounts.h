/* N16 menus only. Account requests never run emulation or wait on live input. */
#ifndef TA_N12_CACHE_DIR
#define TA_N12_CACHE_DIR "E:\\TA_NATIVE_VS"
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
static void accountProfiles(bool write){
 char path[160];accountPath(path,sizeof path,"n14-profiles.dat");
 if(write){
  uint8_t head[8]={'T','A','P','1','4',1,(uint8_t)profileCount,(uint8_t)profileActive};char tmp[168];snprintf(tmp,sizeof tmp,"%s.tmp",path);
  FILE*f=fopen(tmp,"wb");bool ok=false;if(f){ok=fwrite(head,1,8,f)==8&&fwrite(savedProfiles,1,sizeof savedProfiles,f)==sizeof savedProfiles;if(fflush(f))ok=false;if(fclose(f))ok=false;}
  bool renamed=ok&&MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
  /* Some Xbox filesystem wrappers cannot replace an existing file. Preserve
     in-memory sign-in and fall back to the same stdio path used by ROM caches. */
  if(!renamed){f=fopen(path,"wb");ok=false;if(f){ok=fwrite(head,1,8,f)==8&&fwrite(savedProfiles,1,sizeof savedProfiles,f)==sizeof savedProfiles;if(fflush(f))ok=false;if(fclose(f))ok=false;}}
  else ok=true;
  if(ok){uint8_t check[8+sizeof savedProfiles];f=fopen(path,"rb");ok=f&&fread(check,1,sizeof check,f)==sizeof check&&!memcmp(check,head,8)&&!memcmp(check+8,savedProfiles,sizeof savedProfiles);if(f)fclose(f);}
  accountSavedLocally=ok;TA_Log("[TA N16 ACCOUNT] saved_local=%u replace_fallback=%u profiles=%u\n",(unsigned)ok,(unsigned)!renamed,profileCount);
  if(!ok)strcpy(accountNotice,"SIGNED IN - LOCAL SAVE UNAVAILABLE");return;
 }

 if(profilesRead)return;profilesRead=true;FILE*f=fopen(path,"rb");if(!f)return;uint8_t head[8];bool ok=fread(head,1,8,f)==8&&fread(savedProfiles,1,sizeof savedProfiles,f)==sizeof savedProfiles&&fgetc(f)==EOF;fclose(f);
 if(!ok||memcmp(head,"TAP14",5)||head[5]!=1||head[6]>8||(head[6]&&head[7]>=head[6])){profileCount=profileActive=0;memset(savedProfiles,0,sizeof savedProfiles);return;}
 profileCount=head[6];profileActive=profileCount?head[7]:0;for(unsigned i=0;i<profileCount;i++){bool valid=false;for(unsigned k=0;k<16;k++){char c=savedProfiles[i].name[k];if(!c){valid=k>=3;break;}if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))break;}if(!valid){profileCount=profileActive=0;memset(savedProfiles,0,sizeof savedProfiles);break;}}
}
static void accountRect(int x,int y,int w,int h,uint32_t color){for(int j=y;j<y+h&&j<480;j++)for(int i=x;i<x+w&&i<512;i++)if(i>=0&&j>=0)screen[j*512+i]=color;}
static const uint32_t uiText=0x00f1f3f8,uiMuted=0x00bac5d3,uiGold=0x00f1cb72,uiInk=0x00101928;
static void accountSelected(int y,int h){accountRect(35,y,442,h,uiGold);accountRect(35,y,4,h,uiText);for(int k=0;k<5;k++)accountRect(42+k,y+h/2-4+k,1,9-k*2,uiInk);}
static void accountButtonLine(int x,int y,const char*line){
 if(!line)return;for(unsigned i=0;line[i];i++,x+=8){char text[2]={line[i],0};bool button=(!i||line[i-1]==' ')&&line[i+1]==' '&&(line[i]=='A'||line[i]=='B'||line[i]=='X'||line[i]=='Y');
  if(button){uint32_t c=line[i]=='A'?0x005cdb8b:line[i]=='B'?0x00f18791:line[i]=='X'?0x007caeff:0x00f1cb72;accountRect(x-2,y-3,11,16,c);ta_vs_lan_text(screen,x,y,text,uiInk,2);}
  else ta_vs_lan_text(screen,x,y,text,uiText,2);
 }
}
static void accountFrame(const char*title,const char*sub){
 for(unsigned y=0;y<480;y++)for(unsigned x=0;x<512;x++)screen[y*512+x]=((9+y/120)<<16)|((15+y/90)<<8)|(26+y/55);
 accountRect(26,23,460,2,0x00415570);ta_vs_lan_text(screen,30,35,"TETRIS ATTACK",uiText,3);ta_vs_lan_text(screen,446,37,"N16",uiGold,2);ta_vs_lan_text(screen,30,59,title,uiText,4);
 if(sub)ta_vs_lan_text(screen,30,87,sub,uiMuted,2);
 accountRect(26,118,460,2,0x00415570);
 accountRect(26,143,460,247,0x00172335);accountRect(26,400,460,2,0x00415570);
}
static void accountFooter(const char*primaryLine,const char*secondaryLine){accountButtonLine(30,416,primaryLine);if(secondaryLine)accountButtonLine(30,444,secondaryLine);TA_Video_Submit((const uint8_t*)screen);}
static bool accountOpen(void){
 memset(&accountLink,0,sizeof accountLink);memset(&ownProfile,0,sizeof ownProfile);memset(&otherProfile,0,sizeof otherProfile);namedCount=0;reportDone=reportOutcome=reportEpoch=0;accountProfiles(false);
 char text[80]={0};uint8_t key[32],secret[32];FILE*f=fopen("D:\\ta-account-key.txt","rb");if(!f){strcpy(accountNotice,"UPDATE SERVER AND COPY ACCOUNT KEY");return false;}bool ok=fgets(text,sizeof text,f)!=NULL;fclose(f);if(!ok)return false;
 for(unsigned i=0;i<32;i++){unsigned a,b;char c=text[i*2],d=text[i*2+1];a=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:255;b=d>='0'&&d<='9'?d-'0':d>='a'&&d<='f'?d-'a'+10:d>='A'&&d<='F'?d-'A'+10:255;if(a>15||b>15){strcpy(accountNotice,"INVALID SERVER ACCOUNT KEY");return false;}key[i]=(uint8_t)(a*16+b);}
 if(XNetRandom(secret,sizeof secret)){strcpy(accountNotice,"ACCOUNT NETWORK SETUP FAILED");return false;}int result=ta_account_init(&accountLink,secret,key);memset(secret,0,sizeof secret);return result==0;
}
static const char*accountStatus(unsigned s){return s==2?"USERNAME ALREADY TAKEN":s==3?"SIGN IN FAILED - CHECK PASSWORD":s==4?"PLEASE WAIT THEN TRY AGAIN":s==5?"PLEASE SIGN IN FIRST":s==6?"MATCH NOT ELIGIBLE FOR RANKING":s==7?"WAITING FOR BOTH MATCH RESULTS":s==8?"RESULTS DISAGREE - STATS UNCHANGED":"CHECK NAME AND PASSWORD";}
static bool accountCall(unsigned command,const void*data,unsigned bytes,const char*label,bool quiet=false){
 accountLastStatus=-1;
 if(!accountLink.ready){strcpy(accountNotice,"UPDATE SERVER AND COPY ACCOUNT KEY");return false;}
 if(ta_account_request(&accountLink,&publicRoom,command,data,bytes)){strcpy(accountNotice,"CONNECTING - PLEASE TRY AGAIN");return false;}
 uint32_t start=GetTickCount();uint16_t pads[2];TA_Input_Poll(pads);uint16_t previous=pads[0];
 while(accountLink.pending&&(uint32_t)(GetTickCount()-start)<6000){
  if(pump(false)){accountLink.pending=0;strcpy(accountNotice,"ROOM SERVER UNAVAILABLE");return false;}
  if(!accountLink.pending)break;
  TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];if(pressed&0x100){accountLink.pending=0;accountLastStatus=-2;return false;}
  if(quiet)TA_Video_Submit((const uint8_t*)screen);else{accountFrame(label,"SECURE CONNECTION TO TETRIS ATTACK");ta_vs_lan_text(screen,44,230,"PLEASE WAIT",uiText,3);accountFooter("B CANCEL",NULL);}
 }
 if(accountLink.pending){accountLink.pending=0;strcpy(accountNotice,"SERVER TIMEOUT - PLEASE RETRY");return false;}
 if(accountLink.replyBytes<2||accountLink.reply[0]!=command){strcpy(accountNotice,"INVALID SERVER RESPONSE");return false;}
 accountLastStatus=accountLink.reply[1];if(accountLastStatus){strcpy(accountNotice,accountStatus((unsigned)accountLastStatus));if(accountLastStatus==5)accountAuthenticated=false;return false;}return true;
}
/* Uniform menu input: A selects, B returns, held D-pad repeats without
   repeating A/B or leaking the opening A press into the next menu. */
typedef struct{uint16_t previous,held;uint32_t repeated;bool repeating;}TaMenuPad;
static void menuBegin(TaMenuPad*p){uint16_t pads[2];TA_Input_Poll(pads);p->previous=pads[0];p->held=pads[0]&0xf0;p->repeated=GetTickCount();p->repeating=false;}
static unsigned menuUpdate(TaMenuPad*p,uint16_t buttons){unsigned pressed=buttons&~p->previous;p->previous=buttons;uint16_t held=buttons&0xf0;uint32_t now=GetTickCount();if(held!=p->held){p->held=held;p->repeated=now;p->repeating=false;}else if(held&&(uint32_t)(now-p->repeated)>=(p->repeating?100u:320u)){pressed|=held;p->repeated=now;p->repeating=true;}return pressed;}
static unsigned menuRead(TaMenuPad*p){uint16_t pads[2];TA_Input_Poll(pads);return menuUpdate(p,pads[0]);}
static unsigned menuMove(unsigned selected,unsigned count,unsigned pressed){if(!count)return 0;if(pressed&0x10)selected=selected?selected-1:count-1;if(pressed&0x20)selected=(selected+1)%count;return selected;}
static void menuRow(unsigned y,const char*label,bool selected,unsigned scale=3){if(selected)accountSelected((int)y-8,31);ta_vs_lan_text(screen,58,(int)y,label,selected?uiInk:uiText,(int)scale);}
static void menuActions(const char*const*labels,unsigned count,unsigned selected,bool submit=true){
 unsigned width=460/count;for(unsigned i=0;i<count;i++){unsigned x=26+i*width;accountRect((int)x,407,(int)width-4,25,i==selected?uiGold:0x00233248);ta_vs_lan_text(screen,(int)x+8,415,labels[i],i==selected?uiInk:uiText,2);}accountButtonLine(30,446,"D-PAD CHOOSE   A SELECT   B BACK");if(submit)TA_Video_Submit((const uint8_t*)screen);
}
static bool accountKeyboard(const char*title,const char*label,char*out,unsigned maximum,bool password,unsigned minimum=4){
 static const char keys[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";unsigned key=0,length=0;char value[33]={0};TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false))return false;
  if(pressed&0x100){memset(value,0,sizeof value);return false;}if(pressed&0x40)key=(key+39)%40;if(pressed&0x80)key=(key+1)%40;if(pressed&0x10)key=(key+32)%40;if(pressed&0x20)key=(key+8)%40;
  if((pressed&2)||((pressed&1)&&key==38)){if(length)value[--length]=0;}
  else if((pressed&1)&&key<38&&length<maximum){value[length++]=keys[key];value[length]=0;}
  if(((pressed&8)||((pressed&1)&&key==39))&&length>=minimum){memcpy(out,value,length+1);memset(value,0,sizeof value);return true;}
  char hint[80];snprintf(hint,sizeof hint,"%u TO %u CHARACTERS%s",minimum,maximum,password?" - PASSWORD HIDDEN":" - LETTERS NUMBERS DASH UNDERSCORE");accountFrame(title,hint);char shown[40]={0};if(password)memset(shown,'X',length);else memcpy(shown,value,length);ta_vs_lan_text(screen,42,156,label,uiMuted,2);ta_vs_lan_text(screen,42,179,shown,uiText,3);
  for(unsigned k=0;k<40;k++){unsigned x=48+(k%8)*54,y=224+(k/8)*30;if(k==key){accountRect(x-9,y-7,44,26,uiGold);accountRect(x-9,y+17,44,2,uiText);}char ch[2]={k<38?keys[k]:' ',0};ta_vs_lan_text(screen,x,y,k==38?"DEL":k==39?"DONE":ch,k==key?uiInk:uiText,k<38?3:2);}
  accountFooter("D-PAD HIGHLIGHT   A SELECT   B CANCEL","SELECT DEL TO DELETE OR DONE TO CONTINUE");
 }
}
static bool accountAuthenticate(unsigned command,const char*name,const void*credential){
 uint8_t data[48]={0};memcpy(data,name,strlen(name));memcpy(data+16,credential,32);
 bool ok=accountCall(command,data,sizeof data,command==1?"CREATING PROFILE":"SIGNING IN",command==3);memset(data,0,sizeof data);if(!ok)return false;
 TaAccountProfile authenticatedProfile;if(accountLink.replyBytes!=78||ta_account_profile(&authenticatedProfile,accountLink.reply+34,44)){strcpy(accountNotice,"INVALID PROFILE RESPONSE");return false;}
 ownProfile=authenticatedProfile;unsigned index=profileCount;for(unsigned i=0;i<profileCount;i++)if(!strcmp(savedProfiles[i].name,ownProfile.name))index=i;
 if(index==profileCount){if(profileCount==8){strcpy(accountNotice,"REMOVE A LOCAL PROFILE FIRST");accountAuthenticated=false;return false;}profileCount++;}
 memset(&savedProfiles[index],0,sizeof savedProfiles[index]);strcpy(savedProfiles[index].name,ownProfile.name);memcpy(savedProfiles[index].token,accountLink.reply+2,32);profileActive=index;accountAuthenticated=true;TA_Log("[TA N16 ACCOUNT] signed_in=1 profile=%s saved_profiles=%u\n",ownProfile.name,profileCount);strcpy(accountNotice,"SIGNED IN - READY TO PLAY");accountProfiles(true);return true;
}
static void accountRemoveMenu(void){
 unsigned selected=profileActive;TaMenuPad pad;menuBegin(&pad);for(;;){unsigned pressed=menuRead(&pad);if(pump(false)||pressed&0x100)return;selected=menuMove(selected,profileCount+1,pressed);
  if(pressed&1){if(selected==profileCount)return;if(accountAuthenticated&&!strcmp(ownProfile.name,savedProfiles[selected].name)){accountAuthenticated=false;memset(&ownProfile,0,sizeof ownProfile);}for(unsigned i=selected;i+1<profileCount;i++)savedProfiles[i]=savedProfiles[i+1];memset(&savedProfiles[--profileCount],0,sizeof savedProfiles[0]);if(profileActive==selected)profileActive=0;else if(profileActive>selected)profileActive--;if(profileActive>=profileCount)profileActive=0;accountProfiles(true);strcpy(accountNotice,"REMOVED FROM THIS XBOX ONLY");return;}
  accountFrame("REMOVE LOCAL PROFILE","ONLINE ACCOUNT AND STATS WILL BE KEPT");unsigned first=selected>5?selected-5:0;for(unsigned row=0;row<6&&first+row<profileCount+1;row++){unsigned i=first+row;menuRow(156+row*37,i<profileCount?savedProfiles[i].name:"BACK",i==selected);}accountFooter("D-PAD CHOOSE   A REMOVE   B BACK",NULL);
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
  for(unsigned row=0;row<6&&first+row<profileCount+4;row++){unsigned i=first+row;char text[64];if(i<profileCount)snprintf(text,sizeof text,"%s%s",savedProfiles[i].name,accountAuthenticated&&!strcmp(ownProfile.name,savedProfiles[i].name)?" - ACTIVE":"");else strcpy(text,i==profileCount?"CREATE PROFILE":i==profileCount+1?"SIGN IN / RECOVER":i==profileCount+2?"REMOVE LOCAL PROFILE":"BACK");menuRow(156+row*37,text,i==selected);}
  accountFooter("D-PAD HIGHLIGHT   A SELECT   B BACK","SAVED PROFILES SIGN IN WITHOUT A PASSWORD");
 }
}
static bool accountEnsure(void){
 if(accountAuthenticated)return true;
 if(!publicRoom.haveCookie){strcpy(accountNotice,"CONNECTING - PLEASE WAIT");return false;}
 if(profileCount){if(accountAuthenticate(3,savedProfiles[profileActive].name,savedProfiles[profileActive].token))return true;if(accountLastStatus!=3)return false;}
 accountProfileMenu();return accountAuthenticated;
}
static void accountRefreshProfile(void){if(accountAuthenticated&&accountCall(4,NULL,0,"REFRESHING PROFILE",true)&&accountLink.replyBytes==46)ta_account_profile(&ownProfile,accountLink.reply+2,44);}
static void accountDirectory(void){
 if(!accountAuthenticated)return;if(!accountCall(8,NULL,0,"REFRESHING ROOMS",true))return;const uint8_t*b=accountLink.reply+2;unsigned bytes=accountLink.replyBytes-2;if(!bytes||b[0]>8||bytes!=1+b[0]*25)return;namedCount=b[0];
 for(unsigned i=0;i<namedCount;i++){const uint8_t*q=b+1+i*25;namedRooms[i].id=((uint32_t)q[0]<<24)|((uint32_t)q[1]<<16)|((uint32_t)q[2]<<8)|q[3];namedRooms[i].speed=q[4];memcpy(namedRooms[i].name,q+5,16);namedRooms[i].name[16]=0;namedRooms[i].rating=((uint32_t)q[21]<<24)|((uint32_t)q[22]<<16)|((uint32_t)q[23]<<8)|q[24];}
}
static void accountLadder(void){
 uint8_t data[2]={(uint8_t)(ladderOffset>>8),(uint8_t)ladderOffset};if(!accountCall(5,data,2,"LOADING LEADERBOARD"))return;const uint8_t*b=accountLink.reply+2;unsigned n=accountLink.replyBytes-2;if(n<5||b[4]>8||n!=5+b[4]*44)return;ladderOffset=((unsigned)b[0]<<8)|b[1];ladderTotal=((unsigned)b[2]<<8)|b[3];ladderCount=b[4];for(unsigned i=0;i<ladderCount;i++)if(ta_account_profile(&ladder[i],b+5+i*44,44)){ladderCount=0;break;}
}
static void accountStats(void){
 if(!accountAuthenticated){ta_vs_lan_text(screen,44,182,"YOUR VERSUS JOURNEY STARTS HERE",uiText,3);ta_vs_lan_text(screen,44,228,accountNotice,uiMuted,2);ta_vs_lan_text(screen,44,302,"A CREATE OR SIGN IN",uiGold,3);return;}
 char text[80];snprintf(text,sizeof text,"%s",ownProfile.name);ta_vs_lan_text(screen,44,159,text,uiText,4);
 snprintf(text,sizeof text,"%s   %lu RATING",ta_account_tier(ownProfile.rating),(unsigned long)ownProfile.rating);ta_vs_lan_text(screen,44,204,text,uiGold,3);
 if(ownProfile.rank)snprintf(text,sizeof text,"GLOBAL RANK %lu",(unsigned long)ownProfile.rank);else strcpy(text,"GLOBAL RANK - PLAY YOUR FIRST MATCH");ta_vs_lan_text(screen,44,245,text,uiMuted,2);
 snprintf(text,sizeof text,"WINS %lu     LOSSES %lu",(unsigned long)ownProfile.wins,(unsigned long)ownProfile.losses);ta_vs_lan_text(screen,44,280,text,uiText,3);
 snprintf(text,sizeof text,"DRAWS %lu   BEST WIN STREAK %lu",(unsigned long)ownProfile.draws,(unsigned long)ownProfile.best);ta_vs_lan_text(screen,44,319,text,uiMuted,2);
 snprintf(text,sizeof text,"STREAK %ld   MATCHES %lu",(long)ownProfile.streak,(unsigned long)(ownProfile.wins+ownProfile.losses+ownProfile.draws));ta_vs_lan_text(screen,44,350,text,uiMuted,2);
}
static uint32_t leaderboardColor(uint32_t place){static const uint32_t colors[5]={0x00ffd166,0x00cad5e8,0x00e8a174,0x0065d9d1,0x00b5a1ff};return place>=1&&place<=5?colors[place-1]:uiText;}
static void accountDrawLadder(void){
 ta_vs_lan_text(screen,42,153,"POS",uiMuted,2);ta_vs_lan_text(screen,80,153,"PLAYER",uiMuted,2);ta_vs_lan_text(screen,230,153,"TIER",uiMuted,2);ta_vs_lan_text(screen,382,153,"RATING",uiMuted,2);
 if(!ladderCount)ta_vs_lan_text(screen,44,222,"FINISH A PUBLIC MATCH TO RANK",uiText,3);
 for(unsigned i=0;i<ladderCount;i++){const TaAccountProfile*p=&ladder[i];unsigned y=180+i*25;uint32_t color=leaderboardColor(p->rank);char text[24];if(!strcmp(p->name,ownProfile.name))accountRect(35,y-5,442,21,0x00273852);snprintf(text,sizeof text,"%lu",(unsigned long)p->rank);ta_vs_lan_text(screen,42,y,text,color,2);ta_vs_lan_text(screen,80,y,p->name,color,2);ta_vs_lan_text(screen,230,y,ta_account_tier(p->rating),color,2);snprintf(text,sizeof text,"%lu",(unsigned long)p->rating);ta_vs_lan_text(screen,382,y,text,color,2);}
}
static void accountLadderMenu(void){
 if(!accountEnsure())return;ladderOffset=0;accountLadder();unsigned choice=0;TaMenuPad pad;menuBegin(&pad);const char*items[]={"PREVIOUS","NEXT","REFRESH","BACK"};
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false)||pressed&0x100)return;unsigned moved=pressed;if(pressed&0x40)moved|=0x10;if(pressed&0x80)moved|=0x20;choice=menuMove(choice,4,moved);
  if(pressed&1){if(choice==3)return;if(choice==0&&ladderOffset>=8){ladderOffset-=8;accountLadder();}else if(choice==1&&ladderOffset+8<ladderTotal){ladderOffset+=8;accountLadder();}else if(choice==2)accountLadder();menuBegin(&pad);}
  accountFrame("LEADERBOARD","PUBLIC MATCHES - TIER AND RATING");accountDrawLadder();menuActions(items,4,choice);
 }
}
static void accountStatsMenu(void){
 if(!accountEnsure())return;accountRefreshProfile();unsigned choice=0;TaMenuPad pad;menuBegin(&pad);const char*items[]={"PROFILES","REFRESH","BACK"};
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false)||pressed&0x100)return;unsigned moved=pressed;if(pressed&0x40)moved|=0x10;if(pressed&0x80)moved|=0x20;choice=menuMove(choice,3,moved);
  if(pressed&1){if(choice==2)return;if(choice==0)accountProfileMenu();else accountRefreshProfile();menuBegin(&pad);}
  accountFrame("MY PROFILE",accountAuthenticated?ownProfile.name:accountNotice);accountStats();menuActions(items,3,choice);
 }
}
static int accountBrowseRooms(void){
 unsigned choice=0;uint32_t selectedRoom=0;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false))return -1;if(pressed&0x100)return 1;if(publicRoom.haveRoom)return 0;unsigned count=publicRoom.roomCount;if(choice>=count+2)choice=count+1;choice=menuMove(choice,count+2,pressed);
  if(pressed&1){if(choice==count+1)return 1;if(choice==count){publicRoom.requested=0;if(accountAuthenticated)accountDirectory();}else if(selectedRoom&&accountEnsure()){bool exists=false;for(unsigned i=0;i<count;i++)if(publicRoom.rooms[i].id==selectedRoom)exists=true;if(exists)ta_vs_public_join(&publicRoom,selectedRoom);else strcpy(accountNotice,"ROOM CLOSED - CHOOSE ANOTHER ROOM");}menuBegin(&pad);}
  if(choice>=count+2)choice=count+1;accountFrame("BROWSE ROOMS","CHOOSE AN OPEN ROOM TO JOIN");unsigned first=choice>5?choice-5:0;
  for(unsigned row=0;row<6&&first+row<count+2;row++){unsigned i=first+row;char text[80];if(i<count){const char*name=NULL;for(unsigned n=0;n<namedCount;n++)if(namedRooms[n].id==publicRoom.rooms[i].id)name=namedRooms[n].name;if(name)snprintf(text,sizeof text,"%-15s SPEED %u",name,publicRoom.rooms[i].speed);else snprintf(text,sizeof text,"ROOM %06lu   SPEED %u",(unsigned long)publicRoom.rooms[i].id,publicRoom.rooms[i].speed);}else strcpy(text,i==count?"REFRESH ROOMS":"BACK");menuRow(156+row*37,text,i==choice,2);}
  selectedRoom=choice<count?publicRoom.rooms[choice].id:0;accountFooter("D-PAD HIGHLIGHT   A SELECT   B BACK",publicRoom.mode==TA_PUB_JOINING?"JOINING ROOM":"SELECT A ROOM OR REFRESH THE LIST");
 }
}
static int accountBrowser(uint32_t*localIp,uint16_t previous){
 (void)previous;accountFrame("PUBLIC ROOMS","CONNECTING TO YOUR SAVED PROFILE");accountFooter("B BACK",NULL);unsigned choice=0;bool triedResume=false;uint32_t refreshed=0;TaMenuPad pad;menuBegin(&pad);const char*items[]={"BROWSE ROOMS","HOST PUBLIC ROOM","MY PROFILE","LEADERBOARD","PROFILES","BACK"};
 for(;;){unsigned pressed=menuRead(&pad);if(pressed&0x100){leavePublic();ta_vs_udp_close(&transport);return 1;}
  XNADDR address;DWORD status=XNetGetTitleXnAddr(&address);if(status&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_AUTO|XNET_GET_XNADDR_PPPOE)){*localIp=address.ina.s_addr;publicRoom.localAddress=*localIp;int error=pump(false);if(error)return error;}
  if(publicRoom.haveRoom)return 0;if(publicRoom.haveCookie&&!triedResume){triedResume=true;if(!accountAuthenticated&&profileCount&&accountLink.ready)accountAuthenticate(3,savedProfiles[profileActive].name,savedProfiles[profileActive].token);}
  choice=menuMove(choice,6,pressed);if(pressed&1){if(choice==5){leavePublic();ta_vs_udp_close(&transport);return 1;}if(choice==0){int result=accountBrowseRooms();if(result==0)return 0;if(result<0)return -10060;}else if(choice==1){if(accountEnsure()){lobby.role=0;ta_vs_public_host(&publicRoom);}}else if(choice==2)accountStatsMenu();else if(choice==3)accountLadderMenu();else accountProfileMenu();menuBegin(&pad);}
  if(accountAuthenticated&&publicRoom.mode==TA_PUB_BROWSE&&(!refreshed||(uint32_t)(GetTickCount()-refreshed)>5000)){accountDirectory();refreshed=GetTickCount();}
  char sub[64];if(accountAuthenticated)snprintf(sub,sizeof sub,"%s   %s   %lu",ownProfile.name,ta_account_tier(ownProfile.rating),(unsigned long)ownProfile.rating);else strcpy(sub,accountNotice);accountFrame("PUBLIC ROOMS",sub);for(unsigned i=0;i<6;i++)menuRow(156+i*37,items[i],choice==i);accountFooter("D-PAD HIGHLIGHT   A SELECT   B BACK",publicRoom.mode==TA_PUB_HOST?"CREATING YOUR ROOM":"YOUR PROFILE STAYS SIGNED IN");
 }
}
static bool accountCharacterMenu(unsigned*value){
 unsigned choice=*value;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false)||roundState.peerLeft||pressed&0x100)return false;choice=menuMove(choice,13,pressed);if(pressed&1){*value=choice;return true;}accountFrame("CHARACTER","HIGHLIGHT A CHARACTER THEN SELECT");unsigned first=choice>5?choice-5:0;for(unsigned row=0;row<6&&first+row<13;row++){unsigned i=first+row,y=156+row*35;if(i==choice){accountRect(35,y-8,324,29,uiGold);accountRect(35,y-8,4,29,uiText);}ta_vs_lan_text(screen,46,y,ta_vs_character_name(i),i==choice?uiInk:uiText,2);}ta_vs_preview_draw(&previewAtlas,choice,screen,374,211);accountFooter("D-PAD HIGHLIGHT   A CHOOSE   B CANCEL",NULL);}
}
static bool accountSpeedMenu(unsigned*value){
 unsigned choice=*value-1;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false)||roundState.peerLeft||pressed&0x100)return false;choice=menuMove(choice,10,pressed);if(pressed&1){*value=choice+1;return true;}accountFrame("MATCH SPEED","THE HOST SETS THE SAME SPEED FOR BOTH");for(unsigned i=0;i<10;i++){char text[24];snprintf(text,sizeof text,"SPEED %u",i+1);unsigned y=158+i*22;if(i==choice)accountSelected(y-5,21);ta_vs_lan_text(screen,58,y,text,i==choice?uiInk:uiText,2);}accountFooter("D-PAD HIGHLIGHT   A CHOOSE   B CANCEL",NULL);}
}
static void accountFetchPeer(void){if(publicMode&&accountAuthenticated&&accountCall(9,NULL,0,"GETTING OPPONENT PROFILE")&&accountLink.replyBytes==46)ta_account_profile(&otherProfile,accountLink.reply+2,44);}
static void accountArm(void){
 reportDone=reportOutcome=0;reportEpoch=roundState.epoch;reportAt=0;if(!publicMode||!accountAuthenticated)return;
 uint8_t epoch[4]={(uint8_t)(reportEpoch>>24),(uint8_t)(reportEpoch>>16),(uint8_t)(reportEpoch>>8),(uint8_t)reportEpoch};
 if(!accountCall(6,epoch,4,"PREPARING RANKED MATCH")){reportDone=1;TA_Log("[TA N16 ACCOUNT] rank_arm_failed status=%u\n",accountLink.replyBytes>=2?accountLink.reply[1]:255);}
}
static void accountReport(bool lost){
 if(!publicMode||!accountAuthenticated||reportDone)return;
 if(!reportOutcome)reportOutcome=lost?(peerLost?3:2):1;
 if(accountLink.pending)return;
 if(accountLink.replyBytes&&accountLink.reply[0]==7){unsigned status=accountLink.reply[1];if(accountLink.replyBytes==46)ta_account_profile(&ownProfile,accountLink.reply+2,44);if(status!=7){reportDone=1;strcpy(accountNotice,status?accountStatus(status):"MATCH RECORDED - PROFILE UPDATED");TA_Log("[TA N16 ACCOUNT] result_epoch=%lu status=%u rating=%lu wins=%lu losses=%lu\n",(unsigned long)reportEpoch,status,(unsigned long)ownProfile.rating,(unsigned long)ownProfile.wins,(unsigned long)ownProfile.losses);return;}accountLink.replyBytes=0;}
 if(reportAt&&(uint32_t)(GetTickCount()-reportAt)<700)return;
 uint8_t data[13];uint64_t nonce=publicRoom.roomNonce;for(unsigned i=0;i<8;i++)data[7-i]=(uint8_t)(nonce>>(8*i));data[8]=(uint8_t)(reportEpoch>>24);data[9]=(uint8_t)(reportEpoch>>16);data[10]=(uint8_t)(reportEpoch>>8);data[11]=(uint8_t)reportEpoch;data[12]=(uint8_t)reportOutcome;
 if(!ta_account_request(&accountLink,&publicRoom,7,data,sizeof data))reportAt=GetTickCount();
}
static void accountDrainReport(void){if(!publicMode||!reportOutcome||reportDone)return;uint32_t start=GetTickCount();while(!reportDone&&(uint32_t)(GetTickCount()-start)<1500){accountReport(reportOutcome!=1);if(pump(false))break;accountFrame("SAVING MATCH RESULT","YOUR ROOM STAYS CONNECTED");accountFooter("UPDATING PROFILE",NULL);}accountLink.pending=0;}
