#include "ta_vs_board_rows.h"
#include <string.h>
static uint16_t read_flip(const uint8_t*w){return (uint16_t)(w[0x6559]|((uint16_t)w[0x655a]<<8));}
static void write_flip(uint8_t*w,uint16_t v){w[0x6559]=(uint8_t)v;w[0x655a]=(uint8_t)(v>>8);}
int ta_vs_rows_seed(TaVsBoardRows*r,const uint16_t seeds[2]){
 if(!r||!seeds)return TA_VS_RNG_ARGUMENT;if(r->active)return TA_VS_RNG_BUSY;
 uint16_t a=seeds[0],b=seeds[1];memset(r,0,sizeof *r);r->flip[0]=a;r->flip[1]=b;r->seeded=1;return 0;
}
int ta_vs_rows_opcode(TaVsBoardRows*r,unsigned phase,uint32_t pc,uint16_t stack,unsigned db,unsigned widths,uint8_t*w,size_t n){
 if(!r||!r->seeded||!w||n!=131072)return TA_VS_RNG_ARGUMENT;
 if(pc==0x82e901u&&phase<2){
  if(r->active)return TA_VS_RNG_BUSY;
  if(widths||!(db==0x7e||db<0x40||(db>=0x80&&db<0xc0))||w[0x360]!=phase*2||w[0x361])return TA_VS_RNG_ARGUMENT;
  r->saved=read_flip(w);r->stack=stack;r->owner=(uint8_t)phase;r->active=1;write_flip(w,r->flip[phase]);return 0;
 }
 if(pc==0x82e950u&&r->active){
  if(stack!=r->stack||widths)return TA_VS_RNG_ARGUMENT;
  r->flip[r->owner]=read_flip(w);r->rows[r->owner]++;write_flip(w,r->saved);r->active=0;
 }
 return 0;
}
int ta_vs_panel_pick(const uint8_t*rom,size_t n,const TaVsRngState*seed,unsigned limit,unsigned mask,uint16_t above,uint16_t horizontal,unsigned max,TaVsPanelPick*out){
 if(!rom||n!=1048576||!seed||!out||limit<2||limit>8||mask>65535||!max||max>4096)return TA_VS_RNG_ARGUMENT;
 TaVsPanelPick result;memset(&result,0,sizeof result);result.next=*seed;
 for(unsigned k=0;k<max;k++){
  uint8_t random;int status=ta_vs_rng_next(rom,n,&result.next,&random);if(status)return status;
  unsigned value=random&mask&7u;result.draws++;
  if(!value||value>=limit||value==above)continue;
  value|=0x2000u;if(value==horizontal)continue;
  result.panel=(uint16_t)value;*out=result;return 0;
 }
 return 1;
}
