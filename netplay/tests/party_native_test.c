#include "ta_vs_party.h"
#include "ta_vs_local_game.h"
#include "ta_vs_party_view.h"
#include "ta_vs_party_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#define API __declspec(dllexport)
#else
#define API __attribute__((visibility("default")))
#endif
static TaVsParty p[5];static TaVsLocalGame g[5];static Snes machine[5];static Cpu cpu[5];
static TaVsPictureTx tx;static TaVsPictureRx rx[4];static uint32_t view[512*224];
static int receive_attacks(void*c,uint8_t*w,size_t n){return ta_vs_party_deliver((TaVsParty*)c,w,n);}
API void test_init(unsigned i){TaVsIdentity id;memset(&id,0,sizeof id);memset(id.rom,0x57,32);memset(id.rules,0x39,32);id.match=1;char name[9];snprintf(name,sizeof name,"PLAYER%u",i+1);ta_vs_party_init(p+i,&id,100+i,name,0);memset(machine+i,0,sizeof machine[i]);memset(cpu+i,0,sizeof cpu[i]);machine[i].cpu=cpu+i;cpu[i].k=0x82;cpu[i].pc=0x9d59;machine[i].ram[0x2a8]=2;ta_vs_local_init(g+i,machine+i,NULL,0);g[i].rng.seeded=g[i].rows.seeded=1;g[i].sendContext=p+i;g[i].selectTarget=ta_vs_party_target;g[i].sendAttack=ta_vs_party_attack_to;g[i].receiveAttacks=receive_attacks;}
API void test_pair_sender(unsigned i,unsigned peer){g[i].session=p[i].peer+peer;g[i].sendAttack=NULL;g[i].selectTarget=NULL;}
API unsigned test_packet(unsigned i,unsigned now,uint8_t*out){return (unsigned)ta_vs_party_packet(p+i,now,out,TA_PARTY_BYTES);}
API int test_rx(unsigned i,unsigned now,const uint8_t*b,unsigned n){return ta_vs_party_receive(p+i,now,b,n);}
API void test_host(unsigned i,unsigned mode){ta_vs_party_host(p+i,5,mode);}
API void test_join(unsigned i,unsigned room){ta_vs_party_join(p+i,room);}
API unsigned test_get(unsigned i,unsigned what){switch(what){case 0:return p[i].room;case 1:return p[i].self;case 2:return p[i].phase;case 3:return p[i].members;case 4:return p[i].ready;case 5:return p[i].alive;case 6:return p[i].target;case 7:return p[i].roomCount;case 8:return p[i].loaded;case 9:return p[i].error;case 10:return p[i].revision;case 11:return p[i].lastServer;case 12:return p[i].haveCookie;case 13:return p[i].epoch;case 14:return p[i].rotation;case 15:return p[i].speed;case 16:return p[i].present;case 17:return p[i].ranked;case 18:return p[i].authenticated;case 19:return p[i].speedRequests[p[i].self];case 20:return p[i].returning;case 21:return p[i].speedRequest;}return 0;}
API unsigned test_room(unsigned i,unsigned row,unsigned field){if(row>=p[i].roomCount)return 0;const TaPartyRoom*r=p[i].rooms+row;return field==0?r->id:field==1?r->count:field==2?r->mode:r->speed;}
API void test_flags(unsigned i,unsigned f){p[i].flags=(uint8_t)f;p[i].requestSent=0;}
API void test_start(unsigned i){p[i].start=1;p[i].requestSent=0;}
API void test_return(unsigned i){ta_vs_party_return(p+i);}
API void test_leave(unsigned i){p[i].leaving=1;}
API void test_focus(unsigned i,int x,int y){ta_vs_party_focus(p+i,x,y);}
API int test_send(unsigned i,unsigned type,unsigned value,unsigned frame){return ta_vs_party_attack(p+i,type,value,frame);}
API unsigned test_pending(unsigned i,unsigned peer){return ta_vs_pending(p[i].peer+peer);}
API int test_links(unsigned i){for(unsigned s=0;s<4;s++)if(s!=p[i].self&&(p[i].members&(1u<<s))&&!ta_vs_ready(p[i].peer+s))return 0;return 1;}
static unsigned addr(unsigned type){static const unsigned a[7]={0x42c,0,0x444,0x44c,0x454,0x45c,0x470};return a[type];}
API unsigned test_word(unsigned i,unsigned type){unsigned a=addr(type);return machine[i].ram[a]|((unsigned)machine[i].ram[a+1]<<8);}
API void test_clear(unsigned i,unsigned type){unsigned a=addr(type);machine[i].ram[a]=machine[i].ram[a+1]=0;}
API void test_mailbox(unsigned i,unsigned type,unsigned n){unsigned a=addr(type)+2;machine[i].ram[a]=(uint8_t)n;machine[i].ram[a+1]=(uint8_t)(n>>8);}
API void test_tick(unsigned i){machine[i].cycles++;machine[i].frames++;ta_vs_local_bind(g+i);ta_vs_game_trace(cpu+i);ta_vs_local_bind(NULL);}
API int test_game_error(unsigned i){return g[i].error;}
API unsigned test_backlog(unsigned i){unsigned n=0;for(unsigned k=0;k<256;k++)n+=g[i].outgoing[k].used;return n;}
API int test_decode(unsigned type,unsigned value){uint8_t ram[131072]={0};TaVsAttack a={0};a.type=(uint16_t)type;a.value=(uint16_t)value;return ta_vs_attack_apply(ram,sizeof ram,0,&a);}
API unsigned test_crop(unsigned crop,uint8_t*out){memset(&tx,0,sizeof tx);tx.fieldOnly=(uint8_t)crop;static uint32_t colors[512*224];for(unsigned j=0;j<512*224;j++)colors[j]=0x123456;ta_vs_picture_prepare(&tx,colors,512,1);unsigned total=0;size_t n;while((n=ta_vs_picture_next(&tx,1,out,1152))!=0)total+=(out[8]<<8)|out[9];return total;}
API uint32_t*test_view(unsigned i){memset(view,0,sizeof view);for(unsigned s=0;s<4;s++)for(unsigned y=0;y<224;y++)for(unsigned x=0;x<224;x++){unsigned cell=((x/32+y/16+s)%5);rx[s].pixels[y*224+x]=(uint32_t[]){0xe86b70,0x6cc68a,0x6e9ee8,0xe8c669,0xa77bd8}[cell];}ta_party_view(view,p+i,rx);return view;}

