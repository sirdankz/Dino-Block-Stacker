#ifndef TA_DMA_CLOCK_H
#define TA_DMA_CLOCK_H

#include "snes.h"

#if defined(XBOX_PORT) && defined(TA_R44_DMA_CLOCK_FAST) && TA_R44_DMA_CLOCK_FAST
#include "ta_bus_end_limit.h"
#endif

/* Clock advancement only: the DMA caller retains its existing HDMA request
 * handling, byte access order, alignment and stolen-cycle accounting. */
static inline void ta_dma_runClock8(Snes* snes) {
#if defined(XBOX_PORT) && defined(TA_R44_DMA_CLOCK_FAST) && TA_R44_DMA_CLOCK_FAST
  if(__builtin_expect(!snes->hIrqEnabled && !snes->vIrqEnabled &&
                      !snes->irqCondition, 1)) {
    const unsigned h = snes->hPos;
    const unsigned end = h + 8u;
    /* R41's exact gate excludes event starts, refresh crossings and every
     * possible short-line wrap. Failed guards retain the original scheduler. */
    if(__builtin_expect(h < 1358u && end <= ta_r41_busEndLimit[h], 1)) {
      snes->apuMasterPending += 8u;
      snes->hPos = (uint16_t)end;
      snes->cycles += 8u;
      if(snes->autoJoyTimer > 0)
        snes->autoJoyTimer = 8u >= snes->autoJoyTimer
          ? 0 : (uint16_t)(snes->autoJoyTimer - 8u);
      return;
    }
  }
#endif
  snes_runCycles(snes, 8);
}

#endif
