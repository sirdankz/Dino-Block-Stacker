/* R58: exact-ROM common no-rollover counter path at $84:8172. */
#ifndef TA_COUNTER_BLOCK_H
#define TA_COUNTER_BLOCK_H
#include <stdint.h>
#include "ta_accumulate_loop.h"
#if TA_R52_LOOP_ON && TA_R51_BLOCKS_ON && defined(TA_R58_COUNTER_BLOCK) && TA_R58_COUNTER_BLOCK
#define TA_R58_COUNTER_ON 1
#ifdef __cplusplus
extern "C" {
#endif
unsigned ta_native_r58_counter_block(Cpu* cpu, unsigned opcodeBudget);
#if defined(TA_R58_HOST_COVERAGE) && TA_R58_HOST_COVERAGE
extern uint64_t ta_r58HostCounterHits[2];
#endif
#ifdef __cplusplus
}
#endif
#else
#define TA_R58_COUNTER_ON 0
#endif
#endif
