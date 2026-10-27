#ifndef TA_N22_COMMUNITY_H
#define TA_N22_COMMUNITY_H
#include "ta_vs_community.h"
#include "ta_vs_rank.h"
/* Font shapes match the version label, at a smaller 7x10 pixel size. */
#define font_rows ta_community_font_rows
#include "ta_vs_font5x7.h"
#undef font_rows
static void communityText(int x,int y,const char*s,uint32_t color){
 if(!s)return;TaMenuRect b=ta_menu_text_region(y);if(y<b.top)y=b.top;
 for(;*s&&x+7<=b.right;s++,x+=8){if(x<b.left)continue;const uint8_t*g=ta_community_font_rows(*s);for(int dy=0;dy<10;dy++)for(int dx=0;dx<7;dx++)if(y+dy<b.bottom&&(g[dy*7/10]&(1u<<(4-dx*5/7))))screen[(y+dy)*512+x+dx]=color;}
}
static void communityBar(unsigned first,unsigned visible,unsigned total,int top,int height){
 if(total<=visible)return;unsigned thumb=(unsigned)height*visible/total;if(thumb<12)thumb=12;
 accountRect(470,top,3,height,0x273852);accountRect(470,top+(height-thumb)*first/(total-visible),3,thumb,0x7dd9e8);
}
static bool accountEnsure(void);
static void communityProfileView(const char*name);
#define communityExtraTick accountMenuExtraTick
static int communityPump(void){int e=pump(false);return e?e:communityExtraTick?communityExtraTick():0;}
static TaCommunityClock communityClock;
typedef struct{TaAccountProfile stats;char zone[49],discord[34],now[33],windows[7][49];unsigned days,start,finish,count;}TaCommunityProfile;
static TaCommunityProfile communitySelf;
static bool communityReady,communityTwelve;
static bool communityTimeSetup(bool required);
static bool communityDate(char out[17],const char*title="EVENT DATE AND TIME",bool manual=false);
static char communityOwner[17];
static void taCommunityInvalidate(void){communityReady=false;communityClock.known=false;communityOwner[0]=0;}
static bool communityCall(unsigned op,const void*data,unsigned n,const char*label){
 if(n>549)return false;uint8_t request[550];request[0]=(uint8_t)op;if(n)memcpy(request+1,data,n);
 bool ok=accountCall(TA_COMMUNITY_COMMAND,request,n+1,label);
 if(!ok&&accountLastStatus!= -2&&accountLink.replyBytes<2)strcpy(accountNotice,"EVENTS / PROFILES NEED SERVER V1.2.9");
 return ok;
}
static bool communityParseProfile(TaCommunityProfile*out){
 const uint8_t*b=accountLink.reply+2;unsigned n=accountLink.replyBytes>=2?accountLink.replyBytes-2:0;
 if(n<171||b[170]>7||n!=171u+b[170]*48u)return false;
 TaCommunityProfile p;memset(&p,0,sizeof p);
 if(ta_account_profile(&p.stats,b+8,44)||!ta_community_string(p.zone,b+52,48)||!ta_community_string(p.discord,b+100,33)||!ta_community_string(p.now,b+138,32))return false;
 p.days=b[133];p.start=ta_community_r16(b+134);p.finish=ta_community_r16(b+136);p.count=b[170];
 if(p.days>127||p.start>=1440||p.finish>=1440)return false;
 for(unsigned i=0;i<p.count;i++)if(!ta_community_string(p.windows[i],b+171+i*48,48))return false;
 if(!ta_community_clock_set(&communityClock,ta_community_r64(b),GetTickCount()))return false;
 *out=p;return true;
}
static bool communityLoadSelf(void){
 if(!accountAuthenticated||!communityCall(0,NULL,0,"SYNCING PROFILE AND SERVER TIME")||!communityParseProfile(&communitySelf))return false;
 ownProfile=communitySelf.stats;strcpy(communityOwner,ownProfile.name);communityReady=ta_community_configured_zone(communitySelf.zone);
 if(!communityCall(12,NULL,0,"SYNCING TIME FORMAT")||accountLink.replyBytes!=3||accountLink.reply[2]>1){communityReady=false;strcpy(accountNotice,"TIME OPTIONS NEED PRIVATE SERVER V1.2.9");return false;}
 communityTwelve=accountLink.reply[2]!=0;return true;
}
static bool communitySaveProfile(const TaCommunityProfile*p){
 uint8_t data[86]={0};memcpy(data,p->zone,strlen(p->zone));memcpy(data+48,p->discord,strlen(p->discord));data[81]=(uint8_t)p->days;ta_community_w16(data+82,p->start);ta_community_w16(data+84,p->finish);
 if(!communityCall(2,data,sizeof data,"SAVING PUBLIC PROFILE")||!communityParseProfile(&communitySelf))return false;
 communityReady=ta_community_configured_zone(communitySelf.zone);strcpy(communityOwner,ownProfile.name);strcpy(accountNotice,"PROFILE SAVED");return true;
}
#if defined(TA_ANDROID)
extern "C" int ta_android_chat_input(char*,unsigned,const char*,int (*)(void));
#endif
/* Console text entry also exposes punctuation and case, for Discord usernames. */
static bool communityKeyboard(const char*title,char*out,unsigned maximum){
#if defined(TA_PC)
 return TA_PC_TextInput(out,maximum+1,title,"TEXT",false,1,communityPump)!=0;
#elif defined(TA_ANDROID)
 return ta_android_chat_input(out,maximum+1,title,communityPump)!=0;
#else
 static const char keys[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_. /:!?";
 const unsigned letters=sizeof(keys)-1,count=letters+2;char value[49]={0};unsigned len=0,key=0;bool lower=false;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(communityPump()||pressed&0x100)return false;if(menuTouchKey<count)key=menuTouchKey;
  unsigned row=key/8,col=key%8,last=(count-1)/8,lastcol=(row==last?(count-1)%8:7);
  if(pressed&0x40)col=col?col-1:lastcol;if(pressed&0x80)col=col<lastcol?col+1:0;
  if(pressed&0x10)row=row?row-1:last;if(pressed&0x20)row=row<last?row+1:0;
  lastcol=row==last?(count-1)%8:7;if(col>lastcol)col=lastcol;key=row*8+col;
  if(pressed&0x400)lower=!lower;
  if((pressed&2)||((pressed&1)&&key==letters)){if(len)value[--len]=0;}
  else if((pressed&0x200)&&len<maximum){value[len++]=' ';value[len]=0;}
  else if((pressed&1)&&key<letters&&len<maximum){char c=keys[key];if(lower&&c>='A'&&c<='Z')c=(char)(c+32);value[len++]=c;value[len]=0;}
  if(((pressed&8)||((pressed&1)&&key==letters+1))&&len){strcpy(out,value);return true;}
  accountFrame(title,lower?"LOWERCASE - L SWITCHES CASE":"UPPERCASE - L SWITCHES CASE");communityText(42,160,value,uiText);
  for(unsigned k=0;k<count;k++){unsigned x=48+k%8*54,y=202+k/8*30;menuTouchAction(x-5,y-6,52,26,0x1000001u|(k<<16));if(k==key)accountHighlight(x-5,y-6,52,26);char c[2]={k<letters?keys[k]:' ',0};if(lower&&c[0]>='A'&&c[0]<='Z')c[0]+=32;ta_vs_lan_text_box(screen,x,y,44,14,k==letters?"DEL":k==letters+1?"DONE":k<letters&&keys[k]==' '?"SPACE":c,k==key?uiGold:uiText,2);}
  accountFooter("A TYPE  X DELETE  Y SPACE","L CASE  START DONE  B CANCEL");
 }
#endif
}
typedef struct{const char*zone,*label;}TaCommunityZoneChoice;
static const TaCommunityZoneChoice communityQuickZones[]={
 {"America/New_York","US EASTERN - NEW YORK"},
 {"America/Chicago","US CENTRAL - CHICAGO"},
 {"America/Denver","US MOUNTAIN - DENVER"},
 {"America/Los_Angeles","US PACIFIC - LOS ANGELES"},
 {"America/Phoenix","ARIZONA - PHOENIX"},
 {"America/Anchorage","ALASKA - ANCHORAGE"},
 {"Pacific/Honolulu","HAWAII - HONOLULU"},
 {"America/Toronto","CANADA EASTERN - TORONTO"},
 {"America/Winnipeg","CANADA CENTRAL - WINNIPEG"},
 {"America/Edmonton","CANADA MOUNTAIN - EDMONTON"},
 {"America/Vancouver","CANADA PACIFIC - VANCOUVER"},
 {"America/Halifax","CANADA ATLANTIC - HALIFAX"},
 {"America/St_Johns","NEWFOUNDLAND - ST JOHNS"},
 {"America/Regina","SASKATCHEWAN - REGINA"},
 {"America/Whitehorse","YUKON - WHITEHORSE"}
};
static const char*communityLocationName(const char*z){
 static char label[49];if(ta_community_manual_zone(z)){snprintf(label,sizeof label,"MANUAL TIME UTC%s",z+7);return label;}for(unsigned i=0;i<sizeof(communityQuickZones)/sizeof(communityQuickZones[0]);i++)if(!strcmp(z,communityQuickZones[i].zone))return communityQuickZones[i].label;
 const char*city=strchr(z,'/');snprintf(label,sizeof label,"%s",city?city+1:z);for(unsigned k=0;label[k];k++)if(label[k]=='_')label[k]=' ';return label;
}
static bool communityChooseZone(char out[49],bool required){
 static const char*regions[]={"USA / CANADA","AMERICAS","EUROPE","ASIA","AFRICA","AUSTRALIA","PACIFIC","ATLANTIC","INDIAN OCEAN","ANTARCTICA","ARCTIC"};
 /* Preserve server region IDs; the old fixed-offset region 8 is not exposed. */
 static const uint8_t regionIds[]={255,0,1,2,3,4,5,6,7,9,10};
 const unsigned regionCount=sizeof(regions)/sizeof(regions[0]),quickCount=sizeof(communityQuickZones)/sizeof(communityQuickZones[0]);
 unsigned region=0,selected=0,offset=~0u,total=0,count=0;bool choosing=false;char zones[8][49];TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned pressed=menuRead(&pad);if(communityPump())return false;
  if(pressed&0x100){if(choosing){choosing=false;selected=region;menuBegin(&pad);continue;}return false;}
  if(!choosing){selected=menuMove(selected,regionCount,pressed);if(pressed&1){region=selected;selected=0;offset=~0u;total=count=0;choosing=true;menuBegin(&pad);continue;}
   accountFrame("SELECT TIME ZONE","SELECT LOCATION FOR A TIME ZONE\nSO EVENTS TIMES MATCH.");unsigned first=selected>5?selected-5:0;for(unsigned r=0;r<6&&first+r<regionCount;r++)menuRow(160+r*36,regions[first+r],selected==first+r,2);
  }else{
   bool quick=regionIds[region]==255;if(quick)total=quickCount;
   if(total)selected=menuMove(selected,total,pressed);unsigned page=selected/8*8;
   if(offset!=page){
    if(quick){offset=page;count=quickCount-page<8?quickCount-page:8;for(unsigned i=0;i<count;i++)strcpy(zones[i],communityQuickZones[page+i].zone);}
    else{uint8_t req[3]={regionIds[region],0,0};ta_community_w16(req+1,page);if(!communityCall(1,req,3,"LOADING LOCATIONS"))return false;const uint8_t*b=accountLink.reply+2;unsigned n=accountLink.replyBytes-2;
     if(n<3||b[2]>8||n!=3u+b[2]*48u)return false;total=ta_community_r16(b);count=b[2];offset=page;for(unsigned i=0;i<count;i++)if(!ta_community_string(zones[i],b+3+i*48,48))return false;
    }menuBegin(&pad);
   }
   if((pressed&1)&&selected>=offset&&selected-offset<count){if(ta_community_location_zone(zones[selected-offset])){strcpy(out,zones[selected-offset]);return true;}strcpy(accountNotice,"CHOOSE A CITY, NOT A FIXED UTC OFFSET");}
   accountFrame("SELECT TIME ZONE","LOCAL CLOCK CHANGES ARE AUTOMATIC");unsigned first=selected>5?selected-5:0;if(first<offset)first=offset;
   for(unsigned r=0;r<6&&first+r<offset+count;r++){unsigned i=first+r;char label[49];if(quick)strcpy(label,communityQuickZones[i].label);else{const char*city=strchr(zones[i-offset],'/');strcpy(label,city?city+1:zones[i-offset]);for(unsigned k=0;label[k];k++)if(label[k]=='_')label[k]=' ';}menuRow(160+r*36,label,i==selected,2);}
   communityBar(selected,1,total,158,224);
  }
  accountFooter("A SELECT  B BACK","NO NEED TO CHANGE SUMMER / WINTER TIME");
 }
}

