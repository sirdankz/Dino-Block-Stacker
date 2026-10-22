#ifndef TA_VS_PARTY_H
#define TA_VS_PARTY_H
#include "ta_vs_attack.h"
#include "ta_vs_picture.h"
#ifdef __cplusplus
extern "C" {
#endif
#define TA_PARTY_PLAYERS 4u
#define TA_PARTY_PORT 34988u
#define TA_PARTY_BYTES (TA_VS_PACKET_BYTES+33u)
enum {TA_PARTY_HELLO=1,TA_PARTY_COOKIE,TA_PARTY_LIST,TA_PARTY_ROOMS,TA_PARTY_CREATE,TA_PARTY_JOIN,TA_PARTY_KEEP,TA_PARTY_STATE,TA_PARTY_DATA,TA_PARTY_LEAVE,TA_PARTY_ERROR,TA_PARTY_START,TA_PARTY_RETURN,TA_PARTY_SPEED,TA_PARTY_AUTH,TA_PARTY_AUTH_OK};
enum {TA_PARTY_AUTO=0,TA_PARTY_FOCUS=1};
typedef struct {uint32_t id;uint8_t count,speed,mode;char name[9];}TaPartyRoom;
typedef struct {
 TaVsIdentity identity;TaVsSession peer[4];TaVsAttackInbox inbox[4];
 uint64_t nonce,roomNonce;uint32_t room,seed,epoch,revision,lastSend,lastServer,requestRoom;
 uint8_t cookie[16],haveCookie,self,host,speed,mode,members,alive,ready,loaded,phase,flags,request,requestSent,leaving,start,linked,target,stickHeld,rotation,txRotation,rxRotation,returning;
 uint8_t present,speedRequests[4],ranked,authenticated,capability,authState,speedRequest;uint8_t authTicket[76];
 char name[9],names[4][9];TaPartyRoom rooms[8];unsigned roomCount;int error;
}TaVsParty;
void ta_vs_party_init(TaVsParty*,const TaVsIdentity*,uint64_t nonce,const char*name,uint32_t now);
void ta_vs_party_host(TaVsParty*,unsigned speed,unsigned mode);
void ta_vs_party_join(TaVsParty*,uint32_t room);
size_t ta_vs_party_packet(TaVsParty*,uint32_t now,uint8_t*,size_t);
int ta_vs_party_receive(TaVsParty*,uint32_t now,const uint8_t*,size_t);
unsigned ta_vs_party_target(void*);
unsigned ta_vs_party_next_target(const TaVsParty*);
TaVsResult ta_vs_party_attack_to(void*,unsigned,unsigned,uint32_t,unsigned);
TaVsResult ta_vs_party_attack(void*,unsigned type,unsigned value,uint32_t frame);
int ta_vs_party_deliver(TaVsParty*,uint8_t*wram,size_t);
void ta_vs_party_return(TaVsParty*);
void ta_vs_party_speed(TaVsParty*,unsigned speed);
void ta_vs_party_focus(TaVsParty*,int x,int y);
unsigned ta_vs_party_count(unsigned mask);
#ifdef __cplusplus
}
#endif
#endif
