/* R50: one exact SPC700 bus cycle, with an event-free RAM fast path.
 * DSP (every 32 cycles), timer reloads and I/O/boot ROM retain original calls.
 * Accesses are not combined and instruction execution remains unchanged. */
#ifndef TA_SPC_BUS_H
#define TA_SPC_BUS_H
#include "apu.h"
#include "ta_apu_clock.h"
#if defined(XBOX_PORT) && defined(TA_R50_SPC_BUS) && TA_R50_SPC_BUS && !TA_R32_DIAGNOSTICS
#define TA_R50_SPC_ON 1
static inline bool ta_r50_spcClock(Apu* a){
#if TA_R59_APU_ON
 if(a->taClockActive) {
  if(!a->taClockRemaining)return false;
  --a->taClockRemaining;++a->cycles;return true;
 }
#endif
 if(!(a->cycles&31u) || !a->timer[0].cycles || !a->timer[1].cycles || !a->timer[2].cycles)return false;
 --a->timer[0].cycles;--a->timer[1].cycles;--a->timer[2].cycles;++a->cycles;return true;
}
#else
#define TA_R50_SPC_ON 0
#endif
static inline uint8_t ta_r50_spcRead(void* mem,uint16_t adr){
#if TA_R50_SPC_ON
 Apu* a=(Apu*)mem;
 if((adr<0xf0u || adr>0xffu) && (!a->romReadable || adr<0xffc0u) && ta_r50_spcClock(a))return a->ram[adr];
#endif
 return apu_spcRead(mem,adr);
}
static inline void ta_r50_spcWrite(void* mem,uint16_t adr,uint8_t value){
#if TA_R50_SPC_ON
 Apu* a=(Apu*)mem;
 if((adr<0xf0u || adr>0xffu) && ta_r50_spcClock(a)){a->ram[adr]=value;return;}
#endif
 apu_spcWrite(mem,adr,value);
}
static inline void ta_r50_spcIdle(void* mem,bool waiting){
#if TA_R50_SPC_ON
 if(ta_r50_spcClock((Apu*)mem))return;
#endif
 apu_spcIdle(mem,waiting);
}
#endif
