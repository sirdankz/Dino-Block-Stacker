/* R52: exact-ROM native RAM accumulation loop; observers retain single ops. */
#ifndef TA_ACCUMULATE_LOOP_H
#define TA_ACCUMULATE_LOOP_H
#include "ta_game_blocks.h"
#if TA_R48_SERVICE_ON && TA_R49_TIMER_ON && TA_R40_DECODED_DISPATCH && TA_R23_AE_LOOP && defined(TA_R52_ACCUMULATE_LOOP) && TA_R52_ACCUMULATE_LOOP
#define TA_R52_LOOP_ON 1
unsigned ta_native_r52_accumulate(Cpu* cpu,unsigned opcodeBudget);
#if defined(TA_R53_XBOX_COUNTERS) && TA_R53_XBOX_COUNTERS
void ta_native_r53_loop_counts(uint32_t *attempts,uint32_t *accepts,uint32_t *iterations);
#endif
#if defined(TA_R52_HOST_COVERAGE) && TA_R52_HOST_COVERAGE
extern uint64_t ta_r52HostLoopCalls,ta_r52HostLoopIterations;
#endif
#else
#define TA_R52_LOOP_ON 0
#endif
#endif
