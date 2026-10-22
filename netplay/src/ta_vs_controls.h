#ifndef TA_VS_CONTROLS_H
#define TA_VS_CONTROLS_H
#include <stdint.h>
#include <string.h>
#define TA_CONTROL_ACTIONS 8u
#define TA_CONTROL_CYCLE 0x1000u
static inline unsigned ta_controls_physical_bits(unsigned raw){return raw&0x3fffu;}
typedef struct {uint16_t bind[2][TA_CONTROL_ACTIONS];}TaControls;
static inline void ta_controls_reset(TaControls*c,unsigned p){static const uint16_t d[8]={0x10,0x20,0x40,0x80,0x101,0xc02,8,TA_CONTROL_CYCLE};if(p<2)memcpy(c->bind[p],d,sizeof d);}
static inline void ta_controls_bind(TaControls*c,unsigned p,unsigned a,unsigned bit){
 if(p>=2||a>=8||!bit||(bit&(bit-1))||(bit&~0x3fffu))return;unsigned old=c->bind[p][a];
 for(unsigned i=0;i<8;i++)if(i!=a&&(c->bind[p][i]&bit)){c->bind[p][i]&=(uint16_t)~bit;if(!c->bind[p][i])c->bind[p][i]=(uint16_t)old;}
 c->bind[p][a]=(uint16_t)bit;
}
static inline uint16_t ta_controls_apply(const TaControls*c,unsigned p,uint16_t raw){
 static const uint16_t action[8]={0x10,0x20,0x40,0x80,1,0x800,8,TA_CONTROL_CYCLE};uint16_t out=(raw&4u)|((raw&0x4000u)?TA_CONTROL_CYCLE:0u);
 if(p<2)for(unsigned i=0;i<8;i++)if(raw&c->bind[p][i])out|=action[i];return out;
}
static inline uint32_t ta_controls_crc(const uint8_t*b,unsigned n){uint32_t h=2166136261u;for(unsigned i=0;i<n;i++)h=(h^b[i])*16777619u;return h;}
static inline void ta_controls_encode(const TaControls*c,uint8_t b[44]){memcpy(b,"DBC13801",8);memcpy(b+8,c,sizeof *c);uint32_t h=ta_controls_crc(b,40);memcpy(b+40,&h,4);}
static inline int ta_controls_decode(TaControls*c,const uint8_t*b,unsigned n){
 if(n!=44||memcmp(b,"DBC13801",8))return 0;uint32_t h;memcpy(&h,b+40,4);if(h!=ta_controls_crc(b,40))return 0;
 TaControls v;memcpy(&v,b+8,sizeof v);for(unsigned p=0;p<2;p++){unsigned used=0;for(unsigned a=0;a<8;a++){unsigned x=v.bind[p][a];if(!x||(x&~0x3fffu)||(x&used))return 0;used|=x;}}*c=v;return 1;
}
#endif
