/* Include in ta_native.c after ta_setFlags. The exact-ROM identity and
 * existing flag restoration helper belong to that translation unit. */
#ifndef TA_COUNTER_BLOCK_IMPL_H
#define TA_COUNTER_BLOCK_IMPL_H
#include "ta_counter_block.h"
#if TA_R58_COUNTER_ON
#if defined(TA_R58_HOST_COVERAGE) && TA_R58_HOST_COVERAGE
uint64_t ta_r58HostCounterHits[2];
#endif
/* LDA $B3; ORA $B5; AND #$FFF0. A zero mask reaches the two final INCs;
 * otherwise both pairs of INCs execute. A wrapping INC takes additional
 * carry-counter code, so those states retain the original executor.
 * Each accepted group preserves all clocks and final state inside the
 * unchanged R41/R49 observer/event/IRQ fence: 21F+182 or 31F+264. */
unsigned ta_native_r58_counter_block(Cpu* cpu, unsigned opcodeBudget) {
  if(!cpu || !s_identical_rom || cpu->k!=0x84u || cpu->pc!=0x8172u ||
     cpu->db!=0x7eu || cpu->dp!=0u || cpu->mf || cpu->xf || cpu->e ||
     cpu->waiting || cpu->stopped || cpu->resetWanted || cpu->intWanted ||
     cpu->nmiWanted || (cpu->irqWanted&&!cpu->i) ||
     cpu->sp<0x100u || cpu->sp>0x1ffbu)return 0;
  Snes* s=(Snes*)cpu->mem;
  if(!s || s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
     s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return 0;
  const uint16_t mask=(uint16_t)((s->ram[0xb3u]|((uint16_t)s->ram[0xb4u]<<8)|
                              s->ram[0xb5u]|((uint16_t)s->ram[0xb6u]<<8))&0xfff0u);
  const unsigned kind=mask!=0u;
  const unsigned count=kind?14u:10u;
  if(opcodeBudget<count)return 0;
  const unsigned clocks=(kind?31u:21u)*(s->fastMem?6u:8u)+(kind?264u:182u);
  const unsigned h=s->hPos,end=h+clocks;
  if((h&1u) || h>=1358u || end>ta_r41_busEndLimit[h] ||
     !ta_r49_noTimerEdge(s,h,end))return 0;
  const unsigned increments=kind?2u:1u;
  unsigned first=s->ram[0x1a74u]|((unsigned)s->ram[0x1a75u]<<8);
  unsigned second=s->ram[0x1a78u]|((unsigned)s->ram[0x1a79u]<<8);
  if(first>0xffffu-increments || second>0xffffu-increments)return 0;
  first+=increments;second+=increments;
  s->ram[0x1a74u]=(uint8_t)first;s->ram[0x1a75u]=(uint8_t)(first>>8);
  s->ram[0x1a78u]=(uint8_t)second;s->ram[0x1a79u]=(uint8_t)(second>>8);
  /* Stack bytes can alias either counter. Read PLP/RTL operands only after
   * every preceding INC write, as the original instruction order requires. */
  const unsigned sp=cpu->sp;
  const uint8_t flags=s->ram[sp+1u];
  const uint16_t returnPc=(uint16_t)(s->ram[sp+2u]|((uint16_t)s->ram[sp+3u]<<8));
  const uint8_t returnBank=s->ram[sp+4u];
  cpu->a=mask;ta_setFlags(cpu,flags);
  cpu->sp=(uint16_t)(sp+4u);cpu->pc=(uint16_t)(returnPc+1u);cpu->k=returnBank;
  /* RTL samples pending IRQ after PLP restored I. NMI was excluded above. */
  cpu->intWanted=cpu->irqWanted&&!cpu->i;
  s->openBus=returnBank;s->hPos=(uint16_t)end;
  s->cycles+=clocks;s->apuMasterPending+=clocks;
  if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:
    (uint16_t)(s->autoJoyTimer-clocks);
#if defined(TA_R58_HOST_COVERAGE) && TA_R58_HOST_COVERAGE
  ++ta_r58HostCounterHits[kind];
#endif
  return count;
}
#endif
#endif
