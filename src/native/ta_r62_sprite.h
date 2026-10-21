/* R62: observed ROM sprite-record arithmetic, split at event fences. */
#ifndef TA_R62_SPRITE_H
#define TA_R62_SPRITE_H
#include "ta_r61_workers.h"
#include "ta_timer_bus.h"
#ifndef TA_R62_NATIVE_SPRITE
#define TA_R62_NATIVE_SPRITE 0
#endif
#ifndef TA_R62_COVERAGE
#define TA_R62_COVERAGE 0
#endif
#if TA_R48_SERVICE_ON && TA_R49_TIMER_ON && TA_R52_ACCUMULATE_LOOP && TA_R62_NATIVE_SPRITE && !TA_R60_DISPATCH_TRACE
#define TA_R62_SPRITE_ON 1
unsigned ta_native_r62_sprite(Cpu* cpu,unsigned budget);
#if TA_R62_COVERAGE
void ta_native_r62_counts(unsigned block,uint32_t* attempts,uint32_t* hits);
#endif
#else
#define TA_R62_SPRITE_ON 0
#endif
#endif