static bool communityEnsure(void){
 if(!accountAuthenticated)return false;
 if(communityReady&&!strcmp(communityOwner,ownProfile.name)&&ta_community_clock_now(&communityClock,GetTickCount()))return true;
 if(!communityLoadSelf())return false;
 if(communityReady)return true;
 return communityTimeSetup(true);
}
static bool communityDays(unsigned*mask){
 static const char*days[]={"MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY","SUNDAY","DONE"};unsigned selected=0,value=*mask;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return false;selected=menuMove(selected,8,p);if(p&1){if(selected==7){*mask=value;return true;}value^=1u<<selected;}
  accountFrame("DAYS OF THE WEEK","SELECT DAYS - DONE SAVES THE SELECTION");unsigned first=selected>5?selected-5:0;for(unsigned r=0;r<6&&first+r<8;r++){unsigned i=first+r;char s[32];snprintf(s,sizeof s,"%s %s",i<7?(value&(1u<<i)?"ON ":"OFF"):"",days[i]);menuRow(160+r*36,s,i==selected,2);}accountFooter("A TOGGLE / DONE  B CANCEL",NULL);
 }
}
static bool communityTime(const char*title,unsigned*out){
 unsigned val=*out,selected=0,count=communityTwelve?4:3;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return false;selected=menuMove(selected,count,p);
  if(p&(1u|0x40u|0x80u)){int step=p&0x40?-1:1;int h=(int)(val/60),m=(int)(val%60);
   if(selected==0){h=communityTwelve?(h/12)*12+(h%12+12+step)%12:(h+24+step)%24;val=(unsigned)(h*60+m);}
   else if(selected==1)val=(unsigned)(h*60+(m+60+step)%60);
   else if(communityTwelve&&selected==2)val=(val+720)%1440;else if(p&1){*out=val;return true;}
  }
  accountFrame(title,communityTwelve?"12-HOUR TIME - SELECT AM OR PM":"24-HOUR TIME");char s[32];unsigned h=val/60;
  snprintf(s,sizeof s,"HOUR     %02u",communityTwelve?(h%12?h%12:12):h);menuRow(174,s,selected==0,2);
  snprintf(s,sizeof s,"MINUTE   %02u",val%60);menuRow(224,s,selected==1,2);
  if(communityTwelve){snprintf(s,sizeof s,"AM / PM  %s",h<12?"AM":"PM");menuRow(274,s,selected==2,2);}menuRow(communityTwelve?324:274,"DONE",selected==count-1,2);
  accountFooter("LEFT / RIGHT CHANGE  A SELECT","B CANCEL");
 }
}
static unsigned communityDaysInMonth(unsigned year,unsigned month){static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};return month==2&&(!(year%4)&&(year%100||!(year%400)))?29:days[month-1];}
static unsigned communityWeekday(unsigned y,unsigned m,unsigned d){static const unsigned t[]={0,3,2,5,0,3,5,1,4,6,2,4};if(m<3)--y;return (y+y/4-y/100+y/400+t[m-1]+d+6)%7;}
static bool communityDate(char out[17],const char*title,bool manual){
 unsigned y=0,m=0,d=0,h=0,minute=0;if(sscanf(out,"%u-%u-%u %u:%u",&y,&m,&d,&h,&minute)!=5||m<1||m>12||y<2020||y>2199||d<1||d>communityDaysInMonth(y,m)||h>23||minute>59)return false;
 unsigned selected=0,count=communityTwelve?7:6;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return false;selected=menuMove(selected,count,p);
  if(p&(1u|0x40u|0x80u)){int step=p&0x40?-1:1;
   if(selected==0)y=step>0?(y<2199?y+1:y):(y>2020?y-1:y);else if(selected==1)m=(m+11+step)%12+1;
   else if(selected==2)d=(d+communityDaysInMonth(y,m)-1+step)%communityDaysInMonth(y,m)+1;
   else if(selected==3)h=communityTwelve?(h/12)*12+(h%12+12+step)%12:(h+24+step)%24;
   else if(selected==4)minute=(minute+60+step)%60;else if(communityTwelve&&selected==5)h=(h+12)%24;
   else if(p&1){snprintf(out,17,"%04u-%02u-%02u %02u:%02u",y,m,d,h,minute);return true;}
   if(d>communityDaysInMonth(y,m))d=communityDaysInMonth(y,m);
  }
  accountFrame(title,manual?"SET YOUR CURRENT LOCAL DATE AND TIME":communityTwelve?"12-HOUR TIME - SELECT AM OR PM":"24-HOUR TIME - SELECT DONE WHEN READY");
  const char*labels[]={"YEAR","MONTH","DAY","HOUR","MINUTE","AM / PM","DONE"};unsigned vals[]={y,m,d,communityTwelve?(h%12?h%12:12):h,minute};
  for(unsigned r=0;r<count;r++){char s[48];if(r<5)snprintf(s,sizeof s,"%-8s %02u",labels[r],vals[r]);else if(communityTwelve&&r==5)snprintf(s,sizeof s,"AM / PM  %s",h<12?"AM":"PM");else strcpy(s,"DONE");menuRow(160+r*31,s,r==selected,2);}
  accountFooter("LEFT / RIGHT CHANGE  A SELECT",manual?"MANUAL TIME DOES NOT ADJUST FOR DST":"B CANCEL - NO DEVICE CLOCK USED");
 }
}
static bool communitySetManualTime(void){
 char local[17]={0};memcpy(local,communitySelf.now+4,10);local[10]=' ';memcpy(local+11,communitySelf.now+15,5);
 if(!communityDate(local,"SET CURRENT DATE / TIME",true))return false;
 uint8_t data[17]={0};memcpy(data,local,16);
 if(!communityCall(11,data,sizeof data,"SAVING MANUAL LOCAL TIME")||!communityParseProfile(&communitySelf))return false;
 ownProfile=communitySelf.stats;strcpy(communityOwner,ownProfile.name);communityReady=ta_community_configured_zone(communitySelf.zone);
 strcpy(accountNotice,"MANUAL TIME SAVED - UPDATE FOR DST");return communityReady;
}
static bool communityTimeSetup(bool required){
 if(!required&&!communityLoadSelf())return false;unsigned selected=0;TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned p=menuRead(&pad);if(communityPump())return false;if(p&0x100)return communityReady;selected=menuMove(selected,4,p);
  if(p&1){if(selected==3)return communityReady;
   if(selected==0){char z[49]={0};if(communityChooseZone(z,required)){TaCommunityProfile draft=communitySelf;strcpy(draft.zone,z);if(communitySaveProfile(&draft)&&required)return true;}}
   else if(selected==1){if(communitySetManualTime()&&required)return true;}
   else{uint8_t value=communityTwelve?0:1;if(communityCall(12,&value,1,"SAVING TIME FORMAT"))communityLoadSelf();}
   menuBegin(&pad);
  }
  accountFrame("EVENT TIME SETUP","SELECT LOCATION FOR A TIME ZONE\nSO EVENTS TIMES MATCH.");
  menuRow(174,"SELECT LOCATION (AUTOMATIC)",selected==0,2);menuRow(224,"SET DATE / TIME MANUALLY",selected==1,2);
  menuRow(274,communityTwelve?"TIME FORMAT  12-HOUR (AM / PM)":"TIME FORMAT  24-HOUR",selected==2,2);menuRow(324,"BACK",selected==3,2);
  communityText(42,365,accountNotice,uiMuted);
  accountFooter("A SELECT  B BACK",ta_community_manual_zone(communitySelf.zone)?"MANUAL TIME - UPDATE WHEN CLOCKS CHANGE":"LOCATION TIME ADJUSTS AUTOMATICALLY");
 }
}

