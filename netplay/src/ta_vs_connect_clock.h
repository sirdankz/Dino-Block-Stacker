#ifndef TA_VS_CONNECT_CLOCK_H
#define TA_VS_CONNECT_CLOCK_H
#include <stdint.h>
/* Local connection deadline only. No wire fields or guest timing changes. */
typedef struct{uint32_t opened,ready;unsigned armed;}TaVsConnectClock;
static inline void ta_vs_connect_reset(TaVsConnectClock*c,uint32_t now){c->opened=now;c->ready=0;c->armed=0;}
static inline int ta_vs_connect_ready(TaVsConnectClock*c,uint32_t now,uint32_t*lastServer){if(c->armed)return 0;c->armed=1;c->ready=now;*lastServer=now;return 1;}
static inline int ta_vs_connect_expired(const TaVsConnectClock*c,uint32_t now,uint32_t lastServer){return c->armed&&(uint32_t)(now-lastServer)>10000u;}
#endif
