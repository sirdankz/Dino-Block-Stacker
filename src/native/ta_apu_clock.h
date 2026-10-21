/* R59: defer event-free timer decrements within one APU catchup call.
 * All DSP/reload cycles and I/O still execute through the original callback.
 * The canonical timers are settled before any event and before returning. */
#ifndef TA_APU_CLOCK_H
#define TA_APU_CLOCK_H
#include "apu.h"
#if defined(XBOX_PORT) && defined(TA_R59_APU_CLOCK) && TA_R59_APU_CLOCK && \
    defined(TA_R50_SPC_BUS) && TA_R50_SPC_BUS && !TA_R32_DIAGNOSTICS
#define TA_R59_APU_ON 1
static inline void ta_r59_apuFlush(Apu* a) {
  const uint32_t elapsed = a->cycles - a->taClockBase;
  a->timer[0].cycles -= elapsed;
  a->timer[1].cycles -= elapsed;
  a->timer[2].cycles -= elapsed;
  a->taClockBase = a->cycles;
}
static inline void ta_r59_apuArm(Apu* a) {
  unsigned n = (a->cycles & 31u) ? 32u - (a->cycles & 31u) : 0u;
  if(n > a->timer[0].cycles) n = a->timer[0].cycles;
  if(n > a->timer[1].cycles) n = a->timer[1].cycles;
  if(n > a->timer[2].cycles) n = a->timer[2].cycles;
  a->taClockBase = a->cycles;
  a->taClockRemaining = n;
}
static inline void ta_r59_apuBegin(Apu* a) {
  a->taClockActive = true;
  ta_r59_apuArm(a);
}
static inline void ta_r59_apuEnd(Apu* a) {
  if(a->taClockActive)ta_r59_apuFlush(a);
  a->taClockRemaining = 0;
  a->taClockActive = false;
}
#else
#define TA_R59_APU_ON 0
#endif
#endif
