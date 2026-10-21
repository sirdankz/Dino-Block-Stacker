/* R74: observed six-field gameplay loop, fixed native predicates and writes. */
#ifndef TA_R74_FIELDS_H
#define TA_R74_FIELDS_H
#include "ta_r73_chain.h"
#ifndef TA_R74_NATIVE_FIELDS
#define TA_R74_NATIVE_FIELDS 0
#endif
#ifndef TA_R74_COVERAGE
#define TA_R74_COVERAGE 0
#endif
#if TA_R73_CHAIN_ON && TA_R74_NATIVE_FIELDS
#define TA_R74_FIELDS_ON 1
unsigned ta_native_r74_fields(Cpu* cpu,unsigned budget);
static inline __attribute__((always_inline)) bool ta_r74_can_start(const Cpu* c,unsigned budget){
 if(!c || !c->mem)return false;const Snes* s=(const Snes*)c->mem;const unsigned h=s->hPos,fetch=s->fastMem?6u:8u;
 const unsigned ops=c->pc==0xd9d4?1u:c->pc==0xdac0?2u:3u;
 const unsigned clocks=c->pc==0xd9d4?2u*fetch+16u:c->pc==0xdac0?5u*fetch+44u:8u*fetch+28u;
 return budget>=ops && !(h&1u) && h<1358u && ta_r41_busEndLimit[h]>=h+clocks;
}
#if TA_R74_COVERAGE
void ta_native_r74_counts(uint32_t paths[4],uint32_t* calls,uint64_t* ops);
#endif
#else
#define TA_R74_FIELDS_ON 0
#endif
#endif
