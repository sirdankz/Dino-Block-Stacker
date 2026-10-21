/* R116: instruction-local proof of an event-free CPU bus window.
 * This does not batch guest accesses: every byte/idle still updates hPos,
 * cycles, APU pending clocks, joy timer and open bus in the original order.
 * Any unsupported address or exhausted window is invalidated BEFORE calling
 * the original bus, which may invoke observers, DMA or hardware events. */
#ifndef TA_R116_CPU_BUS_WINDOW_H
#define TA_R116_CPU_BUS_WINDOW_H
#if defined(XBOX_PORT) && defined(TA_R116_CPU_BUS_WINDOW) && TA_R116_CPU_BUS_WINDOW
#if !TA_R45_INLINE_CODE_FETCH || !TA_R46_INLINE_WRAM || !TA_R41_BUS_END_LIMIT || !TA_R49_TIMER_BUS || TA_R32_DIAGNOSTICS
#error R116 requires the clean R45/R46/R41/R49 bus implementation
#endif
#define TA_R116_WINDOW_ON 1
/* Single synchronous CPU executor; never persisted across instructions or
 * exposed across callbacks. No SNES state layout/save-state changes. */
static unsigned s_taR116BusEnd;
#if defined(TA_R116_HOST_SWITCH) && TA_R116_HOST_SWITCH
static bool s_taR116HostEnable=true;
#endif
#if defined(TA_R116_BUS_PROFILE) && TA_R116_BUS_PROFILE
static uint64_t s_taR116Counts[6];
void cpu_taR116Counts(uint64_t out[6]){memcpy(out,s_taR116Counts,sizeof s_taR116Counts);}
#define TA_R116_COUNT(i) (++s_taR116Counts[i])
#else
#define TA_R116_COUNT(i) ((void)0)
#endif
static __attribute__((noinline)) void ta_r116_begin(Cpu* cpu) {
  s_taR116BusEnd=0;TA_R116_COUNT(0);
#if defined(TA_R116_HOST_SWITCH) && TA_R116_HOST_SWITCH
  if(!s_taR116HostEnable)return;
#endif
  const Snes* s=(const Snes*)cpu->mem;const unsigned h=s->hPos;
  if(h>=1358u || (h&1u) || (s->autoJoyTimer&1u) || s->inDma ||
     s->readHook || s->writeHook || s->dma->dmaState ||
     s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return;
  unsigned end=ta_r41_busEndLimit[h];
  if(end<=h)return;
  if(!ta_r49_noTimerEdge(s,h,end)) {
    /* Shorten to an IRQ edge only if the original timer predicate proves
     * that the shortened interval is safe. Never expand the R41 fence. */
    const unsigned irq=(unsigned)s->hTimer*4u;
    if(!s->hIrqEnabled || irq<=h || irq>=end || !ta_r49_noTimerEdge(s,h,irq))return;
    end=irq;
  }
  s_taR116BusEnd=end;TA_R116_COUNT(1);
}
static inline __attribute__((always_inline)) bool ta_r116_advance(Snes* s,unsigned clocks) {
  const unsigned end=(unsigned)s->hPos+clocks;
  if(end>s_taR116BusEnd)return false;
  s->apuMasterPending+=clocks;s->hPos=(uint16_t)end;s->cycles+=clocks;
  if(s->autoJoyTimer>0)s->autoJoyTimer=clocks>=s->autoJoyTimer?0:(uint16_t)(s->autoJoyTimer-clocks);
  return true;
}
static inline bool ta_r116_wramOffset(uint32_t adr,unsigned* offset) {
  const uint32_t a=adr&0xffffffu;
  if((a&0xfe0000u)==0x7e0000u)*offset=a&0x1ffffu;
  else if((a&0x40e000u)==0)*offset=a&0x1fffu;
  else return false;
  return true;
}
#define TA_R116_BEGIN(cpu) ta_r116_begin(cpu)
#define TA_R116_END() (s_taR116BusEnd=0)
#else
#define TA_R116_WINDOW_ON 0
#define TA_R116_BEGIN(cpu) ((void)0)
#define TA_R116_END() ((void)0)
#endif
#endif
