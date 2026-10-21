/* R59 ROM-loaded SPC blocks. Original reads/writes/idles, exact flags and
 * per-opcode return boundary; no DSP/channel/audio work is omitted. */
#if TA_R59_SPC_BLOCKS_ON
extern bool ta_r47IdenticalRom;
#if defined(TA_R59_HOST_COVERAGE) && TA_R59_HOST_COVERAGE
uint64_t ta_r59HostSpcBlocks[2];
#endif
static inline uint32_t spc_r59_code32(const uint8_t* p) {
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
/* Known ordinary-RAM code reads have no side effects. With the callback and
 * echo guards below, only their clocks are observable. Coalesce clocks only
 * before the next DSP/timer event; all data, I/O and stack accesses stay exact. */
static __attribute__((noinline)) void spc_r59_advanceEvent(Spc* spc,unsigned n) {
 while(n--)spc_idle(spc);
}
static inline __attribute__((always_inline)) void spc_r59_advanceCode(Spc* spc,unsigned n) {
#if TA_R59_APU_ON
 Apu* a=(Apu*)spc->mem;
 if(a->taClockActive && a->taClockRemaining>=n) {
  a->taClockRemaining-=n;a->cycles+=n;return;
 }
#endif
 spc_r59_advanceEvent(spc,n);
}
bool spc_r59_echoOverlap(uint16_t echo,uint16_t begin,unsigned length) {
 return (uint16_t)(echo-begin+3u)<length+3u;
}
bool spc_r59_nativeBlock(Spc* spc,unsigned remaining) {
 if(remaining<32u || !ta_r47IdenticalRom || spc->resetWanted || spc->stopped)return false;
 Apu* a=(Apu*)spc->mem;Dsp* d=a->dsp;
 if(d->externalSample)return false;
 const unsigned pc=spc->pc;unsigned end,kind;const uint8_t* code=a->ram+pc;
 if(pc==0x0764u) {
  if(spc->p || spc_r59_code32(code)!=0xc407c1f6u ||
     spc_r59_code32(code+4)!=0x07cbf6f2u || spc_r59_code32(code+8)!=0xf3c4e65du ||
     code[12]!=0xfeu || code[13]!=0xe4u)return false;
  /* ESA could redirect the one possible DSP echo write into the code. Its
   * address-table byte must also survive that write until the original read. */
  if(a->ram[0x07c1u+spc->y]==0x6du)return false;
  end=0x0772u;kind=0;
 } else if(pc==0x0ed6u) {
  if(spc_r59_code32(code)!=0xc402b1f5u || spc_r59_code32(code+4)!=0x02b0f50bu ||
     code[8]!=0xc4u || code[9]!=0x0au || code[10]!=0x6fu)return false;
  end=0x0ee1u;kind=1;
 } else return false;
 /* Both groups take at most 29 cycles: at most one DSP sample occurs. The
  * only internal DSP RAM writer is echo; source values are still read at
  * their original cycles, including any permitted echo/source aliases. */
 const uint16_t echo=(uint16_t)(d->echoBufferAdr+d->echoBufferIndex);
 if(spc_r59_echoOverlap(echo,pc,end-pc) ||
    (kind==0 && spc_r59_echoOverlap(echo,0x07c1u+spc->y,1)))return false;
#if defined(TA_R59_HOST_COVERAGE) && TA_R59_HOST_COVERAGE
 ++ta_r59HostSpcBlocks[kind];
#endif
#if defined(TA_R59_SPC_FETCH_BATCH) && TA_R59_SPC_FETCH_BATCH
 if(kind==0) {
  spc_r59_advanceCode(spc,3);spc->pc+=3;spc_idle(spc);
  spc->a=spc_read(spc,0x07c1u+spc->y);spc_setZN(spc,spc->a);
  spc_r59_advanceCode(spc,2);spc->pc+=2;spc_movs(spc,0xf2);
  spc_r59_advanceCode(spc,3);spc->pc+=3;spc_idle(spc);
  spc->a=spc_read(spc,0x07cbu+spc->y);spc_setZN(spc,spc->a);
  spc_r59_advanceCode(spc,2);++spc->pc;spc->x=spc->a;spc_setZN(spc,spc->x);
  spc_r59_advanceCode(spc,2);++spc->pc;spc->a=spc_read(spc,spc->x);spc_setZN(spc,spc->a);
  spc_r59_advanceCode(spc,2);spc->pc+=2;spc_movs(spc,0xf3);
  spc_r59_advanceCode(spc,3);++spc->pc;--spc->y;
  spc_r59_advanceCode(spc,1);++spc->pc;
  spc_doBranch(spc,0xe4,spc->y!=0);
 } else {
  spc_r59_advanceCode(spc,3);spc->pc+=3;spc_idle(spc);
  spc->a=spc_read(spc,0x02b1u+spc->x);spc_setZN(spc,spc->a);
  spc_r59_advanceCode(spc,2);spc->pc+=2;spc_movs(spc,0x0bu|(spc->p<<8));
  spc_r59_advanceCode(spc,3);spc->pc+=3;spc_idle(spc);
  spc->a=spc_read(spc,0x02b0u+spc->x);spc_setZN(spc,spc->a);
  spc_r59_advanceCode(spc,2);spc->pc+=2;spc_movs(spc,0x0au|(spc->p<<8));
  spc_r59_advanceCode(spc,3);++spc->pc;spc->pc=spc_pullWord(spc);
 }
#else
 if(kind==0) {
  (void)spc_readOpcode(spc);spc_mov(spc,spc_adrAby(spc));
  (void)spc_readOpcode(spc);spc_movs(spc,spc_adrDp(spc));
  (void)spc_readOpcode(spc);spc_mov(spc,spc_adrAby(spc));
  (void)spc_readOpcode(spc);spc_read(spc,spc->pc);spc->x=spc->a;spc_setZN(spc,spc->x);
  (void)spc_readOpcode(spc);spc_mov(spc,spc_adrInd(spc));
  (void)spc_readOpcode(spc);spc_movs(spc,spc_adrDp(spc));
  (void)spc_readOpcode(spc);spc_read(spc,spc->pc);spc_idle(spc);--spc->y;
  spc_doBranch(spc,spc_readOpcode(spc),spc->y!=0);
 } else {
  (void)spc_readOpcode(spc);spc_mov(spc,spc_adrAbx(spc));
  (void)spc_readOpcode(spc);spc_movs(spc,spc_adrDp(spc));
  (void)spc_readOpcode(spc);spc_mov(spc,spc_adrAbx(spc));
  (void)spc_readOpcode(spc);spc_movs(spc,spc_adrDp(spc));
  (void)spc_readOpcode(spc);spc_read(spc,spc->pc);spc_idle(spc);spc->pc=spc_pullWord(spc);
 }
#endif
 return true;
}
#endif
