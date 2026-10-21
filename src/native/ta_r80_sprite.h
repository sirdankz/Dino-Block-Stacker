/* R80: Mode1 sprite flip, clipping, bitmap and attribute operations. */
#ifndef TA_R80_SPRITE_H
#define TA_R80_SPRITE_H
#include "ta_r77_dispatch.h"
#ifndef TA_R80_NATIVE_SPRITE
#define TA_R80_NATIVE_SPRITE 0
#endif
#ifndef TA_R80_COVERAGE
#define TA_R80_COVERAGE 0
#endif
#if TA_R77_ENTRY_FIX_ON && TA_R80_NATIVE_SPRITE
#define TA_R80_SPRITE_ON 1
#include "ta_bus_end_limit.h"
static inline __attribute__((always_inline)) bool ta_r80_can_start(const Cpu* c,unsigned budget){
 if(!c||!c->mem||budget<3)return false;
 const Snes* s=(const Snes*)c->mem;unsigned h=s->hPos;
 return !(h&1u)&&h<1358u&&ta_r41_busEndLimit[h]>=h+(s->fastMem?70u:86u);
}
unsigned ta_native_r80_sprite(Cpu* cpu,unsigned budget);
#if TA_R80_COVERAGE
void ta_native_r80_counts(uint32_t stages[8],uint32_t* calls,uint64_t* ops);
#endif
#else
#define TA_R80_SPRITE_ON 0
#endif
#endif
