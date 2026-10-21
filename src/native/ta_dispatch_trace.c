#include "ta_dispatch_trace.h"
#if TA_R60_DISPATCH_TRACE
#include "snes.h"
#include "ta_bus_end_limit.h"
#include <stdio.h>
#include <string.h>
/* Global collector is for one frontend-owned Snes instance. Host A/B runs
 * reset it between instances. Counters saturate; detail is first-observation
 * evidence, NOT a representative frequency/time sample. No emulated reads. */
#define CAP 128u
static const uint32_t sites[16]={0x86b70c,0x86b73e,0x86b743,0x86b752,
 0x86b761,0x86b770,0x86b77f,0x85851d,0x86b720,0x858557,0x858565,
 0x858573,0x85858f,0x85859d,0x8585b0,0x8585b6};
static const uint16_t tables[8]={0xb724,0x871a,0x872a,0x873a,0x875a,0x87a2,0x8eab,0x8ef5};
typedef struct {
 uint32_t pc,frame,wr,pending; uint64_t cycles,stolen; int64_t debt;
 uint16_t a,x,y,sp,dp,h,v,fence,joy,base,state,ptr;
 uint8_t db,p,e,bus,ints,dma,timers; bool ptrValid;
} Snap;
typedef struct {
 unsigned route; uint16_t index,target,ret,sp; uint8_t bank;
 bool complete,printed; Snap before,entry,after;
} Record;
static uint32_t visits[16],counts[8][256],unknown,dropped,overlap;
static uint16_t targets[8][256];
static bool sampled[8][256],siteCaptured[16],sitePrinted[16];
static Snap siteSamples[16];
static Record records[CAP]; static unsigned used;
static int active=-1; static Snap last;
static const Cpu* owner; static uint64_t lastClock; static bool haveClock;
static uint32_t hash(const uint8_t* p,unsigned n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
static uint16_t word(const Snes* s,unsigned a){return s->ram[a]|((uint16_t)s->ram[a+1]<<8);}
static void inc(uint32_t* p){if(*p!=0xffffffffu)++*p;}
static Snap snap(const Cpu* c,bool full){
 const Snes* s=(const Snes*)c->mem; Snap t;memset(&t,0,sizeof t);
 t.pc=((uint32_t)c->k<<16)|c->pc;t.frame=s->frames;
 t.a=c->a;t.x=c->x;t.y=c->y;t.sp=c->sp;t.dp=c->dp;t.db=c->db;
 t.p=c->c|(c->z<<1)|(c->i<<2)|(c->d<<3)|(c->xf<<4)|(c->mf<<5)|(c->v<<6)|(c->n<<7);t.e=c->e;
 t.ints=c->irqWanted|(c->nmiWanted<<1)|(c->intWanted<<2)|(c->resetWanted<<3);
 t.h=s->hPos;t.v=s->vPos;t.cycles=s->cycles;t.stolen=s->stolenCycles;
 t.pending=s->apuMasterPending;t.debt=s->apuCycleDebtNumerator;t.bus=s->openBus;
 t.fence=s->hPos<1358?ta_r41_busEndLimit[s->hPos]:0;t.joy=s->autoJoyTimer;
 t.dma=s->inDma|(!!s->dma->dmaState<<1)|(s->dma->hdmaInitRequested<<2)|(s->dma->hdmaRunRequested<<3);
 t.timers=s->hIrqEnabled|(s->vIrqEnabled<<1)|(s->irqCondition<<2);
 /* Raw WRAM clues, valid low mirror only with DB=$85/$86 and DP=0.
  * The final Y has already been transformed at a JSR; preserve actual DP
  * pointer bytes without reading the emulated bus or guessing provenance. */
 t.base=word(s,0x3f2);t.state=word(s,0x354);
 if(c->dp<=0x1fd9){t.ptr=word(s,c->dp+0x24);t.ptrValid=true;}
 if(full)t.wr=hash(s->ram,sizeof s->ram);
 return t;
}
void ta_r60_trace_reset(void){memset(visits,0,sizeof visits);memset(counts,0,sizeof counts);memset(targets,0,sizeof targets);memset(records,0,sizeof records);memset(sampled,0,sizeof sampled);memset(siteCaptured,0,sizeof siteCaptured);memset(sitePrinted,0,sizeof sitePrinted);used=unknown=dropped=overlap=0;active=-1;owner=NULL;haveClock=false;}
void ta_r60_trace_before(const Cpu* c){
 if(!c||!c->mem||c->waiting||c->stopped||c->resetWanted||c->intWanted)return;
 const Snes* s=(const Snes*)c->mem;
 if(owner && owner!=c){inc(&unknown);return;} owner=c;
 if(haveClock&&lastClock==s->cycles)return;lastClock=s->cycles;haveClock=true;
 uint32_t pc=((uint32_t)c->k<<16)|c->pc;
 if(active>=0){Record* r=&records[active];if(c->k==r->bank&&c->pc==r->ret&&c->sp==r->sp){r->after=snap(c,true);r->complete=true;active=-1;}}
 for(unsigned i=0;i<16;++i)if(pc==sites[i]){inc(&visits[i]);last=snap(c,false);if(!siteCaptured[i]){siteSamples[i]=snap(c,true);siteCaptured[i]=true;}if(i>=8 && !(c->x&1) && c->x<=510 && !sampled[i-8][c->x/2] && used<CAP && active<0)last.wr=hash(s->ram,sizeof s->ram);return;}
}
void ta_r60_trace_jsr(const Cpu* c,uint16_t site,uint16_t index){
 unsigned route=8;
 for(unsigned i=8;i<16;++i)if(((uint32_t)c->k<<16|site)==sites[i]){route=i-8;break;}
 if(route==8)return;
 /* Report indices outside the A&FF-derived even domain as unknown, not
  * as a valid table call. Even indices are still NOT verified table lengths. */
 if((index&1)||index>510){inc(&unknown);return;}
 unsigned ix=index/2; bool first=!counts[route][ix];inc(&counts[route][ix]);
 if(!first && targets[route][ix]!=c->pc){inc(&unknown);return;}
 targets[route][ix]=c->pc;
 if(route==0 || sampled[route][ix])return;
 if(used==CAP){inc(&dropped);return;}
 if(active>=0){inc(&overlap);return;} /* Do not overwrite an unfinished sample. */
 Record* r=&records[used];r->route=route;r->index=index;r->target=c->pc;
 r->bank=c->k;r->ret=(uint16_t)(site+3);r->sp=(uint16_t)(c->sp+2);
 r->before=last;
 r->entry=snap(c,true);r->before.wr=last.wr;sampled[route][ix]=true;active=(int)used++;
}
static void dump(void (*emit)(const char*),unsigned id,const char* phase,const Snap* s){
 char line[384];snprintf(line,sizeof line,"[TA R60 SNAP] id=%u phase=%s frame=%lu pc=%06lX a=%04X x=%04X y=%04X sp=%04X dp=%04X db=%02X p=%02X e=%u ints=%u bus=%02X wr=%08lX\n",id,phase,(unsigned long)s->frame,(unsigned long)s->pc,s->a,s->x,s->y,s->sp,s->dp,s->db,s->p,s->e,s->ints,s->bus,(unsigned long)s->wr);emit(line);
 snprintf(line,sizeof line,"[TA R60 CLOCK] id=%u phase=%s cycles=%llu stolen=%llu h=%u v=%u fence=%u joy=%u pending=%lu debt=%lld dma=%u timers=%u base=%04X state=%04X ptr=%04X ptr_valid=%u\n",id,phase,(unsigned long long)s->cycles,(unsigned long long)s->stolen,s->h,s->v,s->fence,s->joy,(unsigned long)s->pending,(long long)s->debt,s->dma,s->timers,s->base,s->state,s->ptr,s->ptrValid);emit(line);
}
void ta_r60_trace_report(void (*emit)(const char*)){
 if(!emit)return;char line[384];
 snprintf(line,sizeof line,"[TA R60 TRACE] detail=%u cap=%u unknown=%lu dropped=%lu overlap=%lu fps_not_comparable=1 counters=cumulative sample=first_pair\n",used,CAP,(unsigned long)unknown,(unsigned long)dropped,(unsigned long)overlap);emit(line);
 for(unsigned i=0;i<16;++i){snprintf(line,sizeof line,"[TA R60 SITE] pc=%06lX visits=%lu\n",(unsigned long)sites[i],(unsigned long)visits[i]);emit(line);}
 for(unsigned r=0;r<8;++r)for(unsigned i=0;i<256;++i)if(counts[r][i]){
  snprintf(line,sizeof line,"[TA R60 CALL] route=%u table=%02X:%04X x=%u index=%u target=%02X:%04X calls=%lu length=unverified\n",r,r?0x85:0x86,tables[r],2*i,i,r?0x85:0x86,targets[r][i],(unsigned long)counts[r][i]);emit(line);
 }
 for(unsigned i=0;i<16;++i)if(siteCaptured[i]&&!sitePrinted[i]){sitePrinted[i]=true;dump(emit,1000+i,"site",&siteSamples[i]);}
 for(unsigned i=0;i<used;++i){Record* r=&records[i];if(r->printed || !r->complete)continue;r->printed=true;snprintf(line,sizeof line,"[TA R60 PAIR] id=%u route=%u x=%u target=%02X:%04X return=%02X:%04X complete=%u\n",i,r->route,r->index,r->bank,r->target,r->bank,r->ret,r->complete);emit(line);dump(emit,i,"pre",&r->before);dump(emit,i,"entry",&r->entry);if(r->complete)dump(emit,i,"return",&r->after);}
}
#endif
