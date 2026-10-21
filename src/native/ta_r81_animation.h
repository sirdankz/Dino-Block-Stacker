/* R81: guarded animation descriptor setup, callback pointers and byte counters. */
#ifndef TA_R81_ANIMATION_H
#define TA_R81_ANIMATION_H
#include "ta_r77_dispatch.h"
#ifndef TA_R81_NATIVE_ANIMATION
#define TA_R81_NATIVE_ANIMATION 0
#endif
#ifndef TA_R81_COVERAGE
#define TA_R81_COVERAGE 0
#endif
#if TA_R77_ENTRY_FIX_ON && TA_R81_NATIVE_ANIMATION
#define TA_R81_ANIMATION_ON 1
#include "ta_bus_end_limit.h"
/* Read-only lower bounds for the actual entry: avoid full guard work when
 * its shortest valid segment cannot fit. No supported execution is skipped. */
static inline __attribute__((always_inline)) bool ta_r81_can_start(const Cpu* c,unsigned budget){
 if(!c||!c->mem)return false;
 const Snes* s=(const Snes*)c->mem;const unsigned h=s->hPos;if((h&1u)||h>=1358u)return false;
 const unsigned fetch=s->fastMem?6u:8u;unsigned ops,clocks;
 switch(c->pc){
 case 0xa36a:ops=4;clocks=4u*fetch+78u;break;
 case 0xa36e:ops=4;clocks=10u*fetch+46u;break;
 case 0xa371:ops=3;clocks=7u*fetch+46u;break;
 case 0xa37a:case 0xa39e:ops=3;clocks=6u*fetch+38u;break;
 case 0xa383:ops=4;clocks=8u*fetch+62u;break;
 case 0xa38b:ops=7;clocks=11u*fetch+94u;break;
 case 0xa3a4:ops=3;clocks=5u*fetch+44u;break;
 case 0xa3a7:ops=4;clocks=5u*fetch+82u;break;
 case 0xa3a9:ops=3;clocks=3u*fetch+76u;break;
 default:return false;
 }
 return budget>=ops&&ta_r41_busEndLimit[h]>=h+clocks;
}
unsigned ta_native_r81_animation(Cpu* cpu,unsigned budget);
#if TA_R81_COVERAGE
void ta_native_r81_counts(uint32_t stages[8],uint32_t* calls,uint64_t* ops);
#endif
#else
#define TA_R81_ANIMATION_ON 0
#endif
#endif
