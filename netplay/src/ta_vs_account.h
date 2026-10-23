#ifndef TA_VS_ACCOUNT_H
#define TA_VS_ACCOUNT_H
#include "ta_vs_public.h"
#define TA_ACCOUNT_PACKET 640u
#define TA_ACCOUNT_PROFILE_BYTES 44u
enum {TA_ACCOUNT_CREATE=1,TA_ACCOUNT_LOGIN,TA_ACCOUNT_RESUME,TA_ACCOUNT_PROFILE,TA_ACCOUNT_LADDER,TA_ACCOUNT_ARM,TA_ACCOUNT_REPORT,TA_ACCOUNT_DIRECTORY,TA_ACCOUNT_PEER,TA_ACCOUNT_COUNTS,TA_ACCOUNT_CHAT_POLL,TA_ACCOUNT_CHAT_SEND,TA_ACCOUNT_PARTY_TICKET,TA_ACCOUNT_PARTY_LADDER,TA_ACCOUNT_LOBBY_POLL,TA_ACCOUNT_LOBBY_SEND};
typedef struct {char name[17];uint32_t rating,wins,losses,draws;int32_t streak;uint32_t best,rank;} TaAccountProfile;
typedef struct {
 uint8_t key[32],publicKey[32],serverKey[32],tx[TA_ACCOUNT_PACKET],reply[TA_ACCOUNT_PACKET];
 uint64_t sequence;uint32_t lastSend;unsigned txBytes,replyBytes,pending,sent,ready;
} TaAccountLink;
int ta_account_init(TaAccountLink*,const uint8_t secret[32],const uint8_t serverKey[32]);
int ta_account_request(TaAccountLink*,const TaVsPublic*,unsigned command,const void*,unsigned);
size_t ta_account_packet(TaAccountLink*,uint32_t,const uint8_t**);
int ta_account_receive(TaAccountLink*,const TaVsPublic*,const uint8_t*,size_t);
int ta_account_profile(TaAccountProfile*,const uint8_t*,size_t);
const char* ta_account_tier(uint32_t rating);
/* Standard primitive wrappers shared with the room service. No game hot path. */
int ta_account_key(uint8_t out[32],const uint8_t secret[32],const uint8_t peer[32],const uint8_t client[32],const uint8_t server[32]);
void ta_account_seal(uint8_t*,uint8_t[16],const uint8_t[32],uint64_t,unsigned,const uint8_t*,unsigned,const uint8_t*,unsigned);
int ta_account_open(uint8_t*,const uint8_t[16],const uint8_t[32],uint64_t,unsigned,const uint8_t*,unsigned,const uint8_t*,unsigned);
#endif
