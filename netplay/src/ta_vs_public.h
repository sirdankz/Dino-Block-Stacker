#ifndef TA_VS_PUBLIC_H
#define TA_VS_PUBLIC_H
#include "ta_vs_session.h"
#define TA_PUB_PORT 34987u
#define TA_PUB_BYTES (TA_VS_PACKET_BYTES+32u)
enum {TA_PUB_HELLO=1,TA_PUB_COOKIE,TA_PUB_LIST,TA_PUB_ROOMS,TA_PUB_CREATE,TA_PUB_STATE,TA_PUB_JOIN,TA_PUB_KEEP,TA_PUB_LEAVE,TA_PUB_PUNCH,TA_PUB_RELAY,TA_PUB_DATA,TA_PUB_DIRECT,TA_PUB_ERROR,TA_PUB_LEAVE_ACK};
enum {TA_PUB_HOST=0,TA_PUB_BROWSE=1,TA_PUB_JOINING=2};
typedef struct{uint32_t id;uint8_t speed,players;}TaVsRoom;
typedef struct{
 uint32_t serverAddress,localAddress,peerAddress,peerLocalAddress,directAddress,room,joinRoom;
 uint16_t serverPort,localPort,peerPort,peerLocalPort,directPort;
 uint64_t nonce,roomNonce;
 uint8_t cookie[16],key[16],identity[64],mode,role,speed,haveCookie,haveRoom,havePeer,direct,seen,peerSeen,seenLan,peerSeenLan,punchLocal;
 uint32_t lastRequest,lastKeep,lastPunch,lastDirect,lastServer,started;
 uint8_t phase,leaving,leaveAck;uint32_t roundEpoch;
 uint8_t requested,kept,punched,roomCount;TaVsRoom rooms[8];int error;
}TaVsPublic;
uint32_t ta_vs_public_ipv4(const char*);
void ta_vs_public_init(TaVsPublic*,const TaVsIdentity*,uint64_t,uint32_t,uint16_t,uint16_t,uint32_t);
void ta_vs_public_host(TaVsPublic*);
int ta_vs_public_join(TaVsPublic*,uint32_t);
size_t ta_vs_public_packet(TaVsPublic*,uint32_t,uint8_t*,size_t,uint32_t*,uint16_t*);
/* Returns inner gameplay bytes,0 for a handled control, negative for rejection.
   Every public game packet is mapped to the stable logical server endpoint by
   the caller before entering the unchanged LAN/session pinning state machine. */
int ta_vs_public_receive(TaVsPublic*,uint32_t,uint32_t,uint16_t,uint8_t*,size_t);
size_t ta_vs_public_game(TaVsPublic*,uint32_t,const uint8_t*,size_t,uint8_t*,size_t,uint32_t*,uint16_t*);
void ta_vs_public_begin_leave(TaVsPublic*);
size_t ta_vs_public_leave(TaVsPublic*,uint8_t*,size_t);
#endif
