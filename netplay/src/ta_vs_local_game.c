#include "ta_vs_local_game.h"
#include <string.h>
static TaVsLocalGame*bound;
static unsigned word(const uint8_t*w,unsigned a){return w[a]|((unsigned)w[a+1]<<8);}
static void put(uint8_t*w,unsigned a,unsigned v){w[a]=(uint8_t)v;w[a+1]=(uint8_t)(v>>8);}
void ta_vs_local_init(TaVsLocalGame*g,Snes*s,TaVsSession*n,uint32_t seed){memset(g,0,sizeof *g);g->s=s;g->session=n;g->seed=seed;g->active=1;}
void ta_vs_local_bind(TaVsLocalGame*g){bound=g;}
int ta_vs_local_receive(TaVsLocalGame*g){
 if(!g||!g->session)return TA_VS_BAD_ARGUMENT;
 const TaVsEvent*e;while((e=ta_vs_peek(g->session))&&e->tag==TA_VS_ATTACK){int r=ta_vs_attack_stage(g->session,&g->inbox);if(r)return r;}
 return 0;
}
void ta_vs_game_trace(Cpu*c){
 TaVsLocalGame*g=bound;if(!g||!g->active||g->error||c!=g->s->cpu||c->intWanted||c->resetWanted||c->waiting||c->stopped)return;
 uint32_t pc=((uint32_t)c->k<<16)|c->pc;
 if(pc!=0x829d51&&pc!=0x829d59&&pc!=0x829e01&&pc!=0x829eb7&&pc!=0x8481dd&&pc!=0x8481f7&&pc!=0x82e901&&pc!=0x82e950)return;
 /* Interpreter fallback and native dispatch may observe the same not-yet-run
    opcode. Deduplicate by guest master clock, never by host time. */
 if(g->lastPc==pc&&g->lastClock==g->s->cycles)return;g->lastPc=pc;g->lastClock=g->s->cycles;
 if(c->e||c->dp){g->error=TA_VS_BAD_ARGUMENT;return;}
 uint8_t*w=g->s->ram;unsigned widths=c->mf|(c->xf<<1);
 if(pc==0x829d51&&!g->rng.seeded){
  TaVsRngState seeds[2]={{(uint16_t)(g->seed&2047),(uint16_t)(g->seed>>11)},{0,0}};
#if defined(TA_VS_FAST_START) && TA_VS_FAST_START
  /* N11 seeded the original initializer before building the opening board.
     Continue its consumed RNG stream instead of restarting that sequence. */
  if(ta_vs_rng_read(w,sizeof g->s->ram,seeds)){g->error=TA_VS_BAD_ARGUMENT;return;}
#endif
  seeds[1]=seeds[0];
  uint16_t flips[2]={(uint16_t)word(w,0x6559),(uint16_t)word(w,0x6559)};
  if(ta_vs_rng_seed(&g->rng,seeds)||ta_vs_rows_seed(&g->rows,flips)){g->error=TA_VS_BAD_ARGUMENT;return;}
 }
 if(!g->rng.seeded)return;
 int r=ta_vs_rows_opcode(&g->rows,g->rng.phase,pc,c->sp,c->db,widths,w,sizeof g->s->ram);
 if(!r)r=ta_vs_rng_opcode(&g->rng,pc,c->sp,c->db,widths,w,sizeof g->s->ram);
 if(r){g->error=r;return;}
 if(pc==0x829e01){
  /* The real opponent runs on its own device. Hold only the unused local P1
     automatic-rise timer positive, before the original P1 update. Do not
     skip its instructions, copy remote board RAM, or fake local game state. */
  if(widths||word(w,0x2a8)!=2){g->error=TA_VS_BAD_ARGUMENT;return;}
  put(w,0x326,0x6000);
 }
 if(pc!=0x829d59)return;
 if(widths||word(w,0x2a8)!=2){g->error=TA_VS_BAD_ARGUMENT;return;}g->ticks++;
 /* 89A78C has completed original projectile scheduling/delivery. Capture its
    effective P1 mailboxes before either board consumes them. Thus creation
    order never substitutes for the original actor delivery priority. */
 static const unsigned pending[7]={0x42c,0,0x444,0x44c,0x454,0x45c,0x470};
 for(unsigned type=0;type<7;type++)if(type!=1){
  unsigned address=pending[type]+2,n=word(w,address);
  while(n){unsigned value=type?1:n;r=ta_vs_attack_send(g->session,type,value,g->s->frames,NULL);if(r){g->error=r;return;}g->sent++;if(!type)n=0;else n--;put(w,address,n);}
 }
 /* Received events are already ordered by the sender's delivery boundary.
    Only the owning endpoint can modify P0; display packets have no access. */
 for(;;){uint32_t next=0;for(unsigned k=0;k<TA_VS_WINDOW;k++)if(g->inbox.used[k]&&(!next||g->inbox.attack[k].sequence<next))next=g->inbox.attack[k].sequence;
  if(!next)break;r=ta_vs_attack_deliver(&g->inbox,next,w,sizeof g->s->ram,0);if(r){g->error=r;return;}g->received++;
 }
}
uint16_t ta_vs_setup_buttons(unsigned f,unsigned p){
 static const unsigned route[][3]={{300,0,8},{301,0,8},{900,0,8},{901,0,8},{1200,0,8},{1201,0,8},{1503,0,0x20},{1504,0,0x20},{1563,0,0x100},{1564,0,0x100},{1743,0,0x20},{1744,0,0x20},{1803,0,0x100},{1804,0,0x100},{1983,0,0x100},{1984,0,0x100},{2103,0,8},{2104,0,8},{1983,1,0x100},{1984,1,0x100},{2103,1,8},{2104,1,8}};
 for(unsigned k=0;k<sizeof route/sizeof *route;k++)if(f==route[k][0]&&p==route[k][1])return (uint16_t)route[k][2];return 0;
}
