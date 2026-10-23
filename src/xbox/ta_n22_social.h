/* Shared R132 Xbox/Android social frontend. The encrypted account channel is
   the only way to access world chat; packets never use the game relay. */
#ifndef TA_N22_SOCIAL_H
#define TA_N22_SOCIAL_H

typedef struct {uint32_t id;char name[17];char text[33];} TaSocialLine;
static TaSocialLine taSocialLines[4];
static unsigned taSocialLineCount,taSocialAccounts,taSocialInGame,taSocialOnline,taSocialCountKnown;
static uint32_t taSocialLastId,taSocialRequestAt,taSocialCountsAt,taSocialChatAt;
static unsigned taSocialCommand;
static char taSocialNotice[64]="CHAT READY";
static int (*taSocialExtraTick)(void);
static bool taSocialLobbyChannel;
static unsigned taSocialRoomKind;
static uint32_t taSocialRoomId;
static uint64_t taSocialRoomNonce;
static int taSocialPump(void){int error=pump(false);return error?error:taSocialExtraTick?taSocialExtraTick():0;}
static unsigned taSocialPollCommand(void){return taSocialLobbyChannel?TA_ACCOUNT_LOBBY_POLL:TA_ACCOUNT_CHAT_POLL;}
static unsigned taSocialSendCommand(void){return taSocialLobbyChannel?TA_ACCOUNT_LOBBY_SEND:TA_ACCOUNT_CHAT_SEND;}
static void taSocialW32(uint8_t*,uint32_t);
static unsigned taSocialRoomHeader(uint8_t*p){p[0]=(uint8_t)taSocialRoomKind;taSocialW32(p+1,taSocialRoomId);for(unsigned k=0;k<8;k++)p[12-k]=(uint8_t)(taSocialRoomNonce>>(8*k));return 13;}

