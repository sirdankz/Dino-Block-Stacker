/* Included by ta_native.c. Fixed native game operations for the observed
 * animation descriptor path. Original callback bodies and returns remain
 * reference. Each accepted segment fits the exact existing hardware fence. */
#if TA_R81_ANIMATION_ON
#if TA_R81_COVERAGE
static uint32_t s_r81Stages[8],s_r81Calls;static uint64_t s_r81Ops;
void ta_native_r81_counts(uint32_t stages[8],uint32_t* calls,uint64_t* ops){for(unsigned i=0;i<8;++i)if(stages)stages[i]=s_r81Stages[i];if(calls)*calls=s_r81Calls;if(ops)*ops=s_r81Ops;}
#endif
unsigned ta_native_r81_animation(Cpu* c,unsigned budget){
 if(!c||!s_identical_rom||!s_r60ShaVerified||c->k!=0x89||c->dp||c->e||c->d||c->sp<0x100u||c->sp>0x1ff8u||c->waiting||c->stopped||c->resetWanted||c->intWanted||c->nmiWanted||(c->irqWanted&&!c->i))return 0;
 Snes* s=c->mem;if(!s||!s->cart||!s->cart->rom||s->cart->romSize!=1048576u||s->readHook||s->writeHook||s->inDma||s->dma->dmaState||s->dma->hdmaInitRequested||s->dma->hdmaRunRequested)return 0;
 const uint32_t pointer=r62_word(s->ram+0x96)|((uint32_t)s->ram[0x98]<<16);const unsigned bank=pointer>>16,offset=pointer&65535u;
 /* Entire descriptor is WRAM and disjoint from DP/stack. No hardware reads,
  * bank carry or pointer alias is collapsed; other domains use fallback. */
 if((bank!=0x7e&&bank!=0x7f)||offset<0x2000u||offset>0xfff6u)return 0;const unsigned desc=pointer&0x1ffffu;
 const unsigned initial=s->hPos,fetch=s->fastMem?6u:8u;if((initial&1u)||initial>=1358u||s->irqCondition)return 0;
 unsigned limit=ta_r41_busEndLimit[initial];if(s->hIrqEnabled||s->vIrqEnabled){if(s->autoJoyTimer&1u)return 0;if(!s->vIrqEnabled||s->vPos==s->vTimer){if(!s->hIrqEnabled)return 0;unsigned edge=(unsigned)s->hTimer*4u;if(edge>=initial&&edge<limit)limit=edge;}}
 if(limit<initial)return 0;unsigned h=initial,run=0;
 while(c->sp>=0x100u&&c->sp<=0x1ff8u){
  const unsigned entry=c->pc;unsigned stage,ops,clocks;
  if(entry==0xa36a){
   if(c->mf||c->xf)break;stage=0;ops=4;clocks=4u*fetch+78u;if(ops>budget-run||clocks>limit-h)break;
   unsigned sp=c->sp;s->ram[sp--]=(uint8_t)(c->x>>8);s->ram[sp--]=(uint8_t)c->x;s->ram[sp--]=(uint8_t)(c->y>>8);s->ram[sp--]=(uint8_t)c->y;s->ram[sp]=0x89;
   c->sp=(uint16_t)sp;c->db=0x89;c->n=true;c->z=false;c->pc=0xa36e;s->openBus=0x89;
  }else if(entry==0xa36e||entry==0xa371){
   if(c->mf||c->xf||(entry==0xa371&&c->y!=6))break;const uint16_t flags=r62_word(s->ram+desc+6u);const bool active=(flags&1u)!=0;
   stage=1;ops=(active?4u:3u)+(entry==0xa36e?1u:0u);clocks=((active?9u:7u)+(entry==0xa36e?3u:0u))*fetch+46u;if(ops>budget-run||clocks>limit-h)break;
   c->y=6;c->a=flags;c->n=(flags&0x8000u)!=0;c->z=!active;
   if(active){c->mf=c->xf=true;c->x&=255u;c->y&=255u;c->pc=0xa37a;s->openBus=0x30;}else{c->pc=0xa3a7;s->openBus=0x2f;}
  }else if(entry==0xa37a||entry==0xa39e){
   if(!c->mf||!c->xf)break;const uint8_t count=s->ram[desc+5u];const bool initialCount=entry==0xa37a;stage=initialCount?2u:5u;
   ops=!count?3u:initialCount?5u:6u;clocks=(!count?6u:initialCount?9u:11u)*fetch+(!count?38u:initialCount?52u:76u);if(ops>budget-run||clocks>limit-h)break;
   c->y=5;c->a=(c->a&0xff00u)|count;c->n=(count&0x80u)!=0;c->z=count==0;
   if(!count){c->pc=0xa3ae;s->openBus=initialCount?0x2e:0x0a;}
   else if(initialCount){c->mf=false;s->ram[c->sp--]=c->db;c->pc=0xa383;s->openBus=c->db;}
   else{const uint8_t next=(uint8_t)(count-1u);c->a=(c->a&0xff00u)|next;s->ram[desc+5u]=next;c->n=(next&0x80u)!=0;c->z=next==0;c->mf=c->xf=false;c->pc=0xa3a9;s->openBus=0x30;}
  }else if(entry==0xa383){
   if(c->mf||!c->xf)break;stage=3;ops=4;clocks=8u*fetch+62u;if(ops>budget-run||clocks>limit-h)break;
   c->y=8;c->a=r62_word(s->ram+desc+8u);r62_store(s->ram+0x9c,c->a);c->n=(c->a&0x8000u)!=0;c->z=c->a==0;c->mf=true;c->pc=0xa38b;s->openBus=0x20;
  }else if(entry==0xa38b){
   if(!c->mf||!c->xf)break;stage=4;ops=7;clocks=11u*fetch+94u;if(ops>budget-run||clocks>limit-h)break;
   const uint8_t callbackBank=s->ram[desc+2u];c->y=2;c->a=(c->a&0xff00u)|callbackBank;s->ram[0x9e]=callbackBank;s->ram[c->sp]=callbackBank;
   c->db=callbackBank;c->n=(callbackBank&0x80u)!=0;c->z=callbackBank==0;c->mf=c->xf=false;
   const uint8_t flags=(c->n?0x80u:0u)|(c->v?0x40u:0u)|(c->d?8u:0u)|(c->i?4u:0u)|(c->z?2u:0u)|(c->c?1u:0u);s->ram[c->sp--]=flags;
   c->pc=0xa396;s->openBus=flags;
  }else if(entry==0xa3a4){
   if(!c->mf||!c->xf||c->y!=5)break;stage=5;ops=3;clocks=5u*fetch+44u;if(ops>budget-run||clocks>limit-h)break;
   const uint8_t next=(uint8_t)(c->a-1u);c->a=(c->a&0xff00u)|next;s->ram[desc+5u]=next;c->n=(next&0x80u)!=0;c->z=next==0;c->mf=c->xf=false;c->pc=0xa3a9;s->openBus=0x30;
  }else if(entry==0xa3a7||entry==0xa3a9){
   if(entry==0xa3a9&&(c->mf||c->xf))break;stage=entry==0xa3a7?6u:7u;ops=entry==0xa3a7?4u:3u;clocks=(entry==0xa3a7?5u:3u)*fetch+(entry==0xa3a7?82u:76u);if(ops>budget-run||clocks>limit-h)break;
   unsigned sp=c->sp;c->mf=c->xf=false;c->y=r62_word(s->ram+sp+1u);sp+=2;c->x=r62_word(s->ram+sp+1u);sp+=2;c->db=s->ram[++sp];c->sp=(uint16_t)sp;c->n=(c->db&0x80u)!=0;c->z=c->db==0;c->pc=0xa3ac;s->openBus=c->db;
  }else break;
  h+=clocks;run+=ops;
#if TA_R81_COVERAGE
  if(s_r81Stages[stage]!=UINT32_MAX)++s_r81Stages[stage];s_r81Ops+=ops;
#endif
 }
 if(!run)return 0;unsigned clocks=h-initial;s->hPos=(uint16_t)h;s->cycles+=clocks;s->apuMasterPending+=clocks;if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if TA_R81_COVERAGE
 if(s_r81Calls!=UINT32_MAX)++s_r81Calls;
#endif
 return run;
}
#endif
