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
   taSocialCancel();strcpy(taSocialNotice,"SERVICE TIMEOUT - RETRYING");
  }return;}
  if(accountLink.replyBytes>=2 && accountLink.reply[0]==taSocialCommand && !accountLink.reply[1]){
   const uint8_t*b=accountLink.reply;unsigned n=accountLink.replyBytes;
   if(taSocialCommand==TA_ACCOUNT_COUNTS && n==14){
    taSocialAccounts=taSocialU32(b+2);taSocialInGame=taSocialU32(b+6);
    taSocialOnline=taSocialU32(b+10);taSocialCountKnown=1;TA_Video_SetPopulation(taSocialAccounts,taSocialInGame,taSocialOnline,1);
   }else if(taSocialCommand==TA_ACCOUNT_CHAT_POLL && n>=3){
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
  }else if(taSocialCommand==TA_ACCOUNT_CHAT_POLL){strcpy(taSocialNotice,"CHAT SERVICE NOT AVAILABLE");}
  taSocialCommand=0;accountLink.replyBytes=0;
 }
 if(!taSocialCommand && !accountLink.pending && accountAuthenticated){
  if(counts){if(!taSocialCountsAt||(uint32_t)(now-taSocialCountsAt)>10000){
    taSocialCountsAt=now;taSocialBegin(TA_ACCOUNT_COUNTS,NULL,0,now);
   }}else if(!taSocialChatAt||(uint32_t)(now-taSocialChatAt)>1400){
   uint8_t after[4];taSocialW32(after,taSocialLastId);taSocialChatAt=now;
   taSocialBegin(TA_ACCOUNT_CHAT_POLL,after,4,now);
  }
 }
}
static void taSocialDrawCounts(int y){
 char label[95];if(taSocialCountKnown)snprintf(label,sizeof label,"ACCOUNTS %u   IN-GAME %u   ONLINE %u",taSocialAccounts,taSocialInGame,taSocialOnline);
 else strcpy(label,"ACCOUNTS --   IN-GAME --   ONLINE --");
 ta_vs_lan_text(screen,43,y,label,0x0096e3b2,2);
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
  unsigned pressed=menuRead(&pad);if(pump(false)||pressed&0x100)return false;
  selected=taSocialKeyMove(selected,pressed);
  /* Shortcuts: X delete; Y space; Start send; L/R jump to space/send.
     A on the last two on-screen keys is an equally accessible alternative. */
  if(pressed&0x400)selected=38;
  if(pressed&0x800)selected=43;
  if((pressed&2)||((pressed&1)&&selected==42)){if(len)value[--len]=0;}
  else if((pressed&0x200)&&len<32){value[len++]=' ';value[len]=0;}
  else if((pressed&1)&&selected<42&&len<32){value[len++]=keys[selected];value[len]=0;}
  if(((pressed&8)||((pressed&1)&&selected==43))&&len){memcpy(out,value,len+1);memset(value,0,sizeof value);return true;}
  accountFrame("WRITE WORLD CHAT","32 CHARACTERS MAXIMUM");
  ta_n17_panel(38,146,438,41);
  ta_vs_lan_text(screen,43,157,value[0]?value:"TYPE YOUR MESSAGE...",value[0]?uiText:uiMuted,2);
  for(unsigned k=0;k<44;k++){
   const unsigned x=48+(k%8)*54,y=204+(k/8)*30;
   if(k==selected)accountHighlight((int)x-6,(int)y-7,52,27);
   char ch[2]={k<42?keys[k]:' ',0};
   const char*label=k==42?"DEL":k==43?"SEND":k==38?"SPACE":ch;
   ta_vs_lan_text(screen,(int)x,(int)y,label,k==selected?uiText:uiMuted,k==38?1:k>=42?2:3);
  }
  accountFooter("D-PAD MOVE  A TYPE  B BACK","X DELETE  Y SPACE  START SEND");
 }
}
#else
extern "C" int ta_android_chat_input(char*,unsigned);
#endif
static bool taSocialInput(char out[33]){
#ifdef TA_ANDROID
 return ta_android_chat_input(out,33)!=0;
#else
 return taSocialXboxKeyboard(out);
#endif
}
static void taSocialChatMenu(void){
 taSocialCancel();if(!accountEnsure())return;
 taSocialLastId=0;taSocialLineCount=0;taSocialChatAt=0;
 strcpy(taSocialNotice,"CHAT READY - AUTO REFRESH");
 TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(pump(false)){taSocialCancel();return;}
  uint32_t now=GetTickCount();taSocialTick(now,false);
  if(pressed&0x100){taSocialCancel();return;}
  if(pressed&1){
   taSocialCancel();char message[33]={0};
   if(taSocialInput(message)){
    unsigned n=(unsigned)strlen(message);
    if(n && accountCall(TA_ACCOUNT_CHAT_SEND,message,n,"SENDING MESSAGE",true))strcpy(taSocialNotice,"MESSAGE SENT");
    else if(accountLastStatus>=0)snprintf(taSocialNotice,sizeof taSocialNotice,"%s",accountStatus((unsigned)accountLastStatus));
    else strcpy(taSocialNotice,"MESSAGE NOT SENT");
   }
   taSocialChatAt=0;menuBegin(&pad);
  }
  accountFrame("WORLD CHAT",accountAuthenticated?ownProfile.name:"SIGN IN TO CHAT");
  if(!taSocialLineCount)ta_vs_lan_text(screen,44,174,"NO MESSAGES YET",uiMuted,2);
  for(unsigned i=0;i<taSocialLineCount;i++){
   unsigned y=153+i*59;ta_vs_lan_text(screen,44,y,taSocialLines[i].name,uiGold,2);
   ta_vs_lan_text(screen,44,y+23,taSocialLines[i].text,uiText,2);
  }
  accountFooter("A WRITE  B BACK",taSocialNotice);
 }
}
#endif
