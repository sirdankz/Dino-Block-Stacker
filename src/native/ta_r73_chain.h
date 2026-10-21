/* R73: observed sprite coordinate/bitmap/record chain, exact event segments. */
#ifndef TA_R73_CHAIN_H
#define TA_R73_CHAIN_H
#include "ta_r62_sprite.h"
#ifndef TA_R73_NATIVE_CHAIN
#define TA_R73_NATIVE_CHAIN 0
#endif
#ifndef TA_R73_COVERAGE
#define TA_R73_COVERAGE 0
#endif
#if TA_R62_SPRITE_ON && TA_R73_NATIVE_CHAIN
#define TA_R73_CHAIN_ON 1
#include "ta_bus_end_limit.h"
/* Lower bound only: omit a call when no segment can possibly fit. */
static inline __attribute__((always_inline)) bool ta_r73_can_start(const Cpu* c,unsigned budget){
#if defined(TA_R101_SPRITE_RESUME) && TA_R101_SPRITE_RESUME
 if(c && c->mem) {
  unsigned ops=0,bytes=0,extra=0;
  switch(c->pc){
   case 0xbc37:ops=4;bytes=13;extra=66;break;
   case 0xbc39:ops=3;bytes=11;extra=50;break;
   case 0xbc3c:ops=2;bytes=8;extra=28;break;
   case 0xbc3f:case 0xbdbc:case 0xbdd3:ops=1;bytes=3;extra=22;break;
   case 0xbc4b:ops=2;bytes=4;extra=38;break;
   case 0xbdba:ops=2;bytes=5;extra=38;break;
   case 0xbdc3:ops=3;bytes=7;extra=32;break;
   case 0xbdc5:ops=2;bytes=5;extra=16;break;
   case 0xbdc8:ops=1;bytes=2;extra=16;break;
   case 0xbdcd:ops=4;bytes=9;extra=70;break;
   case 0xbdcf:ops=3;bytes=7;extra=54;break;
   case 0xbdd1:ops=2;bytes=5;extra=38;break;
   case 0xbde0:ops=2;bytes=4;extra=44;break;
  }
  if(ops){const Snes*s=(const Snes*)c->mem;unsigned h=s->hPos;return budget>=ops && !(h&1u) && h<1358u && ta_r41_busEndLimit[h]>=h+bytes*(s->fastMem?6u:8u)+extra;}
 }
#endif
 if(!c || !c->mem || budget<(c->pc>=0xbd6d?4u:5u))return false;
 const Snes* s=(const Snes*)c->mem;const unsigned h=s->hPos;
 return !(h&1u) && h<1358u && ta_r41_busEndLimit[h]>=h+(c->pc>=0xbd6d?(s->fastMem?116u:138u):(s->fastMem?158u:194u));
}
unsigned ta_native_r73_chain(Cpu* cpu,unsigned budget);
#if TA_R73_COVERAGE
void ta_native_r73_counts(uint32_t stages[8],uint32_t* calls,uint32_t* rejects,uint64_t* ops);
#endif
#else
#define TA_R73_CHAIN_ON 0
#endif
#endif

