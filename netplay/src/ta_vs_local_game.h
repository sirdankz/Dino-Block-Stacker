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
 uint64_t lastClock;uint32_t lastPc,seed,ticks,sent,received;int error;uint8_t active;
} TaVsLocalGame;
void ta_vs_local_init(TaVsLocalGame*,Snes*,TaVsSession*,uint32_t seed);
void ta_vs_local_bind(TaVsLocalGame*);
void ta_vs_game_trace(Cpu*);
int ta_vs_local_receive(TaVsLocalGame*);
uint16_t ta_vs_setup_buttons(unsigned frame,unsigned player);
#define TA_VS_SETUP_FRAMES 2620u
#ifdef __cplusplus
}
#endif
#endif
