/* R58: exact-ROM task dispatcher prefix; indirect calls remain original. */
#ifndef TA_TASK_PREFIX_H
#define TA_TASK_PREFIX_H
#include "ta_game_blocks.h"
#if TA_R51_BLOCKS_ON && TA_R47_REJECT_ON && defined(TA_R58_TASK_PREFIX) && TA_R58_TASK_PREFIX
#define TA_R58_TASK_ON 1
unsigned ta_native_r58_task_prefix(Cpu* cpu,unsigned opcodeBudget);
#if defined(TA_R58_HOST_COVERAGE) && TA_R58_HOST_COVERAGE
extern uint64_t ta_r58HostTaskHits[3];
#endif
#else
#define TA_R58_TASK_ON 0
#endif
#endif
