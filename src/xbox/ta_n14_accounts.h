/* N14 menus only. Account requests never run emulation or wait on live input. */
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
static unsigned reportOutcome,reportDone;static uint32_t reportEpoch,reportAt;
static void accountPath(char*out,unsigned size,const char*name){snprintf(out,size,"%s/%s",TA_N12_CACHE_DIR,name);}
static void accountProfiles(bool write){
 char path[160];accountPath(path,sizeof path,"n14-profiles.dat");
 if(write){char tmp[168];snprintf(tmp,sizeof tmp,"%s.tmp",path);FILE*f=fopen(tmp,"wb");if(!f){strcpy(accountNotice,"PROFILE NOT SAVED - CHECK HDD");return;}
  uint8_t head[8]={'T','A','P','1','4',1,(uint8_t)profileCount,(uint8_t)profileActive};bool ok=fwrite(head,1,8,f)==8&&fwrite(savedProfiles,1,sizeof savedProfiles,f)==sizeof savedProfiles;if(fclose(f))ok=false;
  if(ok){/* Replace only our small local token file. */if(!MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))strcpy(accountNotice,"PROFILE NOT SAVED - CHECK HDD");}return;
 }
 if(profilesRead)return;profilesRead=true;FILE*f=fopen(path,"rb");if(!f)return;uint8_t head[8];bool ok=fread(head,1,8,f)==8&&fread(savedProfiles,1,sizeof savedProfiles,f)==sizeof savedProfiles&&fgetc(f)==EOF;fclose(f);
 if(!ok||memcmp(head,"TAP14",5)||head[5]!=1||head[6]>8||(head[6]&&head[7]>=head[6])){memset(savedProfiles,0,sizeof savedProfiles);return;}
 profileCount=head[6];profileActive=profileCount?head[7]:0;for(unsigned i=0;i<profileCount;i++){bool valid=false;for(unsigned k=0;k<16;k++){char c=savedProfiles[i].name[k];if(!c){valid=k>=3;break;}if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))break;}if(!valid){profileCount=profileActive=0;memset(savedProfiles,0,sizeof savedProfiles);break;}}
}
static void accountRect(int x,int y,int w,int h,uint32_t color){for(int j=y;j<y+h&&j<480;j++)for(int i=x;i<x+w&&i<512;i++)if(i>=0&&j>=0)screen[j*512+i]=color;}
static void accountFrame(const char*title,const char*sub){
 for(unsigned y=0;y<480;y++)for(unsigned x=0;x<512;x++)screen[y*512+x]=((8+y/60)<<16)|((16+y/32)<<8)|(28+y/20);
 accountRect(26,28,460,4,0x006affa8);ta_vs_lan_text(screen,30,47,"TETRIS ATTACK",0x00ffffff,4);ta_vs_lan_text(screen,30,83,title,0x006fc9ff,3);
 if(sub)ta_vs_lan_text(screen,30,111,sub,0x009fb9ce,2);accountRect(26,137,460,253,0x00172639);accountRect(26,399,460,2,0x00294054);
}
static void accountFooter(const char*primaryLine,const char*secondaryLine){ta_vs_lan_text(screen,30,415,primaryLine,0x006affa8,2);if(secondaryLine)ta_vs_lan_text(screen,30,443,secondaryLine,0x009fb9ce,2);TA_Video_Submit((const uint8_t*)screen);}
static bool accountOpen(void){
 memset(&accountLink,0,sizeof accountLink);memset(&ownProfile,0,sizeof ownProfile);memset(&otherProfile,0,sizeof otherProfile);namedCount=0;reportDone=reportOutcome=reportEpoch=0;accountProfiles(false);
 char text[80]={0};uint8_t key[32],secret[32];FILE*f=fopen("D:\\ta-account-key.txt","rb");if(!f){strcpy(accountNotice,"UPDATE SERVER AND COPY ACCOUNT KEY");return false;}bool ok=fgets(text,sizeof text,f)!=NULL;fclose(f);if(!ok)return false;
 for(unsigned i=0;i<32;i++){unsigned a,b;char c=text[i*2],d=text[i*2+1];a=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:255;b=d>='0'&&d<='9'?d-'0':d>='a'&&d<='f'?d-'a'+10:d>='A'&&d<='F'?d-'A'+10:255;if(a>15||b>15){strcpy(accountNotice,"INVALID SERVER ACCOUNT KEY");return false;}key[i]=(uint8_t)(a*16+b);}
 if(XNetRandom(secret,sizeof secret)){strcpy(accountNotice,"ACCOUNT NETWORK SETUP FAILED");return false;}int result=ta_account_init(&accountLink,secret,key);memset(secret,0,sizeof secret);return result==0;
}
static const char*accountStatus(unsigned s){return s==2?"USERNAME ALREADY TAKEN":s==3?"SIGN IN FAILED - CHECK PASSWORD":s==4?"PLEASE WAIT THEN TRY AGAIN":s==5?"PLEASE SIGN IN FIRST":s==6?"MATCH NOT ELIGIBLE FOR RANKING":s==7?"WAITING FOR BOTH MATCH RESULTS":s==8?"RESULTS DISAGREE - STATS UNCHANGED":"CHECK NAME AND PASSWORD";}
static bool accountCall(unsigned command,const void*data,unsigned bytes,const char*label){
 if(!accountLink.ready){strcpy(accountNotice,"UPDATE SERVER AND COPY ACCOUNT KEY");return false;}
 if(ta_account_request(&accountLink,&publicRoom,command,data,bytes)){strcpy(accountNotice,"CONNECTING - PLEASE TRY AGAIN");return false;}
 uint32_t start=GetTickCount();uint16_t pads[2];TA_Input_Poll(pads);uint16_t previous=pads[0];
 while(accountLink.pending&&(uint32_t)(GetTickCount()-start)<6000){
  if(pump(false)){accountLink.pending=0;strcpy(accountNotice,"ROOM SERVER UNAVAILABLE");return false;}
  if(!accountLink.pending)break;
  TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];if(pressed&0x100){accountLink.pending=0;return false;}
  accountFrame(label,"SECURE CONNECTION TO TETRIS ATTACK");ta_vs_lan_text(screen,44,230,"PLEASE WAIT",0x00ffffff,3);accountFooter("B CANCEL",NULL);
 }
 if(accountLink.pending){accountLink.pending=0;strcpy(accountNotice,"SERVER TIMEOUT - PLEASE RETRY");return false;}
 if(accountLink.replyBytes<2||accountLink.reply[0]!=command){strcpy(accountNotice,"INVALID SERVER RESPONSE");return false;}
 if(accountLink.reply[1]){strcpy(accountNotice,accountStatus(accountLink.reply[1]));return false;}return true;
}
static bool accountKeyboard(const char*title,const char*label,char*out,unsigned maximum,bool password){
 static const char keys[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";unsigned key=0,length=0;char value[33]={0};uint16_t pads[2];TA_Input_Poll(pads);uint16_t previous=pads[0];
 for(;;){TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];if(pump(false))return false;
  if(pressed&0x100){memset(value,0,sizeof value);return false;}if(pressed&0x40)key=(key+39)%40;if(pressed&0x80)key=(key+1)%40;if(pressed&0x10)key=(key+32)%40;if(pressed&0x20)key=(key+8)%40;
  if(pressed&2){if(length)value[--length]=0;}if(pressed&1&&key<sizeof keys-1&&length<maximum){value[length++]=keys[key];value[length]=0;}
  if((pressed&8)&&length>=(password?8u:3u)){memcpy(out,value,length+1);memset(value,0,sizeof value);return true;}
  accountFrame(title,password?"8 TO 32 CHARACTERS - PASSWORD IS HIDDEN":"3 TO 15 LETTERS NUMBERS DASH OR UNDERSCORE");char shown[40];memset(shown,0,sizeof shown);if(password)memset(shown,'X',length);else memcpy(shown,value,length);ta_vs_lan_text(screen,42,156,label,0x009fb9ce,2);ta_vs_lan_text(screen,42,179,shown,0x00ffffff,3);
  for(unsigned k=0;k<40;k++){unsigned x=48+(k%8)*54,y=224+(k/8)*30;if(k==key)accountRect(x-9,y-7,34,26,0x00345163);char ch[2]={k<sizeof keys-1?keys[k]:' ',0};ta_vs_lan_text(screen,x,y,ch,k==key?0x006affa8:0x00ffffff,3);}
  accountFooter("A TYPE   X DELETE   START DONE","D-PAD MOVE   B CANCEL");
 }
}
static bool accountAuthenticate(unsigned command,const char*name,const void*credential){
 uint8_t data[48]={0};memcpy(data,name,strlen(name));memcpy(data+16,credential,32);
 accountAuthenticated=false;bool ok=accountCall(command,data,sizeof data,command==1?"CREATING PROFILE":"SIGNING IN");memset(data,0,sizeof data);if(!ok)return false;
 if(accountLink.replyBytes!=78||ta_account_profile(&ownProfile,accountLink.reply+34,44)){strcpy(accountNotice,"INVALID PROFILE RESPONSE");return false;}
 unsigned index=profileCount;for(unsigned i=0;i<profileCount;i++)if(!strcmp(savedProfiles[i].name,ownProfile.name))index=i;
 if(index==profileCount){if(profileCount==8){strcpy(accountNotice,"REMOVE A LOCAL PROFILE FIRST");accountAuthenticated=false;return false;}profileCount++;}
 memset(&savedProfiles[index],0,sizeof savedProfiles[index]);strcpy(savedProfiles[index].name,ownProfile.name);memcpy(savedProfiles[index].token,accountLink.reply+2,32);profileActive=index;accountAuthenticated=true;TA_Log("[TA N14 ACCOUNT] signed_in=1 profile=%s saved_profiles=%u\n",ownProfile.name,profileCount);strcpy(accountNotice,"SIGNED IN - READY TO PLAY");accountProfiles(true);return true;
}
static void accountProfileMenu(void){
 accountProfiles(false);unsigned selected=profileActive;uint16_t pads[2];TA_Input_Poll(pads);uint16_t previous=pads[0];
 for(;;){TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];if(pump(false))return;if(pressed&0x100)return;
  if(pressed&0x10)selected=selected?selected-1:profileCount+1;if(pressed&0x20)selected=(selected+1)%(profileCount+2);
  if((pressed&2)&&selected<profileCount){if(accountAuthenticated&&!strcmp(ownProfile.name,savedProfiles[selected].name)){accountAuthenticated=false;memset(&ownProfile,0,sizeof ownProfile);}for(unsigned i=selected;i+1<profileCount;i++)savedProfiles[i]=savedProfiles[i+1];memset(&savedProfiles[--profileCount],0,sizeof savedProfiles[0]);if(profileActive==selected)profileActive=0;else if(profileActive>selected)profileActive--;if(profileActive>=profileCount)profileActive=0;selected=profileActive;accountProfiles(true);strcpy(accountNotice,"REMOVED FROM THIS XBOX ONLY");}
  if(pressed&1){
   if(selected<profileCount){profileActive=selected;if(!accountAuthenticate(3,savedProfiles[selected].name,savedProfiles[selected].token)){char pass[33]={0};if(accountKeyboard("SIGN IN","PASSWORD",pass,32,true))accountAuthenticate(2,savedProfiles[selected].name,pass);memset(pass,0,sizeof pass);}}
   else if(profileCount==8)strcpy(accountNotice,"REMOVE A LOCAL PROFILE FIRST");
   else{char name[16]={0},pass[33]={0},confirm[33]={0};bool create=selected==profileCount;
    if(accountKeyboard(create?"CREATE PROFILE":"SIGN IN","USERNAME",name,15,false)&&accountKeyboard(create?"CREATE PROFILE":"SIGN IN","PASSWORD",pass,32,true)){
     if(!create||(accountKeyboard("CONFIRM PASSWORD","PASSWORD",confirm,32,true)&&!strcmp(pass,confirm)))accountAuthenticate(create?1:2,name,pass);else strcpy(accountNotice,"PASSWORDS DO NOT MATCH");}
    memset(pass,0,sizeof pass);memset(confirm,0,sizeof confirm);
   }
   TA_Input_Poll(pads);previous=pads[0];
  }
  accountFrame("PROFILES",accountNotice);unsigned first=selected>5?selected-5:0;
  for(unsigned row=0;row<6&&first+row<profileCount+2;row++){unsigned i=first+row,y=154+row*36;if(i==selected)accountRect(36,y-8,440,30,0x00314b61);char text[64];if(i<profileCount)snprintf(text,sizeof text,"%s%s",savedProfiles[i].name,accountAuthenticated&&!strcmp(ownProfile.name,savedProfiles[i].name)?" - ACTIVE":"");else strcpy(text,i==profileCount?"CREATE PROFILE":"SIGN IN / RECOVER");ta_vs_lan_text(screen,46,y,text,i==selected?0x006affa8:0x00ffffff,3);}
  accountFooter("A SELECT   X REMOVE LOCAL   B BACK","PASSWORDS ARE NEVER SAVED ON YOUR XBOX");
 }
}
static bool accountEnsure(void){
 if(accountAuthenticated)return true;if(profileCount&&accountAuthenticate(3,savedProfiles[profileActive].name,savedProfiles[profileActive].token))return true;accountProfileMenu();return accountAuthenticated;
}
static void accountRefreshProfile(void){if(accountAuthenticated&&accountCall(4,NULL,0,"REFRESHING PROFILE")&&accountLink.replyBytes==46)ta_account_profile(&ownProfile,accountLink.reply+2,44);}
static void accountDirectory(void){
 if(!accountAuthenticated)return;if(!accountCall(8,NULL,0,"REFRESHING ROOMS"))return;const uint8_t*b=accountLink.reply+2;unsigned bytes=accountLink.replyBytes-2;if(!bytes||b[0]>8||bytes!=1+b[0]*25)return;namedCount=b[0];
 for(unsigned i=0;i<namedCount;i++){const uint8_t*q=b+1+i*25;namedRooms[i].id=((uint32_t)q[0]<<24)|((uint32_t)q[1]<<16)|((uint32_t)q[2]<<8)|q[3];namedRooms[i].speed=q[4];memcpy(namedRooms[i].name,q+5,16);namedRooms[i].name[16]=0;namedRooms[i].rating=((uint32_t)q[21]<<24)|((uint32_t)q[22]<<16)|((uint32_t)q[23]<<8)|q[24];}
}
static void accountLadder(void){
 uint8_t data[2]={(uint8_t)(ladderOffset>>8),(uint8_t)ladderOffset};if(!accountCall(5,data,2,"LOADING LEADERBOARD"))return;const uint8_t*b=accountLink.reply+2;unsigned n=accountLink.replyBytes-2;if(n<5||b[4]>8||n!=5+b[4]*44)return;ladderOffset=((unsigned)b[0]<<8)|b[1];ladderTotal=((unsigned)b[2]<<8)|b[3];ladderCount=b[4];for(unsigned i=0;i<ladderCount;i++)if(ta_account_profile(&ladder[i],b+5+i*44,44)){ladderCount=0;break;}
}
static void accountStats(void){
 if(!accountAuthenticated){ta_vs_lan_text(screen,44,182,"YOUR VERSUS JOURNEY STARTS HERE",0x00ffffff,3);ta_vs_lan_text(screen,44,228,accountNotice,0x009fb9ce,2);ta_vs_lan_text(screen,44,302,"A CREATE OR SIGN IN",0x006affa8,3);return;}
 char text[80];snprintf(text,sizeof text,"%s",ownProfile.name);ta_vs_lan_text(screen,44,159,text,0x00ffffff,4);
 snprintf(text,sizeof text,"%s   %lu RATING",ta_account_tier(ownProfile.rating),(unsigned long)ownProfile.rating);ta_vs_lan_text(screen,44,204,text,0x006affa8,3);
 if(ownProfile.rank)snprintf(text,sizeof text,"GLOBAL RANK %lu",(unsigned long)ownProfile.rank);else strcpy(text,"GLOBAL RANK - PLAY YOUR FIRST MATCH");ta_vs_lan_text(screen,44,245,text,0x009fb9ce,2);
 snprintf(text,sizeof text,"WINS %lu     LOSSES %lu",(unsigned long)ownProfile.wins,(unsigned long)ownProfile.losses);ta_vs_lan_text(screen,44,280,text,0x00ffffff,3);
 snprintf(text,sizeof text,"DRAWS %lu   BEST WIN STREAK %lu",(unsigned long)ownProfile.draws,(unsigned long)ownProfile.best);ta_vs_lan_text(screen,44,319,text,0x009fb9ce,2);
 snprintf(text,sizeof text,"STREAK %ld   MATCHES %lu",(long)ownProfile.streak,(unsigned long)(ownProfile.wins+ownProfile.losses+ownProfile.draws));ta_vs_lan_text(screen,44,350,text,0x009fb9ce,2);
}
static int accountBrowser(uint32_t*localIp,uint16_t previous){
 unsigned tab=0,selected=0;uint32_t refreshed=0;bool triedResume=false;ladderCount=ladderOffset=ladderTotal=0;
 for(;;){uint16_t pads[2];TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];if(pressed&0x100){leavePublic();ta_vs_udp_close(&transport);return 1;}
  XNADDR address;DWORD status=XNetGetTitleXnAddr(&address);if(status&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_AUTO|XNET_GET_XNADDR_PPPOE)){*localIp=address.ina.s_addr;publicRoom.localAddress=*localIp;int error=pump(false);if(error)return error;}
  if(publicRoom.haveRoom)return 0;
  if(publicRoom.haveCookie&&!triedResume){triedResume=true;if(profileCount&&accountLink.ready)accountAuthenticate(3,savedProfiles[profileActive].name,savedProfiles[profileActive].token);}
  unsigned oldTab=tab;if(pressed&0x400)tab=(tab+2)%3;if(pressed&0x800)tab=(tab+1)%3;
  if(pressed&2){if(tab==2){if(accountEnsure())accountLadder();}else if(tab==1){accountRefreshProfile();}else{accountProfileMenu();if(accountAuthenticated)accountRefreshProfile();}TA_Input_Poll(pads);previous=pads[0];refreshed=0;}
  if(oldTab!=tab){if(tab==2&&accountEnsure())accountLadder();else if(tab==1)accountRefreshProfile();TA_Input_Poll(pads);previous=pads[0];}
  if(tab==0){
   if((pressed&0x10)&&selected)selected--;if((pressed&0x20)&&selected+1<publicRoom.roomCount)selected++;
   if(selected>=publicRoom.roomCount)selected=publicRoom.roomCount?publicRoom.roomCount-1:0;
   if(publicRoom.mode==TA_PUB_BROWSE&&(pressed&0x200)){if(accountEnsure()){lobby.role=0;ta_vs_public_host(&publicRoom);}TA_Input_Poll(pads);previous=pads[0];}
   if(publicRoom.mode==TA_PUB_BROWSE&&(pressed&1)&&selected<publicRoom.roomCount){if(accountEnsure())ta_vs_public_join(&publicRoom,publicRoom.rooms[selected].id);TA_Input_Poll(pads);previous=pads[0];}
  }else if(tab==1&&(pressed&1)){accountProfileMenu();TA_Input_Poll(pads);previous=pads[0];}
  else if(tab==2){if((pressed&0x80)&&ladderOffset+8<ladderTotal){ladderOffset+=8;accountLadder();}if((pressed&0x40)&&ladderOffset>=8){ladderOffset-=8;accountLadder();}}
  if(tab==0&&accountAuthenticated&&publicRoom.mode==TA_PUB_BROWSE&&(!refreshed||(uint32_t)(GetTickCount()-refreshed)>5000)){accountDirectory();refreshed=GetTickCount();}
  char sub[64];if(accountAuthenticated)snprintf(sub,sizeof sub,"%s   %s   %lu",ownProfile.name,ta_account_tier(ownProfile.rating),(unsigned long)ownProfile.rating);else strcpy(sub,accountNotice);
  accountFrame(tab==0?"PUBLIC ROOMS":tab==1?"MY PROFILE":"LEADERBOARD",sub);
  if(tab==0){
   if(!publicRoom.roomCount){ta_vs_lan_text(screen,44,201,"NO OPEN ROOMS YET",0x00ffffff,3);ta_vs_lan_text(screen,44,245,"Y HOST A NEW ROOM",0x006affa8,3);}
   unsigned first=selected>5?selected-5:0;
   for(unsigned row=0;row<6&&first+row<publicRoom.roomCount;row++){unsigned k=first+row,y=153+row*37;TaVsRoom*room=&publicRoom.rooms[k];if(k==selected)accountRect(36,y-7,440,31,0x00314b61);char text[64];const char*name=NULL;for(unsigned i=0;i<namedCount;i++)if(namedRooms[i].id==room->id)name=namedRooms[i].name;
    if(name)snprintf(text,sizeof text,"%-15s  SPEED %u",name,room->speed);else snprintf(text,sizeof text,"ROOM %06lu     SPEED %u",(unsigned long)room->id,room->speed);ta_vs_lan_text(screen,44,y,text,k==selected?0x006affa8:0x00ffffff,3);}
   accountFooter(publicRoom.mode==TA_PUB_HOST?"CREATING ROOM":publicRoom.mode==TA_PUB_JOINING?"JOINING ROOM":"A JOIN   Y HOST   X PROFILES","L/R TABS   UP/DOWN SELECT   B BACK");
  }else if(tab==1){accountStats();accountFooter("A SWITCH / CREATE / SIGN IN","L/R TABS   X REFRESH PROFILE   B BACK");}
  else{if(!ladderCount)ta_vs_lan_text(screen,44,213,"FINISH A PUBLIC MATCH TO RANK",0x00ffffff,3);
   for(unsigned i=0;i<ladderCount;i++){char text[64];snprintf(text,sizeof text,"%3lu  %-15s %4lu",(unsigned long)ladder[i].rank,ladder[i].name,(unsigned long)ladder[i].rating);ta_vs_lan_text(screen,42,151+i*29,text,!strcmp(ladder[i].name,ownProfile.name)?0x006affa8:0x00ffffff,3);}
   accountFooter("LEFT/RIGHT PAGE   X REFRESH","L/R TABS   PUBLIC MATCHES ONLY   B BACK");}
 }
}
static void accountFetchPeer(void){if(publicMode&&accountAuthenticated&&accountCall(9,NULL,0,"GETTING OPPONENT PROFILE")&&accountLink.replyBytes==46)ta_account_profile(&otherProfile,accountLink.reply+2,44);}
static void accountArm(void){
 reportDone=reportOutcome=0;reportEpoch=roundState.epoch;reportAt=0;if(!publicMode||!accountAuthenticated)return;
 uint8_t epoch[4]={(uint8_t)(reportEpoch>>24),(uint8_t)(reportEpoch>>16),(uint8_t)(reportEpoch>>8),(uint8_t)reportEpoch};
 if(!accountCall(6,epoch,4,"PREPARING RANKED MATCH")){reportDone=1;TA_Log("[TA N14 ACCOUNT] rank_arm_failed status=%u\n",accountLink.replyBytes>=2?accountLink.reply[1]:255);}
}
static void accountReport(bool lost){
 if(!publicMode||!accountAuthenticated||reportDone)return;
 if(!reportOutcome)reportOutcome=lost?(peerLost?3:2):1;
 if(accountLink.pending)return;
 if(accountLink.replyBytes&&accountLink.reply[0]==7){unsigned status=accountLink.reply[1];if(accountLink.replyBytes==46)ta_account_profile(&ownProfile,accountLink.reply+2,44);if(status!=7){reportDone=1;strcpy(accountNotice,status?accountStatus(status):"MATCH RECORDED - PROFILE UPDATED");TA_Log("[TA N14 ACCOUNT] result_epoch=%lu status=%u rating=%lu wins=%lu losses=%lu\n",(unsigned long)reportEpoch,status,(unsigned long)ownProfile.rating,(unsigned long)ownProfile.wins,(unsigned long)ownProfile.losses);return;}accountLink.replyBytes=0;}
 if(reportAt&&(uint32_t)(GetTickCount()-reportAt)<700)return;
 uint8_t data[13];uint64_t nonce=publicRoom.roomNonce;for(unsigned i=0;i<8;i++)data[7-i]=(uint8_t)(nonce>>(8*i));data[8]=(uint8_t)(reportEpoch>>24);data[9]=(uint8_t)(reportEpoch>>16);data[10]=(uint8_t)(reportEpoch>>8);data[11]=(uint8_t)reportEpoch;data[12]=(uint8_t)reportOutcome;
 if(!ta_account_request(&accountLink,&publicRoom,7,data,sizeof data))reportAt=GetTickCount();
}
static void accountDrainReport(void){if(!publicMode||!reportOutcome||reportDone)return;uint32_t start=GetTickCount();while(!reportDone&&(uint32_t)(GetTickCount()-start)<1500){accountReport(reportOutcome!=1);if(pump(false))break;accountFrame("SAVING MATCH RESULT","YOUR ROOM STAYS CONNECTED");accountFooter("UPDATING PROFILE",NULL);}accountLink.pending=0;}
