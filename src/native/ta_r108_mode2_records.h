/* R108: exact-ROM native record traversal at $88:82ED..8355.
 * Active JSL/indirect drawing callbacks and mode-6 helpers remain interpreted.
 * Segments end at original opcode boundaries before every raster/timer event. */
#if defined(TA_R108_MODE2_RECORDS) && TA_R108_MODE2_RECORDS
#if defined(TA_R108_RECORD_PROFILE) && TA_R108_RECORD_PROFILE
static uint64_t s_r108RecordOps[5],s_r108RecordMode2Ops;
void ta_native_r108_record_counts(uint64_t out[6]){
 for(unsigned i=0;i<5;++i)out[i]=s_r108RecordOps[i];out[5]=s_r108RecordMode2Ops;
}
#endif
static inline uint16_t ta_r108_word(const uint8_t*p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static inline void ta_r108_store(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
unsigned ta_native_r108_records(Cpu*c,unsigned budget){
 /* R115 diagnostic classifiers do not relax any old R108 guard. */
 if(!c || !s_identical_rom || !s_r60ShaVerified){TA_R115_R108_REJECT(0);return 0;}
 if(c->k!=0x88 || c->db!=0x88){TA_R115_R108_REJECT(1);return 0;}
 if(c->dp || c->e || c->mf || c->xf || c->d){TA_R115_R108_REJECT(2);return 0;}
 if(c->waiting || c->stopped || c->resetWanted || c->intWanted ||
    c->nmiWanted || (c->irqWanted&&!c->i)){TA_R115_R108_REJECT(3);return 0;}
 Snes*s=(Snes*)c->mem;
 if(!s || s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
    s->dma->hdmaInitRequested || s->dma->hdmaRunRequested || s->irqCondition){TA_R115_R108_REJECT(4);return 0;}
 const unsigned initial=s->hPos,fetch=s->fastMem?6u:8u;
 if((initial&1u)||initial>=1358u){TA_R115_R108_REJECT(5);return 0;}
 const unsigned limit=ta_r41_busEndLimit[initial];unsigned h=initial,run=0;
 unsigned r115RejectReason=11u;
 while(run<budget){
  const unsigned pc=c->pc,sp=c->sp;unsigned group,ops,clocks;
  uint16_t value=0,index=c->x,ret=0,mode=0,pointer=0,saved=0;unsigned sum=0;bool zero=false;
  switch(pc){
   case 0x82ed:case 0x82ee:case 0x82f2:case 0x82f3:case 0x82f7:case 0x82fa:
    group=0;
    if(pc==0x82ed && (sp<0x102u||sp>0x1fffu)){r115RejectReason=7;goto done;}
    if(pc<=0x82ee)index=ta_r108_word(s->ram+0x933b);
    else if(pc==0x82f2)index=c->a;
    value=pc<=0x82f3?ta_r108_word(s->ram+0x90b0u+index):c->a;
    value&=0xffu;zero=pc==0x82fa?c->z:value==0;
    if(pc==0x82ed){ops=6;clocks=15u*fetch+60u;}
    else if(pc==0x82ee){ops=5;clocks=14u*fetch+38u;}
    else if(pc==0x82f2){ops=4;clocks=10u*fetch+22u;}
    else if(pc==0x82f3){ops=3;clocks=9u*fetch+16u;}
    else if(pc==0x82f7){ops=2;clocks=5u*fetch;}
    else{ops=1;clocks=2u*fetch;}
    if(zero)clocks+=6u;break;
   case 0x8303:case 0x8307:case 0x8308:case 0x830c:case 0x830f:
    group=1;
    if(pc==0x8303)index=ta_r108_word(s->ram+0x933b);
    else if(pc==0x8307)index=c->a;
    value=pc<=0x8308?ta_r108_word(s->ram+0x90afu+index):c->a;
    value&=0xffu;zero=pc==0x830f?c->z:value==0;
    if(pc==0x8303){ops=5;clocks=14u*fetch+38u;}
    else if(pc==0x8307){ops=4;clocks=10u*fetch+22u;}
    else if(pc==0x8308){ops=3;clocks=9u*fetch+16u;}
    else if(pc==0x830c){ops=2;clocks=5u*fetch;}
    else{ops=1;clocks=2u*fetch;}
    if(zero)clocks+=6u;break;
   case 0x8300:case 0x832f:case 0x833b:case 0x8355:
    group=2;
    if(pc==0x8300){
     if(sp<0x102u||sp>0x1fffu || (sp>=0x2bau&&sp<=0x2bcu)){r115RejectReason=7;goto done;}
     ret=0x8302;
    }else{
     if(sp<0x100u||sp>0x1ffdu){r115RejectReason=7;goto done;}
     ret=ta_r108_word(s->ram+sp+1u);
    }
    if(pc<=0x832f)mode=ta_r108_word(s->ram+0x2ba);
    if(pc!=0x8355 && mode!=6)value=ta_r108_word(s->ram+0x90b3u+c->x);
    if(pc==0x8355){ops=1;clocks=fetch+34u;}
    else if(pc<=0x832f && mode==6){ops=3;clocks=8u*fetch+16u;}
    else{ops=value?2u:3u;clocks=value?6u*fetch+16u:7u*fetch+56u;
     if(pc<=0x832f){ops+=3u;clocks+=8u*fetch+22u;}
    }
    if(pc==0x8300){++ops;clocks+=3u*fetch+22u;}
    break;
   case 0x8315:case 0x8319:case 0x831a:case 0x831d:
   case 0x8321:case 0x8323:case 0x8324:case 0x8327:case 0x8329:case 0x832a:case 0x832b:
    group=3;
    if(pc<=0x8329){
     if(sp<0x100u||sp>0x1ffdu){r115RejectReason=7;goto done;}
     saved=(uint16_t)(ta_r108_word(s->ram+sp+1u)-1u);
    }else saved=pc==0x832a?(uint16_t)(c->x-1u):c->x;
    zero=pc==0x832b?c->z:saved==0;
    if(pc<=0x831d){
     value=pc==0x8315?ta_r108_word(s->ram+0x933b):c->a;
     if(pc<0x831d)value=(uint16_t)(value+33u+(pc==0x831a&&c->c?1u:0u));
    }
    if(pc<=0x8327){
     pointer=pc<=0x8321?ta_r108_word(s->ram+0x96):c->a;
     sum=pc==0x8327?pointer:(unsigned)pointer+33u+(pc==0x8324&&c->c?1u:0u);
    }
    switch(pc){
     case 0x8315:ops=11;clocks=24u*fetch+110u;break;
     case 0x8319:ops=10;clocks=20u*fetch+94u;break;
     case 0x831a:ops=9;clocks=19u*fetch+88u;break;
     case 0x831d:ops=8;clocks=16u*fetch+88u;break;
     case 0x8321:ops=7;clocks=12u*fetch+72u;break;
     case 0x8323:ops=6;clocks=10u*fetch+56u;break;
     case 0x8324:ops=5;clocks=9u*fetch+50u;break;
     case 0x8327:ops=4;clocks=6u*fetch+50u;break;
     case 0x8329:ops=3;clocks=4u*fetch+34u;break;
     case 0x832a:ops=2;clocks=3u*fetch+6u;break;
     default:ops=1;clocks=2u*fetch;break;
    }
    if(!zero)clocks+=6u;break;
   default:r115RejectReason=6;goto done;
  }
  const unsigned end=h+clocks;
  if(ops>budget-run){r115RejectReason=8;break;}
  if(end>limit){r115RejectReason=9;break;}
  if(!ta_r49_noTimerEdge(s,h,end)){r115RejectReason=10;break;}
  if(group==0){
   if(pc==0x82ed){s->ram[sp]=(uint8_t)(c->x>>8);s->ram[sp-1u]=(uint8_t)c->x;c->sp=(uint16_t)(sp-2u);}
   if(pc<=0x82f2)c->x=index;
   if(pc!=0x82fa){c->a=value;c->n=false;c->z=zero;}
   c->pc=zero?0x8300:0x82fc;s->openBus=4;
  }else if(group==1){
   if(pc<=0x8307)c->x=index;
   if(pc!=0x830f){c->a=value;c->n=false;c->z=zero;}
   c->pc=zero?0x8315:0x8311;s->openBus=4;
  }else if(group==2){
   const bool mode6=pc<=0x832f&&mode==6;
   const bool returning=pc==0x8355||(!mode6&&!value);
   if(pc==0x8300){s->ram[sp]=0x83;s->ram[sp-1u]=2;if(!returning)c->sp=(uint16_t)(sp-2u);}
   else if(returning)c->sp=(uint16_t)(sp+2u);
   if(pc!=0x8355){c->a=mode6?mode:value;c->n=(c->a&0x8000u)!=0;c->z=c->a==0;if(pc<=0x832f)c->c=mode>=6u;}
   /* A mode-6 CMP leaves Z set even though A is six. */
   if(mode6){c->z=true;c->n=false;}
   c->pc=returning?(uint16_t)(ret+1u):mode6?0x8337:0x8341;
   s->openBus=returning?(uint8_t)(ret>>8):mode6?4:0x14;
  }else{
   if(pc<=0x831d)ta_r108_store(s->ram+0x933b,value);
   if(pc<=0x8327){
    if(pc<=0x8324){c->a=(uint16_t)sum;c->c=sum>65535u;c->v=(~(pointer^33u)&(pointer^sum)&0x8000u)!=0;}
    ta_r108_store(s->ram+0x96,c->a);
   }
   if(pc<=0x8329)c->sp=(uint16_t)(sp+2u);
   if(pc<=0x832a){c->x=saved;c->n=(saved&0x8000u)!=0;c->z=zero;}
   c->pc=zero?0x832d:0x82ed;s->openBus=0xc0;
  }
  h=end;run+=ops;
#if defined(TA_R108_RECORD_PROFILE) && TA_R108_RECORD_PROFILE
  s_r108RecordOps[group]+=ops;if(s->ppu->mode==2)s_r108RecordMode2Ops+=ops;
#endif
 }
 done:
 if(!run){TA_R115_R108_REJECT(r115RejectReason);return 0;}
 const unsigned clocks=h-initial;s->hPos=(uint16_t)h;s->cycles+=clocks;s->apuMasterPending+=clocks;
 if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
 return run;
}
#endif
