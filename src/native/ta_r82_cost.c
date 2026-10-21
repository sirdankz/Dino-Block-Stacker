#include "ta_r82_cost.h"
#if TA_R82_COST_ON
#include "snes.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
bool ta_r82_active;
static uint64_t closed82,frameSerial,clockFloor;static uint32_t lastFrame;static unsigned startMode;
static TaR82ModeCost current82,modes82[3];static TaR82Token root82;
static uint64_t invalidClock,stackOverflow,exceptionOverflow,resetScopes,unknownReturns;
static const uint32_t unitEntries[TA82_UNITS]={0x89a366,0x80bbaf,0x80bcff,0x80c08d,0x82d9d4,0x84c12d,0x809086,0x80bd65,0x80bdeb,0x80be85,0x80bf1c,0x80bfc4,0x86b70c,0x85851d};
static const uint8_t returnBytes[TA82_UNITS]={3,3,3,3,2,3,3,2,2,2,2,3,2,3};
static const char* unitNames[TA82_UNITS]={"animation_and_callbacks","simple_sprite","mode2_sprite_pipeline","mode1_sprite_pipeline","six_field_scan","frame_service","quiet_service","mode2_no_flip","mode2_vertical_flip","mode2_horizontal_flip","mode2_both_flip","alternate_sprite","task_dispatch","indirect_handlers"};
typedef struct {uint64_t inclusive[3],exclusive[3],chunks[3];uint32_t starts,completed,mismatch;} Unit82;
typedef struct {uint32_t pc,starts,completed,mismatch;uint64_t ticks[3],exclusive[3];uint16_t dp,spmin,spmax;uint8_t db,mxed,changed;} Callback82;
typedef struct {uint32_t key;uint64_t ticks[3],chunks[3];} Page82;
static Unit82 units82[TA82_UNITS];static Callback82 callbacks82[TA82_CALLBACKS];static Page82 pages82[TA82_PAGES];
static uint64_t callbackOverflow,pageOverflow;
#if TA_R87_CACHE_MAP
/* R87.1: exact, bounded 16-page census. R87's unrestricted shared
 * 4096-PC hash table became saturated on the Xbox and its 4096-probe
 * miss path overwhelmed the rotating observer. Each selected page now
 * has exactly 256 direct-index slots: no hashing, search, or overflow.
 * Other reference PCs are explicitly charged to filtered_ref_ticks.
 * This is a FOCUSED map, not a census of every reference opcode PC. */
