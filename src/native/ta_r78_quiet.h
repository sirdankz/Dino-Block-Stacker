/* R78: quiet four-field gameplay scan and exact row tail. */
#ifndef TA_R78_QUIET_H
#define TA_R78_QUIET_H
#include "ta_r77_dispatch.h"
#ifndef TA_R78_NATIVE_QUIET
#define TA_R78_NATIVE_QUIET 0
#endif
#ifndef TA_R78_COVERAGE
#define TA_R78_COVERAGE 0
#endif
#if TA_R77_ENTRY_FIX_ON && TA_R78_NATIVE_QUIET
#define TA_R78_QUIET_ON 1
unsigned ta_native_r78_quiet(Cpu* cpu,unsigned budget);
static inline __attribute__((always_inline)) bool ta_r78_can_start(const Cpu* c,unsigned budget){
 if(!c||!c->mem)return false;const Snes* s=(const Snes*)c->mem;unsigned fetch=s->fastMem?6u:8u,h=s->hPos;
 unsigned ops=c->pc==0x9be8?2u:c->pc==0x9b2a?3u:c->pc==0x9b91?6u:5u;
 unsigned clocks=c->pc==0x9be8?5u*fetch+44u:c->pc==0x9b2a?8u*fetch+28u:c->pc==0x9b91?13u*fetch+34u:10u*fetch+40u;
 return budget>=ops&&!(h&1u)&&h<1358u&&ta_r41_busEndLimit[h]>=h+clocks;
}
#if TA_R78_COVERAGE
void ta_native_r78_counts(uint32_t stages[5],uint32_t* calls,uint64_t* ops);
#endif
#else
#define TA_R78_QUIET_ON 0
#endif
#endif
