/* Included only by ta_native.c. Translation of observed $80:C0C8..C1F7.
 * Each segment models original bus clocks and flags; no guest opcode dispatch.
 * Rejected segments are inert. PLB/PLP/RTL and event crossings stay reference. */
#if TA_R80_SPRITE_ON
#if TA_R80_COVERAGE
static uint32_t s_r80Stages[8],s_r80Calls;static uint64_t s_r80Ops;
void ta_native_r80_counts(uint32_t stages[8],uint32_t* calls,uint64_t* ops){for(unsigned i=0;i<8;++i)if(stages)stages[i]=s_r80Stages[i];if(calls)*calls=s_r80Calls;if(ops)*ops=s_r80Ops;}
#endif
unsigned ta_native_r80_sprite(Cpu* c,unsigned budget){
 if(!c||!s_identical_rom||!s_r60ShaVerified||c->k!=0x80||c->db!=0x86||c->dp||c->e||c->mf||c->xf||c->d||(c->x&3u)||c->x>0x1fcu||c->y<0x8000||c->y>0xfffb||c->waiting||c->stopped||c->resetWanted||c->intWanted||c->nmiWanted||(c->irqWanted&&!c->i))return 0;
 Snes* s=c->mem;if(!s||!s->cart||!s->cart->rom||s->cart->romSize!=1048576u||s->readHook||s->writeHook||s->inDma||s->dma->dmaState||s->dma->hdmaInitRequested||s->dma->hdmaRunRequested)return 0;
 const unsigned initial=s->hPos,fetch=s->fastMem?6u:8u;
 if((initial&1u)||initial>=1358u||s->irqCondition)return 0;
 unsigned limit=ta_r41_busEndLimit[initial];
 if(s->hIrqEnabled||s->vIrqEnabled){if(s->autoJoyTimer&1u)return 0;if(!s->vIrqEnabled||s->vPos==s->vTimer){if(!s->hIrqEnabled)return 0;unsigned edge=(unsigned)s->hTimer*4u;if(edge>=initial&&edge<limit)limit=edge;}}
 if(limit<initial)return 0;unsigned h=initial,run=0;
 while(c->x<=0x1fcu&&c->y>=0x8000&&c->y<=0xfffb){
  const unsigned entry=c->pc,x=c->x;const uint8_t* src=s->cart->rom+0x30000u+(c->y&0x7fffu);unsigned ops,clocks,stage;
  if(entry==0xc0c8||entry==0xc0cd||entry==0xc0fb){
   const uint16_t raw=r62_word(src),delta=r62_word(s->ram+0xcf);
   const bool big=entry==0xc0fb||(entry==0xc0c8&&(raw&0x8000u)),flip=(r62_word(s->ram+0xde)&0x4000u)!=0;
   const uint16_t ordinate=flip?(uint16_t)((big?0xfff0u:0xfff8u)-raw):raw;const uint32_t sum=(uint32_t)ordinate+delta;
   stage=0;ops=flip?9u:6u;clocks=flip?21u*fetch+56u:15u*fetch+50u;
   if(entry==0xc0c8){ops+=2;clocks+=7u*fetch+6u+(big?6u:0u);}
   if(ops>budget-run||clocks>limit-h)break;
   c->a=(uint16_t)sum;c->c=sum>0xffffu;c->v=((~(ordinate^delta)&(ordinate^sum)&0x8000u)!=0);c->n=(c->a&0x8000u)!=0;c->z=c->a==0;
   c->pc=big?0xc111:0xc0e3;s->openBus=(uint8_t)(delta>>8);
  }else if(entry==0xc0e3||entry==0xc111){
   const bool big=entry==0xc111,bit=(c->a&0x100u)!=0;stage=1;ops=3;clocks=8u*fetch+22u+(bit?0u:6u);
   if(ops>budget-run||clocks>limit-h)break;
   r62_store(s->ram+0x1d00u+x,c->a);c->z=!bit;c->pc=big?(bit?0xc119:0xc129):(bit?0xc0eb:0xc139);s->openBus=big?0x10:0x4e;
  }else if(entry==0xc0eb||entry==0xc119||entry==0xc129){
   const bool clear=entry==0xc129,small=entry==0xc0eb;stage=2;ops=6;clocks=20u*fetch+86u;
   if(ops>budget-run||clocks>limit-h)break;
   const uint16_t pointer=r62_word(s->cart->rom+0x41feu+x);if(pointer<0x1f00||pointer>0x1f1e)break;
   r62_store(s->ram+0xe0,pointer);c->a=r62_word(s->ram+pointer)|r62_word(s->cart->rom+(clear?0x43feu:small?0x4200u:0x4400u)+x);r62_store(s->ram+pointer,c->a);
   c->n=(c->a&0x8000u)!=0;c->z=c->a==0;c->pc=small?0xc139:0xc181;s->openBus=clear?0x48:small?0x3e:0x58;
  }else if(entry==0xc139||entry==0xc181){
   const bool big=entry==0xc181,flip=(r62_word(s->ram+0xde)&0x8000u)!=0;const uint16_t raw=r62_word(src+2);const bool neg=(raw&0x80u)!=0;
   const uint16_t ordinate=neg?(raw|0xff80u):(raw&0x7fu);const uint32_t sum=(uint32_t)ordinate+(big?16u:8u);
   stage=3;ops=(neg?7u:8u)+(flip?5u:0u);clocks=(20u+(neg?0u:2u)+(flip?10u:0u))*fetch+(flip?46u:34u);
   if(ops>budget-run||clocks>limit-h)break;
   c->a=flip?(uint16_t)(0u-sum):ordinate;if(flip){c->c=sum>0xffffu;c->v=((~(ordinate^(big?16u:8u))&(ordinate^sum)&0x8000u)!=0);}
   c->n=(c->a&0x8000u)!=0;c->z=c->a==0;c->pc=big?0xc1b2:0xc16a;s->openBus=flip?0x10:neg?0xff:0x03;
  }else if(entry==0xc16a||entry==0xc1b2){
   const uint16_t ordinate=c->a,delta=r62_word(s->ram+0xd1),value=(uint16_t)(ordinate+delta);const uint32_t sum=(uint32_t)value+16u;const uint16_t check=(uint16_t)sum;const bool clip=check>=0x100u;
   stage=4;ops=clip?10u:8u;clocks=(clip?23u:17u)*fetch+(clip?78u:62u);
   if(ops>budget-run||clocks>limit-h)break;
   r62_store(s->ram+0x1d01u+x,value);if(clip)r62_store(s->ram+0x1d01u+x,0xf0);
   c->a=clip?0xf0:check;c->c=clip;c->v=((~(value^16u)&(value^sum)&0x8000u)!=0);c->n=clip?false:(((uint16_t)(check-0x100u)&0x8000u)!=0);c->z=clip?false:check==0x100u;
   c->pc=0xc1c9;s->openBus=entry==0xc16a?0x48:0x00;
  }else if(entry==0xc1c9){
   stage=5;ops=6;clocks=16u*fetch+44u;if(ops>budget-run||clocks>limit-h)break;
   const uint16_t tile=r62_word(src+3)&0x1ffu,delta=r62_word(s->ram+0xdc);const uint32_t sum=(uint32_t)tile+delta;
   c->a=sum&0x1ffu;r62_store(s->ram+0xe0,c->a);c->c=sum>0xffffu;c->v=((~(tile^delta)&(tile^sum)&0x8000u)!=0);c->n=false;c->z=c->a==0;c->pc=0xc1d7;s->openBus=(uint8_t)(c->a>>8);
  }else if(entry==0xc1d7){
   stage=6;ops=7;clocks=19u*fetch+92u;if(ops>budget-run||clocks>limit-h)break;
   c->a=(((r62_word(src+3)&0xfe00u)&r62_word(s->ram+0xd8))|r62_word(s->ram+0xda)|r62_word(s->ram+0xe0))^r62_word(s->ram+0xde);
   r62_store(s->ram+0x1d02u+x,c->a);c->n=(c->a&0x8000u)!=0;c->z=c->a==0;c->pc=0xc1e8;s->openBus=(uint8_t)(c->a>>8);
  }else if(entry==0xc1e8){
   const uint16_t count=(uint16_t)(r62_word(s->ram+0xe4)-1u);stage=7;ops=count?12u:11u;clocks=(count?16u:13u)*fetch+(count?92u:98u);
   if(ops>budget-run||clocks>limit-h)break;
   c->y=(uint16_t)(c->y+5u);c->x=(uint16_t)(x+4u);s->ram[0xe5]=(uint8_t)(count>>8);s->ram[0xe4]=(uint8_t)count;c->n=(count&0x8000u)!=0;c->z=count==0;c->pc=count?0xc0c8:0xc1f8;s->openBus=count?0xc0:0x03;
  }else break;
  h+=clocks;run+=ops;
#if TA_R80_COVERAGE
  if(s_r80Stages[stage]!=UINT32_MAX)++s_r80Stages[stage];s_r80Ops+=ops;
#endif
 }
 if(!run)return 0;unsigned clocks=h-initial;s->hPos=(uint16_t)h;s->cycles+=clocks;s->apuMasterPending+=clocks;if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if TA_R80_COVERAGE
 if(s_r80Calls!=UINT32_MAX)++s_r80Calls;
#endif
 return run;
}
#endif
