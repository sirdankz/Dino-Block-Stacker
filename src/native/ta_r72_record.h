/* R72: observed $80:BC33 record update; complete event-fenced native block. */
#ifndef TA_R72_RECORD_H
#define TA_R72_RECORD_H
#include "ta_r62_sprite.h"
#ifndef TA_R72_NATIVE_RECORD
#define TA_R72_NATIVE_RECORD 0
#endif
#ifndef TA_R72_COVERAGE
#define TA_R72_COVERAGE 0
#endif
#if TA_R62_SPRITE_ON && TA_R72_NATIVE_RECORD
#define TA_R72_RECORD_ON 1
unsigned ta_native_r72_record(Cpu* cpu,unsigned budget);
#if TA_R72_COVERAGE
void ta_native_r72_counts(uint32_t* attempts,uint32_t* hits);
#endif
#else
#define TA_R72_RECORD_ON 0
#endif
#endif
