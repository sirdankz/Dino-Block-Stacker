#include "ta_vs_controls.h"
#include <assert.h>
#include <stdio.h>
int main(void){assert(ta_controls_physical_bits(0x1000)==0x1000);assert(ta_controls_physical_bits(0x2000)==0x2000);assert(ta_controls_physical_bits(0x4000)==0);TaControls c,d;ta_controls_reset(&c,0);ta_controls_reset(&c,1);
 assert(ta_controls_apply(&c,0,1)==1);assert(ta_controls_apply(&c,0,0x100)==1);assert(ta_controls_apply(&c,0,0x400)==0x800);assert(ta_controls_apply(&c,0,0x1000)==TA_CONTROL_CYCLE);
 ta_controls_bind(&c,0,7,0x800);assert(ta_controls_apply(&c,0,0x800)==TA_CONTROL_CYCLE);assert(ta_controls_apply(&c,0,0x400)==0x800);assert(ta_controls_apply(&c,1,0x800)==0x800);assert(ta_controls_apply(&c,0,0x4000)==TA_CONTROL_CYCLE);
 ta_controls_bind(&c,0,4,8);assert(ta_controls_apply(&c,0,8)==1);assert(ta_controls_apply(&c,0,1)==8);
 uint8_t b[44];ta_controls_encode(&c,b);assert(ta_controls_decode(&d,b,sizeof b));assert(!memcmp(&c,&d,sizeof c));b[8]^=1;assert(!ta_controls_decode(&d,b,sizeof b));assert(!memcmp(&c,&d,sizeof c));
 for(unsigned p=0;p<2;p++)for(unsigned a=0;a<8;a++)for(unsigned bit=1;bit<=0x2000;bit<<=1){ta_controls_bind(&c,p,a,bit);ta_controls_encode(&c,b);assert(ta_controls_decode(&d,b,sizeof b));assert(ta_controls_apply(&c,p,(uint16_t)bit)&(a<4?0x10u<<a:a==4?1:a==5?0x800:a==6?8:TA_CONTROL_CYCLE));}
 puts("Control tests passed: defaults, independent players, collision-safe remaps, saved settings validation and target binding.");return 0;}
