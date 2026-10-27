#ifndef TA_VS_COMMUNITY_H
#define TA_VS_COMMUNITY_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
/* Account command 17 is an additive extension; existing profile and match packets stay unchanged. */
#define TA_COMMUNITY_COMMAND 17u
#define TA_COMMUNITY_EVENT_BYTES 97u
#define TA_COMMUNITY_CHAT_VISIBLE 14u
#define TA_COMMUNITY_CHAT_CAPACITY 128u
static uint16_t ta_community_r16(const uint8_t*p){return (uint16_t)(((unsigned)p[0]<<8)|p[1]);}
static uint32_t ta_community_r32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t ta_community_r64(const uint8_t*p){uint64_t n=0;for(unsigned i=0;i<8;i++)n=(n<<8)|p[i];return n;}
static void ta_community_w16(uint8_t*p,unsigned n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void ta_community_w32(uint8_t*p,uint32_t n){for(unsigned i=0;i<4;i++)p[3-i]=(uint8_t)(n>>(8*i));}
static bool ta_community_string(char*out,const uint8_t*p,unsigned n){bool end=false;for(unsigned i=0;i<n;i++){unsigned c=p[i];if(!c)end=true;else if(end||c<32||c>126)return false;out[i]=(char)c;}out[n]=0;return true;}
typedef struct{uint32_t id;uint64_t utc;char title[34],owner[17],local[33];unsigned days,interested,mine;}TaCommunityEvent;
static bool ta_community_event(TaCommunityEvent*out,const uint8_t*p,unsigned n){
 if(n!=97)return false;TaCommunityEvent e;memset(&e,0,sizeof e);e.id=ta_community_r32(p);e.utc=ta_community_r64(p+4);
 if(!e.id||!ta_community_string(e.title,p+12,33)||!ta_community_string(e.owner,p+45,16)||!ta_community_string(e.local,p+61,32)||p[93]>127||p[96]>1)return false;
 e.days=p[93];e.interested=ta_community_r16(p+94);e.mine=p[96];*out=e;return true;
}
/* Profiles store a place's civil-time rules, never a guessed numeric offset. */
static bool ta_community_location_zone(const char*z){
 static const char*const regions[]={"America/","Europe/","Asia/","Africa/","Australia/","Pacific/","Atlantic/","Indian/","Antarctica/","Arctic/"};
 if(!z)return false;for(unsigned i=0;i<sizeof(regions)/sizeof(regions[0]);i++){unsigned n=(unsigned)strlen(regions[i]);if(!strncmp(z,regions[i],n)&&z[n])return true;}return false;
}
static bool ta_community_manual_zone(const char*z){
 if(!z||strncmp(z,"Manual/",7)||strlen(z)!=13||(z[7]!='+'&&z[7]!='-')||z[10]!=':')return false;
 for(unsigned i=8;i<13;i++)if(i!=10&&(z[i]<'0'||z[i]>'9'))return false;
 unsigned h=(unsigned)(z[8]-'0')*10+(unsigned)(z[9]-'0'),m=(unsigned)(z[11]-'0')*10+(unsigned)(z[12]-'0');
 unsigned total=h*60+m;return m<60&&total<=(z[7]=='-'?720u:840u);
}
static bool ta_community_configured_zone(const char*z){return ta_community_location_zone(z)||ta_community_manual_zone(z);}
static void ta_community_format_clock(char*out,unsigned capacity,unsigned minutes,bool twelve){
 unsigned h=minutes/60,m=minutes%60;if(twelve)snprintf(out,capacity,"%u:%02u %s",h%12?h%12:12,m,h<12?"AM":"PM");else snprintf(out,capacity,"%02u:%02u",h,m);
}
static void ta_community_format_datetime(char*out,unsigned capacity,const char*canonical,bool twelve){
 unsigned y,m,d,h,minute;if(sscanf(canonical,"%u-%u-%u %u:%u",&y,&m,&d,&h,&minute)!=5||h>23||minute>59){if(capacity)out[0]=0;return;}
 char clock[16];ta_community_format_clock(clock,sizeof clock,h*60+minute,twelve);snprintf(out,capacity,"%04u-%02u-%02u %s",y,m,d,clock);
}
/* Unsigned deltas tolerate GetTickCount wrap. Wall time is never read from the device. */
typedef struct{uint64_t utc;uint32_t tick;bool known;}TaCommunityClock;
static bool ta_community_clock_set(TaCommunityClock*c,uint64_t utc,uint32_t tick){if(utc<1577836800ull||utc>7258118400ull)return false;c->utc=utc;c->tick=tick;c->known=true;return true;}
static uint64_t ta_community_clock_now(const TaCommunityClock*c,uint32_t tick){return c->known&&(uint32_t)(tick-c->tick)<600000u?c->utc+(uint32_t)(tick-c->tick)/1000u:0;}
#endif
