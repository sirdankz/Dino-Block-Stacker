#ifndef TA_VS_BOARD_RNG_H
#define TA_VS_BOARD_RNG_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native state ownership; original 65816 draw instructions still execute.
 * Explicit online policy: two independent streams, not the offline shared
 * sequence. Caller must verify ROM, classify board scope and exclude nesting.
 * No controller synchronization, CPU register changes or guest cycle changes. */
typedef struct {uint16_t index,bias;} TaVsRngState;
typedef struct {
 TaVsRngState board[2],saved;
 uint32_t draws[2];
 uint16_t scopeStack;
 uint8_t active,owner,seeded,phase;
} TaVsBoardRng;
enum {TA_VS_RNG_OK=0,TA_VS_RNG_ARGUMENT=-1,TA_VS_RNG_BUSY=-2,TA_VS_RNG_INACTIVE=-3};
/* Zero-initialize a context before its first seed. Reseeding an active scope
 * is refused, preserving its original global-state restoration obligation. */
int ta_vs_rng_seed(TaVsBoardRng*,const TaVsRngState seed[2]);
int ta_vs_rng_read(const uint8_t*wram,size_t,TaVsRngState*);
int ta_vs_rng_enter(TaVsBoardRng*,unsigned board,uint8_t*wram,size_t);
int ta_vs_rng_leave(TaVsBoardRng*,uint8_t*wram,size_t);
/* Native binary-mode equivalent of8481D6. ROM table is borrowed from a
 * caller-verified1MiB ROM, never copied into the source distribution.
 * Does not translate CPU flags, stack operations, decimal arithmetic or cycles.
 * Caller falls back to original instructions for decimal-mode execution. */
/* Pre-opcode ownership boundary for the verified R129 game path. Caller must
 * visit both interpreter/native execution and skip pending interrupt dispatch.
 * Seed explicitly at agreed match setup; phase2 means shared/non-board work.
 * Unsupported width/bank/stack or overlapping scope is an explicit error.
 * Does not advance a PC, change a register, or skip an original instruction. */
int ta_vs_rng_opcode(TaVsBoardRng*,uint32_t pc,uint16_t stack,unsigned dataBank,
                    unsigned widthFlags,uint8_t*wram,size_t);
int ta_vs_rng_next(const uint8_t*rom,size_t,TaVsRngState*,uint8_t*value);
#ifdef __cplusplus
}
#endif
#endif
