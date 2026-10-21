/* R51: exact-ROM game blocks; opcode observers retain the original executor. */
#ifndef TA_GAME_BLOCKS_H
#define TA_GAME_BLOCKS_H
#include "ta_game_service.h"
#include "ta_timer_bus.h"
#if TA_R48_SERVICE_ON && TA_R49_TIMER_ON && TA_R40_DECODED_DISPATCH && defined(TA_R51_GAME_BLOCKS) && TA_R51_GAME_BLOCKS
#define TA_R51_BLOCKS_ON 1
unsigned ta_native_r51_game_block(Cpu* cpu,unsigned opcodeBudget);
#if defined(TA_R55_HOST_COVERAGE) && TA_R55_HOST_COVERAGE
extern uint64_t ta_r55HostChainedCalls,ta_r55HostExtraBlocks;
#endif
#if defined(TA_R51_HOST_COVERAGE) && TA_R51_HOST_COVERAGE
extern uint64_t ta_r51HostBlockHits[3];
#endif
#else
#define TA_R51_BLOCKS_ON 0
#endif
#endif