API void test_layout(unsigned width,unsigned height,unsigned wide,unsigned peers,float*out){TaPartyLayout l=ta_party_layout(width,height,wide,peers);TaPartyRect a[8]={l.local,l.peer[0],l.peer[1],l.peer[2],l.label[0],l.label[1],l.label[2],l.label[3]};for(unsigned i=0;i<8;i++){out[i*4]=a[i].left;out[i*4+1]=a[i].top;out[i*4+2]=a[i].right;out[i*4+3]=a[i].bottom;}}
API uint32_t*test_labels(unsigned i,unsigned suffix){strcpy(p[i].names[0],suffix?"ABCZZZZZ":"ABCDEFGH");for(unsigned k=0;k<512*224;k++)view[k]=0x123456;ta_party_labels(view,p+i);return view;}

API int test_upload_due(unsigned have,unsigned prev,unsigned revision,unsigned now,unsigned last,unsigned interval){return ta_party_upload_due(have,prev,revision,now,last,interval);}
API unsigned test_capture_interval(unsigned players){return ta_party_capture_interval(players);}
API uint32_t*test_roster(unsigned i){for(unsigned k=0;k<512*224;k++)view[k]=0x123456;ta_party_roster(view,p+i);return view;}

API unsigned test_next(unsigned i){return ta_vs_party_next_target(p+i);}

API void test_speed(unsigned i,unsigned speed){ta_vs_party_speed(p+i,speed);}
API void test_auth(unsigned i,const uint8_t*ticket){memcpy(p[i].authTicket,ticket,76);p[i].authState=1;}

API void test_auth_wait(unsigned i){p[i].authState=3;}
