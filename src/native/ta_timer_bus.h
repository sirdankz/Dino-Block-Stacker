/* R49: exact timer-IRQ exclusion for one original CPU bus transaction.
 * No instruction or memory access is merged. IRQ samples and bus observers
 * retain their original positions; any edge/event retains scalar timing.
 * Existing R41 fences separately exclude refresh/raster/line events. */
#ifndef TA_TIMER_BUS_H
#define TA_TIMER_BUS_H
#include "snes.h"
#if defined(XBOX_PORT) && defined(TA_R49_TIMER_BUS) && TA_R49_TIMER_BUS && TA_R14_EVENT_FREE_SHORTSTEP && TA_R25_INLINE_BUS_SHORTSTEP && TA_R41_BUS_END_LIMIT && !TA_R32_DIAGNOSTICS
#define TA_R49_TIMER_ON 1
#if defined(TA_R49_HOST_DIFF) && TA_R49_HOST_DIFF
extern bool ta_r49HostEnable;
#endif
#else
#define TA_R49_TIMER_ON 0
#endif
static inline __attribute__((always_inline)) bool ta_r49_noTimerEdge(const Snes* s,unsigned h,unsigned end) {
  if(s->irqCondition)return false;
  if(!s->hIrqEnabled && !s->vIrqEnabled)return true;
#if TA_R49_TIMER_ON
#if defined(TA_R49_HOST_DIFF) && TA_R49_HOST_DIFF
  if(!ta_r49HostEnable)return false;
#endif
  /* Odd synthetic timers keep the reference's scalar uint16_t behavior. */
  if((h&1u) || (s->autoJoyTimer&1u))return false;
  if(s->vIrqEnabled && s->vPos!=s->vTimer)return true;
  if(!s->hIrqEnabled)return false;
  const unsigned irqClock=(unsigned)s->hTimer*4u;
  return irqClock<h || irqClock>=end;
#else
  (void)h;(void)end;return false;
#endif
}
#endif
