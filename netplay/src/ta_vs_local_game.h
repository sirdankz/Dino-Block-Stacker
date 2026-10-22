#ifndef TA_VS_LOCAL_GAME_H
#define TA_VS_LOCAL_GAME_H
#include "ta_vs_attack.h"
#include "ta_vs_board_rng.h"
#include "ta_vs_board_rows.h"
#include "snes.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Online adaptation: every endpoint owns original P0. Original P1 is a static
 * non-authoritative placeholder, never filled from received display data.
 * All CPU instructions and timing guards remain enabled. */
typedef struct {
 Snes*s;TaVsSession*session;TaVsBoardRng rng;TaVsBoardRows rows;TaVsAttackInbox inbox;
 int (*receiveAttacks)(void*,uint8_t*,size_t);
 void*sendContext;
 unsigned (*selectTarget)(void*);
 TaVsResult (*sendAttack)(void*,unsigned,unsigned,uint32_t,unsigned);
 struct {uint32_t order,frame;uint16_t type,value;uint8_t route,used;}outgoing[256];
 uint32_t outgoingOrder;unsigned outgoingCount;
 uint64_t lastClock;uint32_t lastPc,seed,ticks,sent,received;int error;uint8_t active;
} TaVsLocalGame;
void ta_vs_local_init(TaVsLocalGame*,Snes*,TaVsSession*,uint32_t seed);
void ta_vs_local_bind(TaVsLocalGame*);
/* Flush owned attacks without advancing a guest board (e.g. while spectating). */
int ta_vs_local_flush(TaVsLocalGame*);
void ta_vs_game_trace(Cpu*);
int ta_vs_local_receive(TaVsLocalGame*);
uint16_t ta_vs_setup_buttons(unsigned frame,unsigned player);
#define TA_VS_SETUP_FRAMES 2620u
#ifdef __cplusplus
}
#endif
#endif
