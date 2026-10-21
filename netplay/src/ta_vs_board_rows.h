#ifndef TA_VS_BOARD_ROWS_H
#define TA_VS_BOARD_ROWS_H
#include "ta_vs_board_rng.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Row refill alternation is shared in the original ROM. Its independent online
 * ownership is distinct from RNG ownership. Zero initialize before seeding. */
typedef struct {uint16_t flip[2],saved,stack;uint32_t rows[2];uint8_t seeded,active,owner;} TaVsBoardRows;
int ta_vs_rows_seed(TaVsBoardRows*,const uint16_t seeds[2]);
int ta_vs_rows_opcode(TaVsBoardRows*,unsigned phase,uint32_t pc,uint16_t stack,unsigned db,unsigned widths,uint8_t*wram,size_t);
typedef struct {TaVsRngState next;uint16_t panel,draws;} TaVsPanelPick;
/* Binary-mode panel candidate generator, matching82E98E/82E9B1 up to the
 * graphics callback. Neighbors are RAW16-bit words; do not strip their flags.
 * Returns1 on attempt exhaustion, without committing seed/RAM/output changes.
 * This helper does not translate CPU registers, stack, graphics or timing. */
int ta_vs_panel_pick(const uint8_t*rom,size_t,const TaVsRngState*seed,unsigned colorLimit,unsigned mask,uint16_t above,uint16_t horizontal,unsigned maxAttempts,TaVsPanelPick*out);
#ifdef __cplusplus
}
#endif
#endif
