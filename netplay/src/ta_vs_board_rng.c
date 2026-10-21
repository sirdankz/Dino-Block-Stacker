#include "ta_vs_board_rng.h"
#include <string.h>
static TaVsRngState read_state(const uint8_t*w){
 TaVsRngState s={(uint16_t)(w[0x1a74]|((uint16_t)w[0x1a75]<<8)),(uint16_t)(w[0x1a76]|((uint16_t)w[0x1a77]<<8))};return s;
}
static void write_state(uint8_t*w,TaVsRngState s){
 w[0x1a74]=(uint8_t)s.index;w[0x1a75]=(uint8_t)(s.index>>8);w[0x1a76]=(uint8_t)s.bias;w[0x1a77]=(uint8_t)(s.bias>>8);
}
int ta_vs_rng_read(const uint8_t*w,size_t n,TaVsRngState*out){if(!w||n!=131072||!out)return TA_VS_RNG_ARGUMENT;*out=read_state(w);return 0;}
int ta_vs_rng_seed(TaVsBoardRng*r,const TaVsRngState seed[2]){
 if(!r||!seed)return TA_VS_RNG_ARGUMENT;
 if(r->active)return TA_VS_RNG_BUSY;
 /* seed may refer to r->board; preserve it before clearing the context. */
 TaVsRngState s[2]={seed[0],seed[1]};memset(r,0,sizeof *r);r->board[0]=s[0];r->board[1]=s[1];r->seeded=1;r->phase=2;return 0;
}
int ta_vs_rng_enter(TaVsBoardRng*r,unsigned board,uint8_t*w,size_t n){
 if(!r||!r->seeded||board>1||!w||n!=131072)return TA_VS_RNG_ARGUMENT;
 if(r->active)return TA_VS_RNG_BUSY;
 r->saved=read_state(w);r->owner=(uint8_t)board;r->active=1;write_state(w,r->board[board]);return 0;
}
int ta_vs_rng_leave(TaVsBoardRng*r,uint8_t*w,size_t n){
 if(!r||!w||n!=131072)return TA_VS_RNG_ARGUMENT;
 if(!r->active)return TA_VS_RNG_INACTIVE;
 r->board[r->owner]=read_state(w);r->draws[r->owner]++;write_state(w,r->saved);r->active=0;return 0;
}
int ta_vs_rng_next(const uint8_t*rom,size_t n,TaVsRngState*s,uint8_t*out){
 if(!rom||n!=1048576||!s||!out)return TA_VS_RNG_ARGUMENT;
 s->index=(uint16_t)((s->index+1u)&2047u);if(!s->index)s->bias++;
 /* ADC is binary, CLC precedes it; only the low byte survives AND00FF. */
 *out=(uint8_t)(rom[0x20290u+s->index]+s->bias);return 0;
}

int ta_vs_rng_opcode(TaVsBoardRng*r,uint32_t pc,uint16_t stack,unsigned db,unsigned widths,uint8_t*w,size_t n){
 if(!r||!r->seeded||!w||n!=131072)return TA_VS_RNG_ARGUMENT;
 if(pc==0x829d51u||pc==0x829d59u||pc==0x829e01u||pc==0x829eb7u){
  if(r->active)return TA_VS_RNG_BUSY;
  r->phase=(uint8_t)(pc==0x829d59u?0:pc==0x829e01u?1:2);return 0;
 }
 if(pc==0x8481ddu&&r->phase<2){
  if(db!=0x84||widths)return TA_VS_RNG_ARGUMENT;
  int result=ta_vs_rng_enter(r,r->phase,w,n);if(!result)r->scopeStack=stack;return result;
 }
 if(pc==0x8481f7u&&r->active){
  if(stack!=r->scopeStack||db!=0x84||widths)return TA_VS_RNG_ARGUMENT;
  return ta_vs_rng_leave(r,w,n);
 }
 return 0;
}
