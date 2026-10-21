#ifndef TA_VS_SESSION_H
#define TA_VS_SESSION_H
/* Native, allocation-free two-peer event channel. No controller-input delay,
 * emulator snapshots, rollback, guest writes, or garbage-rule assumptions.
 * Caller pins the transport endpoint and maps verified game events separately. */
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TA_VS_WINDOW 32u
#define TA_VS_EVENT_BYTES 32u
#define TA_VS_SNAPSHOT_BYTES 1152u
#define TA_VS_PACKET_BYTES (32u+TA_VS_SNAPSHOT_BYTES)
#define TA_VS_RETRY_MS 50u
#define TA_VS_HELLO_MS 250u
#define TA_VS_KEEPALIVE_MS 1000u
/* Tags are application boundaries, not a guessed SNES garbage descriptor. */
enum {TA_VS_ATTACK=1,TA_VS_RESULT=2};
typedef enum {
 TA_VS_OK=0,TA_VS_BAD_ARGUMENT=-1,TA_VS_NOT_READY=-2,TA_VS_QUEUE_FULL=-3,
 TA_VS_BAD_PACKET=-4,TA_VS_WRONG_SESSION=-5,TA_VS_IDENTITY_MISMATCH=-6,
 TA_VS_BAD_ACK=-7,TA_VS_OUTSIDE_WINDOW=-8,TA_VS_SEQUENCE_EXHAUSTED=-9,
 TA_VS_CONFLICT=-10
} TaVsResult;
typedef struct {
 uint64_t match;
 uint8_t rom[32],rules[32];
 uint32_t seed;
} TaVsIdentity;
typedef struct {
 uint32_t sequence;
 uint16_t tag,size;
 uint8_t data[TA_VS_EVENT_BYTES];
} TaVsEvent;
typedef struct {TaVsEvent event;uint32_t lastSent;uint8_t valid,sent;} TaVsSlot;
typedef struct {
 TaVsIdentity identity;
 uint32_t txLast,txAck,txHighestSent,rxApplied;
 uint32_t lastHello,lastPacket,lastReceive;
 uint32_t snapshotRevision,snapshotSent,remoteRevision;
 uint16_t snapshotSize,remoteSize;
 uint8_t role,peerHello,helloAcked,helloSent,helloReply,ackPending,everReceived;
 uint8_t snapshot[TA_VS_SNAPSHOT_BYTES],remoteSnapshot[TA_VS_SNAPSHOT_BYTES];
 TaVsSlot tx[TA_VS_WINDOW],rx[TA_VS_WINDOW];
 uint32_t applicationRound;
} TaVsSession;
TaVsResult ta_vs_init(TaVsSession*,const TaVsIdentity*,unsigned role);
int ta_vs_ready(const TaVsSession*);
/* Failure never consumes an application event. Queue full requires explicit
 * match error/backpressure handling; never silently discard an attack. */
TaVsResult ta_vs_enqueue(TaVsSession*,unsigned tag,const void*,size_t,uint32_t*);
TaVsResult ta_vs_set_snapshot(TaVsSession*,const void*,size_t);
/* Build at most one datagram. Retry timers only control lost-packet resends;
 * they do not insert an attack delay. Polling/budgets belong to the frontend. */
size_t ta_vs_packet(TaVsSession*,uint32_t nowMs,void*,size_t);
TaVsResult ta_vs_receive(TaVsSession*,uint32_t nowMs,const void*,size_t);
/* ACK advances only after successful game-side application, not receipt.
 * A failed application may leave the event pending; do not commit it. */
const TaVsEvent* ta_vs_peek(const TaVsSession*);
TaVsResult ta_vs_commit(TaVsSession*,uint32_t sequence);
unsigned ta_vs_pending(const TaVsSession*);
/* Returned view is latest display state only; never apply it as local board
 * authority. Revision zero means no snapshot has arrived. */
const uint8_t* ta_vs_remote_snapshot(const TaVsSession*,size_t*,uint32_t*);
#ifdef __cplusplus
}
#endif
#endif