static uint32_t taSocialU32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void taSocialW32(uint8_t*p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static void taSocialCancel(void){
 if(taSocialCommand && accountLink.pending)accountLink.pending=0;
 taSocialCommand=0;accountLink.replyBytes=0;
}
static void taSocialReset(void){
 taSocialCancel();taSocialCountKnown=0;taSocialCountsAt=0;taSocialChatAt=0;
}
static void taSocialBegin(unsigned cmd,const void*data,unsigned bytes,uint32_t now){
 if(accountLink.pending||taSocialCommand||!accountAuthenticated||!publicRoom.haveCookie)return;
 if(!ta_account_request(&accountLink,&publicRoom,cmd,data,bytes)){
  taSocialCommand=cmd;taSocialRequestAt=now;
 }
}
static void taSocialTick(uint32_t now,bool counts){
 if(taSocialCommand){
  if(accountLink.pending){if((uint32_t)(now-taSocialRequestAt)>4500){
   taSocialCancel();strcpy(taSocialNotice,taSocialLobbyChannel?"LOBBY CHAT NEEDS SERVER UPDATE":"SERVICE TIMEOUT - RETRYING");
  }return;}
  if(accountLink.replyBytes>=2 && accountLink.reply[0]==taSocialCommand && !accountLink.reply[1]){
   const uint8_t*b=accountLink.reply;unsigned n=accountLink.replyBytes;
   if(taSocialCommand==TA_ACCOUNT_COUNTS && n==14){
    taSocialAccounts=taSocialU32(b+2);taSocialInGame=taSocialU32(b+6);
    taSocialOnline=taSocialU32(b+10);taSocialCountKnown=1;TA_Video_SetPopulation(taSocialAccounts,taSocialInGame,taSocialOnline,1);
   }else if(taSocialCommand==taSocialPollCommand() && n>=3){
    unsigned count=b[2];if(count<=4&&n==3+count*52){
     for(unsigned i=0;i<count;i++){
      const uint8_t*e=b+3+i*52;uint32_t id=taSocialU32(e);
      if(id<=taSocialLastId)continue;
      TaSocialLine *m;
      if(taSocialLineCount==4){memmove(taSocialLines,taSocialLines+1,3*sizeof taSocialLines[0]);m=taSocialLines+3;}
      else m=taSocialLines+taSocialLineCount++;
      m->id=id;memcpy(m->name,e+4,16);m->name[16]=0;
      memcpy(m->text,e+20,32);m->text[32]=0;taSocialLastId=id;
     }
    }
   }
   else if(taSocialCommand==taSocialSendCommand())strcpy(taSocialNotice,"MESSAGE SENT");
  }else if(taSocialCommand==taSocialPollCommand()||taSocialCommand==taSocialSendCommand()){
   unsigned status=accountLink.replyBytes>=2?accountLink.reply[1]:1;
   if(status==5){accountAuthenticated=false;strcpy(accountNotice,"PLEASE SIGN IN AGAIN");}
   strcpy(taSocialNotice,status==6?"WAITING FOR LOBBY CHAT CONNECTION":status==1&&taSocialLobbyChannel?"LOBBY CHAT NEEDS SERVER UPDATE":accountStatus(status));
  }
  taSocialCommand=0;accountLink.replyBytes=0;
 }
 if(!taSocialCommand && !accountLink.pending && accountAuthenticated){
  if(counts){if(!taSocialCountsAt||(uint32_t)(now-taSocialCountsAt)>10000){
    taSocialCountsAt=now;taSocialBegin(TA_ACCOUNT_COUNTS,NULL,0,now);
   }}else if(!taSocialChatAt||(uint32_t)(now-taSocialChatAt)>1400){
   uint8_t after[17];unsigned offset=taSocialLobbyChannel?taSocialRoomHeader(after):0;taSocialW32(after+offset,taSocialLastId);taSocialChatAt=now;
   taSocialBegin(taSocialPollCommand(),after,offset+4,now);
  }
 }
}
static void taSocialDrawCounts(int y){
 char label[95];if(taSocialCountKnown)snprintf(label,sizeof label,"ACCOUNTS %u   IN-GAME %u   ONLINE %u",taSocialAccounts,taSocialInGame,taSocialOnline);
 else strcpy(label,"ACCOUNTS --   IN-GAME --   ONLINE --");
 ta_menu_text(screen,43,y,label,0x0096e3b2,2);
}
#ifndef TA_ANDROID
/* 8-wide alphabet/digit grid; no accidental wrap to an unrelated column
   when crossing into the shorter bottom row (the old modulo-44 bug). */
static unsigned taSocialKeyMove(unsigned selected,unsigned pressed){
 unsigned row=selected/8,col=selected%8;
 const unsigned lastCol[6]={7,7,7,7,7,3};
 if(pressed&0x40)col=col?col-1:lastCol[row];
 if(pressed&0x80)col=col<lastCol[row]?col+1:0;
 if(pressed&0x10)row=row?row-1:5;
 if(pressed&0x20)row=row<5?row+1:0;
 if(col>lastCol[row])col=lastCol[row];
 return row*8+col;
}
static bool taSocialXboxKeyboard(char out[33]){
 static const char keys[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ .!?";
 unsigned selected=0,len=0;char value[33]={0};
 TaMenuPad pad;menuBegin(&pad);
 for(;;){
  unsigned pressed=menuRead(&pad);if(taSocialPump()||pressed&0x100)return false;
  selected=taSocialKeyMove(selected,pressed);
  /* Shortcuts: X delete; Y space; Start send; L/R jump to space/send.
     A on the last two on-screen keys is an equally accessible alternative. */
  if(pressed&0x400)selected=38;
  if(pressed&0x800)selected=43;
  if((pressed&2)||((pressed&1)&&selected==42)){if(len)value[--len]=0;}
  else if((pressed&0x200)&&len<32){value[len++]=' ';value[len]=0;}
  else if((pressed&1)&&selected<42&&len<32){value[len++]=keys[selected];value[len]=0;}
  if(((pressed&8)||((pressed&1)&&selected==43))&&len){memcpy(out,value,len+1);memset(value,0,sizeof value);return true;}
  accountFrame(taSocialLobbyChannel?"WRITE LOBBY CHAT":"WRITE WORLD CHAT","32 CHARACTERS MAXIMUM");
  ta_n17_panel(38,146,438,41);
  ta_menu_text(screen,43,157,value[0]?value:"TYPE YOUR MESSAGE...",value[0]?uiText:uiMuted,2);
  for(unsigned k=0;k<44;k++){
   const unsigned x=48+(k%8)*54,y=204+(k/8)*30;
   if(k==selected)accountHighlight((int)x-6,(int)y-7,52,27);
   char ch[2]={k<42?keys[k]:' ',0};
   const char*label=k==42?"DEL":k==43?"SEND":k==38?"SPACE":ch;
   ta_vs_lan_text_box(screen,(int)x,(int)y,44,14,label,k==selected?uiText:uiMuted,2);
  }
  accountFooter("D-PAD MOVE  A TYPE  B BACK","X DELETE  Y SPACE  START SEND",0,true);
 }
}
#else
extern "C" int ta_android_chat_input(char*,unsigned,const char*,int (*)(void));
#endif
static bool taSocialInput(char out[33]){
#ifdef TA_ANDROID
 return ta_android_chat_input(out,33,taSocialLobbyChannel?"Lobby chat":"World chat",taSocialPump)!=0;
#else
 return taSocialXboxKeyboard(out);
#endif
}
static void taSocialHistoryReset(void){
 taSocialCancel();taSocialLastId=0;taSocialLineCount=0;taSocialChatAt=0;
 strcpy(taSocialNotice,"CHAT READY - AUTO REFRESH");
}
static void taSocialChatView(void){
 taSocialHistoryReset();TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(taSocialPump())break;
  if(pressed&0x100)break;
  if((pressed&2u)&&taSocialRoomKind){taSocialLobbyChannel=!taSocialLobbyChannel;taSocialHistoryReset();menuBegin(&pad);continue;}
  taSocialTick(GetTickCount(),false);if(!accountAuthenticated)break;
  if((pressed&1u)&&!(taSocialCommand==taSocialSendCommand()&&accountLink.pending)){
   taSocialCancel();char message[33]={0};
   if(taSocialInput(message)&&!taSocialPump()){
    uint8_t data[45];unsigned offset=taSocialLobbyChannel?taSocialRoomHeader(data):0,n=(unsigned)strlen(message);
    if(n){memcpy(data+offset,message,n);taSocialBegin(taSocialSendCommand(),data,offset+n,GetTickCount());strcpy(taSocialNotice,"SENDING MESSAGE");}
   }
   memset(message,0,sizeof message);taSocialChatAt=0;menuBegin(&pad);
  }
  accountFrame(taSocialLobbyChannel?"LOBBY CHAT":"WORLD CHAT",ownProfile.name);
  if(!taSocialLineCount)ta_menu_text(screen,44,174,"NO MESSAGES YET",uiMuted,2);
  for(unsigned i=0;i<taSocialLineCount;i++){
   unsigned y=160+i*59;ta_menu_text(screen,44,y,taSocialLines[i].name,uiGold,2);
   ta_menu_text(screen,44,y+23,taSocialLines[i].text,uiText,2);
  }
  accountFooter(taSocialRoomKind?(taSocialLobbyChannel?"A WRITE  X WORLD  B BACK":"A WRITE  X LOBBY  B BACK"):"A WRITE  B BACK",taSocialNotice);
 }
 taSocialCancel();
}
static void taSocialChatMenu(int (*tick)(void)){
 taSocialCancel();if(!accountEnsure())return;
 taSocialExtraTick=tick;taSocialRoomKind=0;taSocialLobbyChannel=false;taSocialChatView();taSocialExtraTick=NULL;
}
static void taSocialLobbyMenu(int (*tick)(void),unsigned kind,uint32_t room,uint64_t nonce){
 if(!accountAuthenticated||!room)return;
 taSocialCancel();taSocialExtraTick=tick;taSocialRoomKind=kind;taSocialRoomId=room;taSocialRoomNonce=nonce;
 unsigned choice=0;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(taSocialPump()||!accountAuthenticated||(pressed&0x100u))break;
  choice=menuMove(choice,3,pressed);
  if(pressed&1u){if(choice==2)break;taSocialLobbyChannel=choice==0;taSocialChatView();menuBegin(&pad);continue;}
  accountFrame("CHAT","STAY CONNECTED WHILE WAITING FOR PLAYERS");
  menuRow(174,"LOBBY CHAT",choice==0);menuRow(229,"WORLD CHAT",choice==1);menuRow(284,"BACK TO LOBBY",choice==2);
  accountFooter("A OPEN  B BACK","CHAT CLOSES WHEN THE MATCH STARTS");
 }
 taSocialCancel();taSocialExtraTick=NULL;taSocialRoomKind=0;taSocialLobbyChannel=false;
}
#endif
