/* R45: inline only event-free mapped code reads. Every byte retains its
 * original access duration, open bus and PC update order. Events, pending
 * DMA/HDMA, hooks, unmapped pages and low-half execution use R16 unchanged. */
#ifndef TA_CODE_FETCH_H
#define TA_CODE_FETCH_H
#include "snes.h"
#include "dma.h"
#include "ta_timer_bus.h"
extern uint8_t snes_cpuReadOpcodeFast(void* mem, uint32_t adr);
#if defined(XBOX_PORT) && defined(TA_R45_INLINE_CODE_FETCH) && TA_R45_INLINE_CODE_FETCH
extern const uint8_t* snes_taR45CodePages[4096];
#include "ta_bus_end_limit.h"
#endif
static inline __attribute__((always_inline)) uint8_t ta_r45_readCode(void* mem, uint32_t adr) {
#if defined(XBOX_PORT) && defined(TA_R45_INLINE_CODE_FETCH) && TA_R45_INLINE_CODE_FETCH && TA_R14_EVENT_FREE_SHORTSTEP && TA_R25_INLINE_BUS_SHORTSTEP && TA_R41_BUS_END_LIMIT && TA_R16_DIRECT_OPCODE_PAGE && !TA_R32_DIAGNOSTICS
  Snes* s=(Snes*)mem;
  const uint32_t a24=adr & 0xffffffu;
  const unsigned clocks=(s->fastMem && a24>=0x800000u)?6u:8u;
  if(__builtin_expect((a24 & 0x8000u) && !s->readHook &&
      !s->dma->dmaState && !s->dma->hdmaInitRequested && !s->dma->hdmaRunRequested &&
      ta_r49_noTimerEdge(s,s->hPos,s->hPos+clocks),1)) {
    const unsigned h=s->hPos,end=h+clocks;
    if(__builtin_expect(h<1358u && end<=ta_r41_busEndLimit[h],1)) {
      const uint8_t* page=snes_taR45CodePages[a24>>12];
      if(__builtin_expect(page!=NULL,1)) {
        s->apuMasterPending+=clocks;
        s->hPos=(uint16_t)end;
        s->cycles+=clocks;
        if(s->autoJoyTimer>0)
          s->autoJoyTimer=clocks>=s->autoJoyTimer?0:(uint16_t)(s->autoJoyTimer-clocks);
        const uint8_t value=page[a24 & 4095u];
        s->openBus=value;
        return value;
      }
    }
  }
#endif
  return snes_cpuReadOpcodeFast(mem,adr);
}
#endif