#define TA87_REF_SLOTS 4096u
#define TA87_FOCUSED_PAGES 16u
static const uint16_t refPages87[TA87_FOCUSED_PAGES]={
 0x809a,0x809b,0x89a3,0x89a4,0x8481,0x84c1,0x80bd,0x80bc,
 0x86b7,0x86b6,0x80c1,0x80c0,0x80bb,0x82da,0x80a5,0x8094
};
typedef struct {uint32_t pc_plus_one;uint64_t ticks[3],chunks[3];} Ta87RefPc;
static Ta87RefPc ref87[TA87_REF_SLOTS];
static uint64_t frameRefTicks[TA87_REF_SLOTS],frameRefChunks[TA87_REF_SLOTS];
static uint64_t filteredRefTicks87[3],filteredRefChunks87[3];
static uint64_t frameFilteredRefTicks87,frameFilteredRefChunks87;
static uint64_t events87[3][TA87_EVENTS],frameEvents87[TA87_EVENTS];
void ta_r87_cache_event(unsigned event) {if(ta_r82_active && event<TA87_EVENTS) ++frameEvents87[event];}
static unsigned refPcSlot87(uint32_t pc) {
 const uint16_t page=(uint16_t)(pc>>8);
 for(unsigned i=0;i<TA87_FOCUSED_PAGES;++i)if(page==refPages87[i]){
  const unsigned slot=(i<<8)|(pc&255u);
  ref87[slot].pc_plus_one=pc+1u;
  return slot;
 }
 return TA87_REF_SLOTS;
}
#endif
#if TA_R83_RESIDUAL_PROFILE
enum {TA83_SERVICE,TA83_OTHER_NATIVE,TA83_REFERENCE,TA83_DISPATCH_KINDS};
typedef struct {
 uint64_t ticks[TA83_DISPATCH_KINDS],chunks[TA83_DISPATCH_KINDS];
 uint64_t service_ticks[TA83_DISPATCH_KINDS],service_chunks[TA83_DISPATCH_KINDS];
 uint64_t service_accept,service_reject;
} Residual83;
static Residual83 frame83, mode83[3];
static bool r83_dispatch_native, r83_service_success;
static uint32_t r83_step_accept, r83_step_reject;
void ta_r83_cpu_native_done(void){if(ta_r82_active)r83_dispatch_native=true;}
void ta_r83_service_attempt(bool accepted){
 if(!ta_r82_active)return;
 if(accepted){r83_service_success=true;++r83_step_accept;}
 else ++r83_step_reject;
}
#endif
typedef struct {int unit,callback;uint32_t entry,expected;uint16_t sp;} Scope82;
static Scope82 scopes82[16];static unsigned depth82;
static struct {uint16_t sp;unsigned baseDepth;} exceptions82[8];static unsigned exceptionDepth;
static uint64_t frameUnitTicks[TA82_UNITS],frameUnitExclusive[TA82_UNITS],frameUnitChunks[TA82_UNITS];
static uint64_t frameCallbackTicks[TA82_CALLBACKS],frameCallbackExclusive[TA82_CALLBACKS];
static uint64_t framePageTicks[TA82_PAGES],framePageChunks[TA82_PAGES];
#ifdef TA_R82_CLOCK_TEST
extern uint64_t ta_r82_test_clock(void);
static uint64_t clock82(void){return ta_r82_test_clock();}
#else
static inline uint64_t clock82(void){__asm__ __volatile__("" ::: "memory");uint64_t t=__builtin_readcyclecounter();__asm__ __volatile__("" ::: "memory");return t;}
#endif
TaR82Token ta_r82_scope_begin(unsigned kind){TaR82Token t={0,0,kind,false};if(ta_r82_active){t.closed=closed82;t.start=clock82();t.active=true;}return t;}
uint64_t ta_r82_scope_finish(TaR82Token* t){if(!t||!t->active)return 0;t->active=false;uint64_t now=clock82(),child=closed82-t->closed,elapsed=now-t->start;
 if(now<t->start||child>elapsed){++invalidClock;return 0;}uint64_t own=elapsed-child;closed82+=own;current82.ticks[t->kind]+=own;++current82.calls[t->kind];return own;}
