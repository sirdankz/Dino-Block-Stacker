/* Exact SHA-verified bank88 prefixes; never cross the old R41 event horizon. */
#if defined(TA_R126_CPU_PREFIX) && TA_R126_CPU_PREFIX
#if defined(TA_R126_HOST_COVERAGE) && TA_R126_HOST_COVERAGE
uint64_t ta_r126PrefixCalls,ta_r126PrefixOps;
#endif
unsigned ta_native_r126_prefix(Cpu*c,unsigned budget){
 if(!c||!s_identical_rom||!s_r60ShaVerified||c->k!=0x88||c->db!=0x88||
    c->dp||c->e||c->mf||c->xf||c->d||c->waiting||c->stopped||c->resetWanted||
    c->intWanted||c->nmiWanted||(c->irqWanted&&!c->i))return 0;
 Snes*s=c->mem;if(!s||s->readHook||s->writeHook||s->inDma||s->dma->dmaState||
    s->dma->hdmaInitRequested||s->dma->hdmaRunRequested||s->irqCondition)return 0;
 const unsigned initial=s->hPos,fetch=s->fastMem?6u:8u;
 if((initial&1u)||initial>=1358u)return 0;
 const unsigned limit=ta_r41_busEndLimit[initial];unsigned h=initial,run=0;
 while(run<budget){
  unsigned bytes,extra,kind,operand=0;const unsigned pc=c->pc;
  switch(pc){
   case 0x82ed:kind=0;bytes=1;extra=22;if(c->sp<0x102||c->sp>0x1fff)goto done;break;
   case 0x82ee:case 0x8303:case 0x8315:kind=1;bytes=4;extra=16;operand=0x933b;break;
   case 0x82f2:case 0x8307:kind=2;bytes=1;extra=6;break;
   case 0x82f3:case 0x8308:kind=1;bytes=4;extra=16;operand=(pc==0x82f3?0x90b0u:0x90afu)+c->x;break;
   case 0x82f7:case 0x830c:kind=3;bytes=3;extra=0;break;
   case 0x8319:case 0x8323:kind=4;bytes=1;extra=6;break;
   case 0x831a:case 0x8324:kind=5;bytes=3;extra=0;break;
   case 0x831d:kind=6;bytes=4;extra=16;operand=0x933b;break;
   case 0x8321:kind=1;bytes=2;extra=16;operand=0x96;break;
   case 0x8327:kind=6;bytes=2;extra=16;operand=0x96;break;
   case 0x8329:kind=7;bytes=1;extra=28;if(c->sp<0x100||c->sp>0x1ffd)goto done;break;
   case 0x832a:kind=8;bytes=1;extra=6;break;
   default:goto done;
  }
  const unsigned clocks=bytes*fetch+extra;
  if(h+clocks>limit||!ta_r49_noTimerEdge(s,h,h+clocks))break;
  uint16_t v;
  switch(kind){
   case 0:s->ram[c->sp--]=(uint8_t)(c->x>>8);s->ram[c->sp--]=(uint8_t)c->x;s->openBus=(uint8_t)c->x;break;
   case 1:c->a=ta_r108_word(s->ram+operand);c->n=(c->a&0x8000u)!=0;c->z=c->a==0;s->openBus=(uint8_t)(c->a>>8);break;
   case 2:c->x=c->a;c->n=(c->x&0x8000u)!=0;c->z=c->x==0;s->openBus=0xaa;break;
   case 3:c->a&=255u;c->n=false;c->z=c->a==0;s->openBus=0;break;
   case 4:c->c=false;s->openBus=0x18;break;
   case 5:{unsigned a=c->a,sum=a+33u+(unsigned)c->c;c->a=(uint16_t)sum;c->c=sum>65535u;c->v=((~(a^33u)&(a^sum)&0x8000u)!=0);c->n=(c->a&0x8000u)!=0;c->z=c->a==0;s->openBus=0;break;}
   case 6:ta_r108_store(s->ram+operand,c->a);s->openBus=(uint8_t)(c->a>>8);break;
   case 7:v=ta_r108_word(s->ram+c->sp+1u);c->sp+=2;c->x=v;c->n=(v&0x8000u)!=0;c->z=v==0;s->openBus=(uint8_t)(v>>8);break;
   default:c->x--;c->n=(c->x&0x8000u)!=0;c->z=c->x==0;s->openBus=0xca;break;
  }
  c->pc=(uint16_t)(pc+bytes);h+=clocks;++run;
 }
 done:if(run){
#if defined(TA_R126_HOST_COVERAGE) && TA_R126_HOST_COVERAGE
 ++ta_r126PrefixCalls;ta_r126PrefixOps+=run;
#endif
 const unsigned elapsed=h-initial;s->hPos=(uint16_t)h;s->cycles+=elapsed;s->apuMasterPending+=elapsed;
  if(s->autoJoyTimer)s->autoJoyTimer=elapsed>=s->autoJoyTimer?0:(uint16_t)(s->autoJoyTimer-elapsed);}
 return run;
}
#endif

