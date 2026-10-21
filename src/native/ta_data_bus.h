/* R46: WRAM-only event-free data bus. Each byte keeps its original eight
 * master clocks, open bus, access order and interrupt-sampling position.
 * Hooks, DMA/HDMA, IRQ and event boundaries use the unchanged reference bus.
 * No map/table/cache is added. ROM, SRAM and I/O always use the original bus. */
#ifndef TA_DATA_BUS_H
#define TA_DATA_BUS_H
#include "snes.h"
#include "dma.h"
#include "ta_timer_bus.h"
#if defined(XBOX_PORT) && defined(TA_R46_INLINE_WRAM) && TA_R46_INLINE_WRAM && TA_R14_EVENT_FREE_SHORTSTEP && TA_R25_INLINE_BUS_SHORTSTEP && TA_R41_BUS_END_LIMIT && !TA_R32_DIAGNOSTICS
#include "ta_bus_end_limit.h"
#define TA_R46_WRAM_ON 1
static inline bool ta_r46_wramWindow(Snes* s,uint32_t adr,bool write,unsigned* offset) {
  const uint32_t a24=adr & 0xffffffu;
  if((a24 & 0xfe0000u)==0x7e0000u) *offset=a24 & 0x1ffffu;
  else if((a24 & 0x40e000u)==0) *offset=a24 & 0x1fffu;
  else return false;
  if((write ? s->writeHook!=NULL : s->readHook!=NULL) ||
      s->dma->dmaState || s->dma->hdmaInitRequested || s->dma->hdmaRunRequested ||
      !ta_r49_noTimerEdge(s,s->hPos,s->hPos+8u)) return false;
  const unsigned h=s->hPos,end=h+8u;
  if(h>=1358u || end>ta_r41_busEndLimit[h]) return false;
  s->apuMasterPending+=8u;s->hPos=(uint16_t)end;s->cycles+=8u;
  if(s->autoJoyTimer>0) s->autoJoyTimer=s->autoJoyTimer<=8u ? 0 : (uint16_t)(s->autoJoyTimer-8u);
  return true;
}
#else
#define TA_R46_WRAM_ON 0
#endif
static inline uint8_t ta_r46_readData(void* mem,uint32_t adr) {
#if TA_R46_WRAM_ON
  Snes* s=(Snes*)mem;unsigned offset;
  if(ta_r46_wramWindow(s,adr,false,&offset)) {
    const uint8_t value=s->ram[offset];s->openBus=value;return value;
  }
#endif
  return snes_cpuRead(mem,adr);
}
static inline void ta_r46_writeData(void* mem,uint32_t adr,uint8_t value) {
#if TA_R46_WRAM_ON
  Snes* s=(Snes*)mem;unsigned offset;
  if(ta_r46_wramWindow(s,adr,true,&offset)) {
    s->openBus=value;s->ram[offset]=value;return;
  }
#endif
  snes_cpuWrite(mem,adr,value);
}
#endif