void ta_r82_reset(void){ta_r82_active=false;closed82=frameSerial=0;lastFrame=0;depth82=exceptionDepth=0;invalidClock=stackOverflow=exceptionOverflow=resetScopes=unknownReturns=callbackOverflow=pageOverflow=0;
 memset(modes82,0,sizeof modes82);memset(units82,0,sizeof units82);
#if TA_R87_CACHE_MAP
 memset(ref87,0,sizeof ref87);memset(events87,0,sizeof events87);memset(filteredRefTicks87,0,sizeof filteredRefTicks87);memset(filteredRefChunks87,0,sizeof filteredRefChunks87);
#endif
#if TA_R83_RESIDUAL_PROFILE
 memset(mode83,0,sizeof mode83);memset(&frame83,0,sizeof frame83);
 r83_dispatch_native=r83_service_success=false;r83_step_accept=r83_step_reject=0;
#endif
 memset(callbacks82,0,sizeof callbacks82);memset(pages82,0,sizeof pages82);clockFloor=UINT64_MAX;
 for(unsigned i=0;i<64;++i){uint64_t a=clock82(),b=clock82();if(b>=a&&b-a<clockFloor)clockFloor=b-a;}if(clockFloor==UINT64_MAX)clockFloor=0;
}
void ta_r82_frame_begin(Snes* s){if(s->frames<lastFrame){depth82=exceptionDepth=0;++resetScopes;}lastFrame=s->frames;uint64_t call=frameSerial++;ta_r82_active=(call%60u)==((call/60u)%60u);
 if(!ta_r82_active)return;closed82=0;startMode=s->ppu->mode;memset(&current82,0,sizeof current82);
#if TA_R87_CACHE_MAP
 memset(frameEvents87,0,sizeof frameEvents87);
 memset(frameRefTicks,0,sizeof frameRefTicks);memset(frameRefChunks,0,sizeof frameRefChunks);frameFilteredRefTicks87=frameFilteredRefChunks87=0;
#endif
#if TA_R83_RESIDUAL_PROFILE
 memset(&frame83,0,sizeof frame83);
#endif
 memset(frameUnitTicks,0,sizeof frameUnitTicks);memset(frameUnitExclusive,0,sizeof frameUnitExclusive);memset(frameUnitChunks,0,sizeof frameUnitChunks);memset(frameCallbackTicks,0,sizeof frameCallbackTicks);memset(frameCallbackExclusive,0,sizeof frameCallbackExclusive);memset(framePageTicks,0,sizeof framePageTicks);memset(framePageChunks,0,sizeof framePageChunks);root82=ta_r82_scope_begin(TA82_OTHER);
}
void ta_r82_frame_end(Snes* s){if(!ta_r82_active)return;(void)ta_r82_scope_finish(&root82);unsigned mode=(startMode==s->ppu->mode&&startMode==1)?0u:(startMode==s->ppu->mode&&startMode==2)?1u:2u;
 TaR82ModeCost* m=&modes82[mode];++m->frames;m->rootTicks+=closed82;for(unsigned k=0;k<TA82_KINDS;++k){m->ticks[k]+=current82.ticks[k];m->calls[k]+=current82.calls[k];}
 for(unsigned i=0;i<TA82_UNITS;++i){units82[i].inclusive[mode]+=frameUnitTicks[i];units82[i].exclusive[mode]+=frameUnitExclusive[i];units82[i].chunks[mode]+=frameUnitChunks[i];}
 for(unsigned i=0;i<TA82_CALLBACKS;++i){callbacks82[i].ticks[mode]+=frameCallbackTicks[i];callbacks82[i].exclusive[mode]+=frameCallbackExclusive[i];}
 for(unsigned i=0;i<TA82_PAGES;++i){pages82[i].ticks[mode]+=framePageTicks[i];pages82[i].chunks[mode]+=framePageChunks[i];}
#if TA_R87_CACHE_MAP
 for(unsigned e=0;e<TA87_EVENTS;++e)events87[mode][e]+=frameEvents87[e];
 filteredRefTicks87[mode]+=frameFilteredRefTicks87;filteredRefChunks87[mode]+=frameFilteredRefChunks87;
 for(unsigned i=0;i<TA87_REF_SLOTS;++i)if(frameRefChunks[i]){
  ref87[i].ticks[mode]+=frameRefTicks[i];ref87[i].chunks[mode]+=frameRefChunks[i];
 }
#endif
#if TA_R83_RESIDUAL_PROFILE
 for(unsigned k=0;k<TA83_DISPATCH_KINDS;++k){
   mode83[mode].ticks[k]+=frame83.ticks[k];mode83[mode].chunks[k]+=frame83.chunks[k];
   mode83[mode].service_ticks[k]+=frame83.service_ticks[k];mode83[mode].service_chunks[k]+=frame83.service_chunks[k];
 }
 mode83[mode].service_accept+=frame83.service_accept;mode83[mode].service_reject+=frame83.service_reject;
#endif
 ta_r82_active=false;
}
void ta_r82_get_mode(unsigned mode,TaR82ModeCost* result){if(result&&mode<3)*result=modes82[mode];}
static uint32_t pc82(const Cpu* c){return ((uint32_t)c->k<<16)|c->pc;}
static uint32_t expected82(Snes* s,const Cpu* c,unsigned size){unsigned sp=c->sp;if(sp>0x1ffcu||sp<0x100u){++unknownReturns;return UINT32_MAX;}unsigned lo=s->ram[sp+1]|((unsigned)s->ram[sp+2]<<8),bank=size==3?s->ram[sp+3]:c->k;return ((uint32_t)bank<<16)|((lo+1u)&65535u);}
static void push82(Cpu* c,int unit,int callback,unsigned size){uint32_t pc=pc82(c);if(depth82&&scopes82[depth82-1].entry==pc&&scopes82[depth82-1].sp==c->sp&&scopes82[depth82-1].unit==unit&&scopes82[depth82-1].callback==callback)return;
 if(depth82==16){++stackOverflow;return;}Scope82* q=&scopes82[depth82++];q->unit=unit;q->callback=callback;q->entry=pc;q->sp=c->sp;q->expected=expected82(c->mem,c,size);
 if(unit>=0)++units82[unit].starts;if(callback>=0)++callbacks82[callback].starts;
}
static int callback82(Cpu* c){uint32_t pc=pc82(c);for(unsigned i=0;i<TA82_CALLBACKS;++i){Callback82* q=&callbacks82[i];if(q->pc==pc||!q->pc){uint8_t mx=c->mf|(c->xf<<1)|(c->e<<2)|(c->d<<3);if(!q->pc){q->pc=pc;q->dp=c->dp;q->db=c->db;q->mxed=mx;q->spmin=q->spmax=c->sp;}else{q->changed|=q->dp!=c->dp||q->db!=c->db||q->mxed!=mx;if(c->sp<q->spmin)q->spmin=c->sp;if(c->sp>q->spmax)q->spmax=c->sp;}return (int)i;}}++callbackOverflow;return -1;}
static unsigned page82(uint32_t pc){uint32_t key=(pc>>8)+1u;unsigned slot=(key*2654435761u)>>22;for(unsigned j=0;j<TA82_PAGES;++j){unsigned i=(slot+j)&(TA82_PAGES-1u);if(!pages82[i].key||pages82[i].key==key){pages82[i].key=key;return i;}}++pageOverflow;return TA82_PAGES;}
void ta_r82_cpu_step(Cpu* c,bool vblank,uint32_t frame){TaR82Token observer=ta_r82_scope_begin(TA82_OBSERVER);uint32_t pc=pc82(c);const bool entryInterrupt=c->intWanted;const uint16_t entrySp=c->sp;
 if(c->intWanted){if(exceptionDepth<8){exceptions82[exceptionDepth].sp=c->sp;exceptions82[exceptionDepth++].baseDepth=depth82;}else ++exceptionOverflow;}
 if(!entryInterrupt)for(unsigned i=0;i<TA82_UNITS;++i)if(pc==unitEntries[i]){push82(c,(int)i,-1,returnBytes[i]);break;}
 unsigned suspended=exceptionDepth?exceptions82[exceptionDepth-1].baseDepth:0u;
 (void)ta_r82_scope_finish(&observer);
#if TA_R83_RESIDUAL_PROFILE
 r83_dispatch_native=r83_service_success=false;r83_step_accept=r83_step_reject=0;
#endif
 TaR82Token cpu=ta_r82_scope_begin(TA82_CPU);cpu_runFrameOpcode(c,vblank,frame);uint64_t own=ta_r82_scope_finish(&cpu);
 observer=ta_r82_scope_begin(TA82_OBSERVER);
 if(ta_r82_active){
#if TA_R83_RESIDUAL_PROFILE
  const unsigned lane=r83_dispatch_native?(r83_service_success?TA83_SERVICE:TA83_OTHER_NATIVE):TA83_REFERENCE;
  frame83.ticks[lane]+=own;frame83.chunks[lane]++;
#if TA_R87_CACHE_MAP
  if(lane==TA83_REFERENCE){unsigned r=refPcSlot87(pc);
   if(r<TA87_REF_SLOTS){frameRefTicks[r]+=own;++frameRefChunks[r];}
   else {frameFilteredRefTicks87+=own;++frameFilteredRefChunks87;}}
#endif
  frame83.service_accept+=r83_step_accept;frame83.service_reject+=r83_step_reject;
  bool in_service=(pc==0x84c12du||pc==0x848162u||pc==0x84c13au);
  for(unsigned j=suspended;j<depth82;++j)if(scopes82[j].unit==5){in_service=true;break;}
  if(in_service){frame83.service_ticks[lane]+=own;frame83.service_chunks[lane]++;}
#endif
 unsigned page=page82(pc);if(page<TA82_PAGES){framePageTicks[page]+=own;++framePageChunks[page];}
  for(unsigned i=suspended;i<depth82;++i){Scope82* q=&scopes82[i];if(q->unit>=0){frameUnitTicks[q->unit]+=own;++frameUnitChunks[q->unit];}if(q->callback>=0)frameCallbackTicks[q->callback]+=own;}
  if(depth82>suspended){Scope82* q=&scopes82[depth82-1];if(q->unit>=0)frameUnitExclusive[q->unit]+=own;if(q->callback>=0)frameCallbackExclusive[q->callback]+=own;}
 }
 while(depth82&&c->sp>scopes82[depth82-1].sp){Scope82* q=&scopes82[--depth82];bool mismatch=q->expected!=UINT32_MAX&&q->expected!=pc82(c);if(q->unit>=0){++units82[q->unit].completed;units82[q->unit].mismatch+=mismatch;}if(q->callback>=0){++callbacks82[q->callback].completed;callbacks82[q->callback].mismatch+=mismatch;}}
 while(exceptionDepth&&c->sp>=exceptions82[exceptionDepth-1].sp)--exceptionDepth;
 if(pc==0x89a44du&&!entryInterrupt&&c->sp==entrySp&&pc82(c)!=pc){int cb=callback82(c);if(cb>=0)push82(c,-1,cb,3);}
 (void)ta_r82_scope_finish(&observer);
}
void ta_r82_report(void (*emit)(const char*),uint32_t endFrame){if(!emit)return;char line[1024];
 snprintf(line,sizeof line,"[TA R82 STATUS] end_frame=%u sample_period=60 rotating_phase=1 cumulative=1 frontend_frames=%llu timer_pair_floor=%llu invalid_clock=%llu scope_overflow=%llu exception_overflow=%llu callback_overflow=%llu page_overflow=%llu reset_scopes=%llu unknown_returns=%llu open_scopes=%u fps_not_comparable=1\n",endFrame,(unsigned long long)frameSerial,(unsigned long long)clockFloor,(unsigned long long)invalidClock,(unsigned long long)stackOverflow,(unsigned long long)exceptionOverflow,(unsigned long long)callbackOverflow,(unsigned long long)pageOverflow,(unsigned long long)resetScopes,(unsigned long long)unknownReturns,depth82);emit(line);
 for(unsigned mode=0;mode<3;++mode){TaR82ModeCost* m=&modes82[mode];if(!m->frames)continue;unsigned label=mode<2?mode+1:0;
  snprintf(line,sizeof line,"[TA R82 COST] end_frame=%u mode=%u samples=%llu total_ticks=%llu cpu_bus_ticks=%llu ppu_other_ticks=%llu bg_ticks=%llu sprite_eval_ticks=%llu sprite_comp_ticks=%llu apu_spc_ticks=%llu dsp_ticks=%llu dma_ticks=%llu other_ticks=%llu observer_ticks=%llu cpu_chunks=%llu ppu_calls=%llu dsp_calls=%llu cumulative=1 fps_not_comparable=1\n",endFrame,label,(unsigned long long)m->frames,(unsigned long long)m->rootTicks,(unsigned long long)m->ticks[TA82_CPU],(unsigned long long)m->ticks[TA82_PPU],(unsigned long long)m->ticks[TA82_BG],(unsigned long long)m->ticks[TA82_SPRITE_EVAL],(unsigned long long)m->ticks[TA82_SPRITE_COMP],(unsigned long long)m->ticks[TA82_APU],(unsigned long long)m->ticks[TA82_DSP],(unsigned long long)m->ticks[TA82_DMA],(unsigned long long)m->ticks[TA82_OTHER],(unsigned long long)m->ticks[TA82_OBSERVER],(unsigned long long)m->calls[TA82_CPU],(unsigned long long)m->calls[TA82_PPU],(unsigned long long)m->calls[TA82_DSP]);emit(line);
#if TA_R83_RESIDUAL_PROFILE
  {
   Residual83* r=&mode83[mode];
   uint64_t native_compose=m->ticks[TA82_NATIVE_COMPOSE];
#if TA_R84_COMPOSE_PROFILE
   for(unsigned k=TA84_SETUP;k<=TA84_BLANK;++k)native_compose+=m->ticks[k];
#if TA_R87_CACHE_MAP
   for(unsigned k=TA87_KEY;k<=TA87_STORE;++k)native_compose+=m->ticks[k];
#endif
#endif
   uint64_t ppu_total=m->ticks[TA82_PPU]+m->ticks[TA82_BG]+m->ticks[TA82_SPRITE_EVAL]+m->ticks[TA82_SPRITE_COMP]+native_compose+m->ticks[TA82_GENERIC_PIXELS];
   snprintf(line,sizeof line,"[TA R83 CPU] end_frame=%u mode=%u samples=%llu native_service_ticks=%llu native_other_ticks=%llu reference_ticks=%llu native_service_chunks=%llu native_other_chunks=%llu reference_chunks=%llu cumulative=1 fps_not_comparable=1\n",endFrame,label,(unsigned long long)m->frames,
    (unsigned long long)r->ticks[TA83_SERVICE],(unsigned long long)r->ticks[TA83_OTHER_NATIVE],(unsigned long long)r->ticks[TA83_REFERENCE],
    (unsigned long long)r->chunks[TA83_SERVICE],(unsigned long long)r->chunks[TA83_OTHER_NATIVE],(unsigned long long)r->chunks[TA83_REFERENCE]);emit(line);
   snprintf(line,sizeof line,"[TA R83 SERVICE] end_frame=%u mode=%u service_native_ticks=%llu service_other_native_ticks=%llu service_reference_ticks=%llu service_native_chunks=%llu service_other_native_chunks=%llu service_reference_chunks=%llu r55_accept=%llu r55_reject=%llu cumulative=1 fps_not_comparable=1\n",endFrame,label,
    (unsigned long long)r->service_ticks[TA83_SERVICE],(unsigned long long)r->service_ticks[TA83_OTHER_NATIVE],(unsigned long long)r->service_ticks[TA83_REFERENCE],
    (unsigned long long)r->service_chunks[TA83_SERVICE],(unsigned long long)r->service_chunks[TA83_OTHER_NATIVE],(unsigned long long)r->service_chunks[TA83_REFERENCE],
    (unsigned long long)r->service_accept,(unsigned long long)r->service_reject);emit(line);
   snprintf(line,sizeof line,"[TA R83 PPU] end_frame=%u mode=%u samples=%llu ppu_total_ticks=%llu native_compositor_other_ticks=%llu generic_pixel_ticks=%llu remaining_line_ticks=%llu bg_ticks=%llu sprite_eval_ticks=%llu sprite_comp_ticks=%llu cumulative=1 fps_not_comparable=1\n",endFrame,label,(unsigned long long)m->frames,
    (unsigned long long)ppu_total,(unsigned long long)native_compose,(unsigned long long)m->ticks[TA82_GENERIC_PIXELS],(unsigned long long)m->ticks[TA82_PPU],(unsigned long long)m->ticks[TA82_BG],(unsigned long long)m->ticks[TA82_SPRITE_EVAL],(unsigned long long)m->ticks[TA82_SPRITE_COMP]);emit(line);
#if TA_R84_COMPOSE_PROFILE
   /* Existing R83 aggregate includes every new child, preserving conservation.
      Output loops fuse color computation and stores: report that honestly. */
   snprintf(line,sizeof line,"[TA R84 COMPOSE] end_frame=%u mode=%u samples=%llu total_ticks=%llu other_ticks=%llu lines=%llu cumulative=1 fps_not_comparable=1\n",endFrame,label,(unsigned long long)m->frames,(unsigned long long)native_compose,(unsigned long long)m->ticks[TA82_NATIVE_COMPOSE],(unsigned long long)m->calls[TA82_NATIVE_COMPOSE]);emit(line);
   static const char* names84[]={"setup_palette_window","bg_cache_bookkeeping","direct_output","fixed_math_output","subscreen_math_output","window_math_output","scalar_math_output","blank_output"};
   for(unsigned k=TA84_SETUP;k<=TA84_BLANK;++k){
    snprintf(line,sizeof line,"[TA R84 STAGE] end_frame=%u mode=%u stage=%s ticks=%llu calls=%llu exclusive=1 cumulative=1 fps_not_comparable=1\n",endFrame,label,names84[k-TA84_SETUP],(unsigned long long)m->ticks[k],(unsigned long long)m->calls[k]);emit(line);
   }
#endif
#if TA_R87_CACHE_MAP
   static const char* names87[]={"key_compare","vram_dependency","restore_buffers","cache_miss_setup","store_buffers"};
   for(unsigned k=TA87_KEY;k<=TA87_STORE;++k){
    snprintf(line,sizeof line,"[TA R87 CACHE] end_frame=%u mode=%u stage=%s ticks=%llu calls=%llu exclusive=1 cumulative=1 fps_not_comparable=1\n",
     endFrame,label,names87[k-TA87_KEY],(unsigned long long)m->ticks[k],(unsigned long long)m->calls[k]);emit(line);
   }
   snprintf(line,sizeof line,"[TA R87 EVENTS] end_frame=%u mode=%u key_hit=%llu key_miss=%llu stamp_bypass=%llu generation_scan=%llu generation_fail=%llu bg_restore=%llu bg_store=%llu cumulative=1 fps_not_comparable=1\n",
    endFrame,label,(unsigned long long)events87[mode][TA87_EVENT_KEY_HIT],(unsigned long long)events87[mode][TA87_EVENT_KEY_MISS],
    (unsigned long long)events87[mode][TA87_EVENT_STAMP_BYPASS],(unsigned long long)events87[mode][TA87_EVENT_GENERATION_SCAN],
    (unsigned long long)events87[mode][TA87_EVENT_GENERATION_FAIL],(unsigned long long)events87[mode][TA87_EVENT_BG_RESTORE],
    (unsigned long long)events87[mode][TA87_EVENT_BG_STORE]);emit(line);
   uint64_t sum=0;for(unsigned i=0;i<TA87_REF_SLOTS;++i)sum+=ref87[i].ticks[mode];
   snprintf(line,sizeof line,"[TA R87 REF STATUS] end_frame=%u mode=%u refpc_ticks=%llu reference_cpu_ticks=%llu filtered_ref_ticks=%llu filtered_ref_chunks=%llu focused_pages=16 overflow=0 pc_scope=focused_pages16 pc_entry_attribution=1 cumulative=1 fps_not_comparable=1\n",
    endFrame,label,(unsigned long long)sum,(unsigned long long)r->ticks[TA83_REFERENCE],(unsigned long long)filteredRefTicks87[mode],(unsigned long long)filteredRefChunks87[mode]);emit(line);
   /* R87.1: complete exact-page totals inside our deliberately selected
      16 pages. Page totals are more useful than a misleading global top16. */
   for(unsigned g=0;g<TA87_FOCUSED_PAGES;++g){
    uint64_t pgTicks=0,pgChunks=0;
    for(unsigned j=0;j<256;++j){unsigned idx=g*256u+j;
     pgTicks+=ref87[idx].ticks[mode];pgChunks+=ref87[idx].chunks[mode];}
    if(!pgTicks)continue;
    snprintf(line,sizeof line,"[TA R87.1 REFPAGE] end_frame=%u mode=%u page=%04X00 reference_cpu_ticks=%llu chunks=%llu pc_scope=focused_pages16 cumulative=1 fps_not_comparable=1\n",
     endFrame,label,(unsigned)refPages87[g],(unsigned long long)pgTicks,(unsigned long long)pgChunks);emit(line);
   }
   bool usedRef87[TA87_REF_SLOTS]={false};
   for(unsigned rank=0;rank<16;++rank){unsigned best=TA87_REF_SLOTS;
    for(unsigned i=0;i<TA87_REF_SLOTS;++i)if(!usedRef87[i]&&ref87[i].ticks[mode]&&
       (best==TA87_REF_SLOTS||ref87[i].ticks[mode]>ref87[best].ticks[mode]))best=i;
    if(best==TA87_REF_SLOTS)break;usedRef87[best]=true;
    snprintf(line,sizeof line,"[TA R87 REFPC] end_frame=%u mode=%u rank=%u pc=%06X reference_cpu_ticks=%llu chunks=%llu entry_pc_attribution=1 cumulative=1 fps_not_comparable=1\n",
     endFrame,label,rank+1,ref87[best].pc_plus_one-1u,(unsigned long long)ref87[best].ticks[mode],(unsigned long long)ref87[best].chunks[mode]);emit(line);
   }
#endif
  }
#endif
  for(unsigned i=0;i<TA82_UNITS;++i){Unit82* q=&units82[i];if(!q->inclusive[mode])continue;snprintf(line,sizeof line,"[TA R82 OP] end_frame=%u mode=%u pc=%06X name=%s inclusive_cpu_ticks=%llu exclusive_cpu_ticks=%llu sampled_chunks=%llu starts_all_frames=%u completed_all_frames=%u return_mismatches=%u cumulative=1\n",endFrame,label,unitEntries[i],unitNames[i],(unsigned long long)q->inclusive[mode],(unsigned long long)q->exclusive[mode],(unsigned long long)q->chunks[mode],q->starts,q->completed,q->mismatch);emit(line);}
  bool used[TA82_CALLBACKS]={false};for(unsigned rank=0;rank<8;++rank){unsigned best=TA82_CALLBACKS;for(unsigned i=0;i<TA82_CALLBACKS;++i)if(!used[i]&&callbacks82[i].ticks[mode]&&(best==TA82_CALLBACKS||callbacks82[i].ticks[mode]>callbacks82[best].ticks[mode]))best=i;if(best==TA82_CALLBACKS)break;used[best]=true;Callback82* q=&callbacks82[best];snprintf(line,sizeof line,"[TA R82 CALLBACK] end_frame=%u mode=%u rank=%u pc=%06X inclusive_cpu_ticks=%llu exclusive_cpu_ticks=%llu starts_all_frames=%u completed_all_frames=%u return_mismatches=%u db=%02X dp=%04X mxed=%u state_changed=%u sp_min=%04X sp_max=%04X cumulative=1\n",endFrame,label,rank+1,q->pc,(unsigned long long)q->ticks[mode],(unsigned long long)q->exclusive[mode],q->starts,q->completed,q->mismatch,q->db,q->dp,q->mxed,q->changed,q->spmin,q->spmax);emit(line);}
  bool usedPages[TA82_PAGES]={false};for(unsigned rank=0;rank<8;++rank){unsigned best=TA82_PAGES;for(unsigned i=0;i<TA82_PAGES;++i)if(!usedPages[i]&&pages82[i].ticks[mode]&&(best==TA82_PAGES||pages82[i].ticks[mode]>pages82[best].ticks[mode]))best=i;if(best==TA82_PAGES)break;usedPages[best]=true;snprintf(line,sizeof line,"[TA R82 PAGE] end_frame=%u mode=%u rank=%u page=%06X exclusive_cpu_ticks=%llu sampled_chunks=%llu entry_pc_attribution=1 cumulative=1\n",endFrame,label,rank+1,(pages82[best].key-1u)<<8,(unsigned long long)pages82[best].ticks[mode],(unsigned long long)pages82[best].chunks[mode]);emit(line);}
 }
}
#endif
