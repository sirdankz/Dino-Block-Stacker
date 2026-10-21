/* R58: complete inactive task slots; active callbacks remain original. */
#ifndef TA_IDLE_SLOTS_H
#define TA_IDLE_SLOTS_H
#include "ta_accumulate_loop.h"
#if TA_R52_LOOP_ON && TA_R51_BLOCKS_ON && TA_R47_REJECT_ON && defined(TA_R58_IDLE_SLOTS) && TA_R58_IDLE_SLOTS
#define TA_R58_IDLE_ON 1
unsigned ta_native_r58_idle_slots(Cpu* cpu,unsigned opcodeBudget);
#if defined(TA_R58_HOST_COVERAGE) && TA_R58_HOST_COVERAGE
extern uint64_t ta_r58HostIdleCalls,ta_r58HostIdleSlots;
#endif
#else
#define TA_R58_IDLE_ON 0
#endif
#endif