static void communityDrawProfile(const TaCommunityProfile*p,unsigned page){
 char s[80];if(!page){snprintf(s,sizeof s,"1V1 %s  %lu",ta_account_tier(p->stats.rating),(unsigned long)p->stats.rating);ta_menu_text(screen,42,162,s,uiGold,2);
  snprintf(s,sizeof s,"WINS %lu   LOSSES %lu",(unsigned long)p->stats.wins,(unsigned long)p->stats.losses);ta_menu_text(screen,42,189,s,uiText,2);
  snprintf(s,sizeof s,"RANK %lu   STREAK %ld",(unsigned long)p->stats.rank,(long)p->stats.streak);communityText(42,217,s,uiMuted);
  snprintf(s,sizeof s,"BEST STREAK %lu   MATCHES %lu",(unsigned long)p->stats.best,(unsigned long)(p->stats.wins+p->stats.losses+p->stats.draws));communityText(42,236,s,uiMuted);
  communityText(42,263,ta_community_manual_zone(p->zone)?"MANUAL LOCAL TIME - UPDATE FOR DST":"TIME ZONE - AUTOMATIC LOCAL TIME",uiMuted);communityText(42,280,p->zone[0]?communityLocationName(p->zone):"NOT SET",uiText);
  communityText(42,307,"DISCORD USERNAME",uiMuted);communityText(42,324,p->discord[0]?p->discord:"NOT SHARED",0x6bb8ff);
  unsigned next=ta_rank_next(p->stats.rating);if(next)snprintf(s,sizeof s,"%s IN %lu RATING",ta_rank_name(next),(unsigned long)(next-p->stats.rating));else strcpy(s,"MASTER EGG - HIGHEST DIVISION");
  communityText(42,344,s,uiGold);accountRect(42,359,422,3,0x273852);accountRect(42,359,422*ta_rank_progress(p->stats.rating)/100,3,0x7dd9e8);communityText(42,370,"RIGHT / X: VIEW UPCOMING PLAY TIMES",uiMuted);
 }else{communityText(42,160,"UPCOMING USUAL ONLINE TIMES (YOUR ZONE)",uiMuted);if(!p->count)communityText(42,200,"NO AVAILABILITY SHARED",uiText);for(unsigned i=0;i<p->count;i++)communityText(42,188+i*25,p->windows[i],uiText);communityText(42,370,"TIMES INCLUDE UTC OFFSETS FOR DST CHANGES",uiMuted);}
}
static void communityProfileView(const char*name){
 uint8_t data[16]={0};memcpy(data,name,strlen(name)<16?strlen(name):15);if(!communityCall(3,data,16,"LOADING PLAYER PROFILE"))return;TaCommunityProfile profile;if(!communityParseProfile(&profile))return;
 unsigned page=0;TaMenuPad pad;menuBegin(&pad);for(;;){unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return;if(p&(2u|0xc0u|1u))page^=1;
  accountFrame(profile.stats.name,page?"USUAL ONLINE TIMES - CONVERTED FOR YOU":"PUBLIC PLAYER PROFILE");communityDrawProfile(&profile,page);accountFooter("A / X SWITCH PAGE  B BACK",communityLocationName(communitySelf.zone));
 }
}
static void communityAvailability(void){
 TaCommunityProfile draft=communitySelf;unsigned selected=0;TaMenuPad pad;menuBegin(&pad);for(;;){unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return;selected=menuMove(selected,5,p);
  if(p&1){if(selected==0)communityDays(&draft.days);else if(selected==1)communityTime("USUALLY ONLINE FROM",&draft.start);else if(selected==2)communityTime("USUALLY ONLINE UNTIL",&draft.finish);else if(selected==3){if(communitySaveProfile(&draft))return;}else return;menuBegin(&pad);}
  accountFrame("USUAL ONLINE TIMES",communityLocationName(draft.zone));char s[80];snprintf(s,sizeof s,"DAYS  %s%s%s%s%s%s%s",draft.days&1?"MO ":"",draft.days&2?"TU ":"",draft.days&4?"WE ":"",draft.days&8?"TH ":"",draft.days&16?"FR ":"",draft.days&32?"SA ":"",draft.days&64?"SU ":"");menuRow(160,s,selected==0,2);char clock[16];ta_community_format_clock(clock,sizeof clock,draft.start,communityTwelve);snprintf(s,sizeof s,"FROM  %s",clock);menuRow(203,s,selected==1,2);ta_community_format_clock(clock,sizeof clock,draft.finish,communityTwelve);snprintf(s,sizeof s,"UNTIL %s",clock);menuRow(246,s,selected==2,2);menuRow(289,"SAVE AVAILABILITY",selected==3,2);menuRow(332,"CANCEL",selected==4,2);accountFooter("A CHANGE / SAVE  B CANCEL","UNTIL EARLIER THAN FROM = NEXT DAY");
 }
}
static void communityMyProfileMenu(void){
 if(!accountEnsure())return;unsigned selected=0;TaMenuPad pad;menuBegin(&pad);for(;;){unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return;selected=menuMove(selected,7,p);
  if(p&1){if(selected==6)return;if(selected==0)communityProfileView(ownProfile.name);else if(selected==1)communityTimeSetup(false);
   else if(selected==2)communityAvailability();else if(selected==3){char value[49]={0};if(communityKeyboard("DISCORD USERNAME",value,32)){TaCommunityProfile draft=communitySelf;strcpy(draft.discord,value);communitySaveProfile(&draft);}}
   else if(selected==4){TaCommunityProfile draft=communitySelf;draft.discord[0]=0;communitySaveProfile(&draft);}else{accountProfileMenu();if(!communityEnsure())return;}menuBegin(&pad);
  }
  const char*items[]={"VIEW MY PUBLIC PROFILE","TIME SETUP / 12-24 HOUR FORMAT","USUAL ONLINE DAYS / TIMES","DISCORD USERNAME","CLEAR DISCORD USERNAME","SWITCH / MANAGE PROFILES","BACK"};accountFrame("MY PROFILE",accountNotice);unsigned first=selected>5?selected-5:0;for(unsigned r=0;r<6&&first+r<7;r++)menuRow(160+r*36,items[first+r],first+r==selected,2);accountFooter("A SELECT  B BACK",communityLocationName(communitySelf.zone));
 }
}
static void communityPlayers(void){
 if(!accountEnsure())return;unsigned selected=0,offset=~0u,total=0,count=0;char names[20][17],prefix[16]={0};TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return;
  if(p&2){char value[49]={0};if(communityKeyboard("FIND PLAYER - NAME PREFIX",value,15)){for(unsigned i=0;value[i];i++)if(value[i]>='a'&&value[i]<='z')value[i]-=32;strcpy(prefix,value);selected=0;offset=~0u;}menuBegin(&pad);}
  selected=menuMove(selected,total?total:1,p);unsigned page=selected/20*20;
  if(offset!=page){uint8_t req[18]={0};memcpy(req,prefix,strlen(prefix));ta_community_w16(req+16,page);if(!communityCall(9,req,18,"LOADING PLAYERS"))return;const uint8_t*b=accountLink.reply+2;unsigned n=accountLink.replyBytes-2;if(n<3||b[2]>20||n!=3u+b[2]*16u)return;total=ta_community_r16(b);count=b[2];offset=page;for(unsigned i=0;i<count;i++)if(!ta_community_string(names[i],b+3+i*16,16))return;menuBegin(&pad);}
  if((p&1)&&selected>=offset&&selected-offset<count){communityProfileView(names[selected-offset]);menuBegin(&pad);}
  accountFrame("PLAYER PROFILES",prefix[0]?prefix:"PUBLIC PROFILES - LOCALIZED AVAILABILITY");unsigned first=selected>5?selected-5:0;if(first<offset)first=offset;for(unsigned r=0;r<6&&first+r<offset+count;r++)menuRow(160+r*36,names[first+r-offset],first+r==selected,2);if(!total)ta_menu_text(screen,42,186,"NO PLAYERS FOUND",uiMuted,2);communityBar(selected,1,total,158,224);accountFooter("A PROFILE  X FIND  B BACK",NULL);
 }
}
static bool communityConfirm(const char*title,const char*detail){
 unsigned selected=1;TaMenuPad pad;menuBegin(&pad);for(;;){unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return false;selected=menuMove(selected,2,p);if(p&1)return selected==0;accountFrame(title,detail);menuRow(200,"YES",selected==0,2);menuRow(260,"CANCEL",selected==1,2);accountFooter("A SELECT  B CANCEL",NULL);}
}
static bool communityEditEvent(uint32_t id,const char*oldTitle,const char*oldZone,const char*oldAnchor,unsigned oldDays){
 char title[49]={0},z[49]={0},anchor[17]={0};unsigned days=oldDays;if(oldTitle)strcpy(title,oldTitle);strcpy(z,oldZone?oldZone:communitySelf.zone);
 if(oldAnchor)strcpy(anchor,oldAnchor);else{memcpy(anchor,communitySelf.now+4,10);anchor[10]=' ';memcpy(anchor+11,communitySelf.now+15,5);anchor[16]=0;unsigned yy,mm,dd,hh,mi;if(sscanf(anchor,"%u-%u-%u %u:%u",&yy,&mm,&dd,&hh,&mi)==5){if(++hh==24){hh=0;if(++dd>communityDaysInMonth(yy,mm)){dd=1;if(++mm>12){mm=1;++yy;}}}snprintf(anchor,sizeof anchor,"%04u-%02u-%02u %02u:%02u",yy,mm,dd,hh,mi);}}
 unsigned selected=0;TaMenuPad pad;menuBegin(&pad);for(;;){unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return false;selected=menuMove(selected,5,p);
  if(p&1){if(selected==0){char value[49]={0};if(communityKeyboard("EVENT TITLE",value,32))strcpy(title,value);}else if(selected==1)communityDate(anchor);else if(selected==2)communityDays(&days);else if(selected==4)return false;
   else if(title[0]){unsigned yy,mm,dd,hh,mi;if(sscanf(anchor,"%u-%u-%u %u:%u",&yy,&mm,&dd,&hh,&mi)!=5)return false;if(days&&!(days&(1u<<communityWeekday(yy,mm,dd))))strcpy(accountNotice,"FIRST DATE MUST BE A SELECTED REPEAT DAY");else{
     uint8_t data[103]={0};ta_community_w32(data,id);memcpy(data+4,title,strlen(title));memcpy(data+37,z,strlen(z));memcpy(data+85,anchor,16);data[102]=(uint8_t)days;if(communityCall(6,data,103,"SAVING EVENT")){strcpy(accountNotice,"EVENT SAVED");return true;}}
   }else strcpy(accountNotice,"ADD AN EVENT TITLE FIRST");menuBegin(&pad);
  }
  accountFrame(id?"EDIT EVENT":"CREATE EVENT",communityLocationName(z));char s[80];snprintf(s,sizeof s,"TITLE  %.32s",title[0]?title:"ADD TITLE");menuRow(160,s,selected==0,2);char shown[32];ta_community_format_datetime(shown,sizeof shown,anchor,communityTwelve);snprintf(s,sizeof s,"WHEN   %s",shown);menuRow(203,s,selected==1,2);snprintf(s,sizeof s,"REPEAT %s%s%s%s%s%s%s%s",days?"":"ONCE",days&1?"MO ":"",days&2?"TU ":"",days&4?"WE ":"",days&8?"TH ":"",days&16?"FR ":"",days&32?"SA ":"",days&64?"SU ":"");menuRow(246,s,selected==2,2);menuRow(289,"SAVE EVENT",selected==3,2);menuRow(332,"CANCEL",selected==4,2);communityText(42,367,"DST: FIRST REPEATED HOUR; MISSING TIMES SKIP",uiMuted);accountFooter("A EDIT / SAVE  B CANCEL",accountNotice);
 }
}
static void communityEventPeople(uint32_t id){
 unsigned selected=0,offset=~0u,total=0,count=0;char names[12][17];TaMenuPad pad;menuBegin(&pad);
 for(;;){unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return;selected=menuMove(selected,total?total:1,p);unsigned page=selected/12*12;
  if(offset!=page){uint8_t req[6];ta_community_w32(req,id);ta_community_w16(req+4,page);if(!communityCall(5,req,6,"LOADING INTERESTED PLAYERS"))return;const uint8_t*b=accountLink.reply+2;unsigned n=accountLink.replyBytes-2;if(n<165||b[164]>12||n!=165u+b[164]*16u)return;total=ta_community_r16(b+162);count=b[164];offset=page;for(unsigned i=0;i<count;i++)if(!ta_community_string(names[i],b+165+i*16,16))return;menuBegin(&pad);}
  if((p&1)&&selected>=offset&&selected-offset<count){communityProfileView(names[selected-offset]);menuBegin(&pad);}accountFrame("INTERESTED PLAYERS","SELECT A PLAYER TO VIEW THEIR PROFILE");unsigned first=selected>5?selected-5:0;if(first<offset)first=offset;for(unsigned r=0;r<6&&first+r<offset+count;r++)menuRow(160+r*36,names[first+r-offset],first+r==selected,2);if(!total)communityText(42,190,"NO INTEREST REGISTERED YET",uiMuted);communityBar(selected,1,total,158,224);accountFooter("A PROFILE  B BACK",NULL);
 }
}
static void communityEventView(uint32_t id){
 unsigned selected=0;bool reload=true;TaCommunityEvent event;char z[49],anchor[18];TaMenuPad pad;menuBegin(&pad);
 for(;;){if(reload){uint8_t req[6]={0};ta_community_w32(req,id);if(!communityCall(5,req,6,"LOADING EVENT"))return;const uint8_t*b=accountLink.reply+2;unsigned n=accountLink.replyBytes-2;
   if(n<165||b[164]>12||n!=165u+b[164]*16u||!ta_community_event(&event,b,97)||!ta_community_string(z,b+97,48)||!ta_community_string(anchor,b+145,17)||strlen(anchor)!=16)return;reload=false;menuBegin(&pad);}
  unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return;bool owner=!strcmp(event.owner,ownProfile.name);unsigned choices=owner?6:4;selected=menuMove(selected,choices,p);
  if(p&1){if(selected==0){uint8_t req[5];ta_community_w32(req,id);req[4]=event.mine?0:1;if(communityCall(8,req,5,"UPDATING INTEREST"))reload=true;}
   else if(selected==1)communityEventPeople(id);else if(selected==2)communityProfileView(event.owner);else if(!owner||selected==5)return;else if(selected==3){if(communityEditEvent(id,event.title,z,anchor,event.days))reload=true;}else if(communityConfirm("CANCEL EVENT?",event.title)){uint8_t req[4];ta_community_w32(req,id);if(communityCall(7,req,4,"CANCELLING EVENT"))return;}menuBegin(&pad);}
  accountFrame("EVENT",event.title);communityText(42,158,event.local,uiText);char s[80];snprintf(s,sizeof s,"%s  %u INTERESTED",event.days?"RECURRING SERIES":"ONE-TIME EVENT",event.interested);communityText(42,177,s,uiMuted);
  const char*items[6]={event.mine?"REMOVE MY INTEREST":"I AM INTERESTED","INTERESTED PLAYERS","VIEW ORGANIZER",owner?"EDIT EVENT":"BACK","CANCEL EVENT","BACK"};for(unsigned r=0;r<choices;r++)menuRow(207+r*31,items[r],r==selected,2);accountFooter("A SELECT  B BACK",communityLocationName(communitySelf.zone));
 }
}
static void communityEvents(void){
 if(!accountEnsure()||!communityLoadSelf())return;unsigned selected=0,offset=~0u,total=0,count=0;TaCommunityEvent entries[5];TaMenuPad pad;menuBegin(&pad);uint32_t refreshed=0;
 for(;;){unsigned p=menuRead(&pad);if(communityPump()||p&0x100)return;
  if(p&2){communityEditEvent(0,NULL,NULL,NULL,0);offset=~0u;menuBegin(&pad);}if(p&0x200)offset=~0u;
  selected=menuMove(selected,total?total:1,p);unsigned page=selected/5*5;
  if(offset!=page||(uint32_t)(GetTickCount()-refreshed)>60000u){uint8_t req[2];ta_community_w16(req,page);if(!communityCall(4,req,2,"LOADING EVENTS"))return;const uint8_t*b=accountLink.reply+2;unsigned n=accountLink.replyBytes-2;if(n<11||b[10]>5||n!=11u+b[10]*97u)return;
   ta_community_clock_set(&communityClock,ta_community_r64(b),GetTickCount());total=ta_community_r16(b+8);count=b[10];offset=page;for(unsigned i=0;i<count;i++)if(!ta_community_event(&entries[i],b+11+i*97,97))return;if(selected>=total)selected=total?total-1:0;refreshed=GetTickCount();menuBegin(&pad);}
  if((p&1)&&selected>=offset&&selected-offset<count){communityEventView(entries[selected-offset].id);offset=~0u;menuBegin(&pad);}
  accountFrame("EVENTS",communityLocationName(communitySelf.zone));if(!total)communityText(42,180,"NO UPCOMING EVENTS - X CREATES AN EVENT",uiMuted);
  for(unsigned r=0;r<count;r++){unsigned y=160+r*44,i=offset+r;/* Three 10-pixel lines at 14-pixel intervals, with two pixels of padding. */menuTouchChoice(36,(int)y-2,428,42,r,selected==i);if(selected==i)accountHighlight(36,(int)y-2,428,42);communityText(42,y,entries[r].title,selected==i?uiGold:uiText);communityText(42,y+14,entries[r].local,uiText);char s[64];snprintf(s,sizeof s,"%.15s - %u INTERESTED%s",entries[r].owner,entries[r].interested,entries[r].days?" - REPEATS":"");communityText(42,y+28,s,uiMuted);}
  communityBar(offset,count,total,158,224);accountFooter("A OPEN  X NEW  Y REFRESH  B BACK","ALL TIMES IN YOUR PROFILE TIME ZONE");
 }
}
#endif
