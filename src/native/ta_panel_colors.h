#ifndef TA_PANEL_COLORS_H
#define TA_PANEL_COLORS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
extern unsigned taPanelStyle;
extern uint32_t taPanelRevision;
extern uint8_t taPanelIndices[256];
extern uint16_t taPanelTint[256];
#ifdef __cplusplus
}
#endif
/* Only the original stationary and animated panel palettes are eligible. CGRAM reads,
   writes, state saves, rules and panel identities retain their original data. */
static inline uint16_t ta_panel_color(const uint16_t*palette,unsigned index,unsigned mode){
 uint16_t color=palette[index];
 if(!taPanelStyle||mode!=2||!taPanelIndices[index])return color;
 /* Neutral sprite entries fingerprint the original gameplay palette. */
 if(palette[0xa1]!=0x6b5a||palette[0xb1]!=0x6b5a)return color;
 if(taPanelStyle==1)return palette[taPanelIndices[index]];
 if(!(color&0x7fff)||(color&0x7fff)==0x7fff)return color;
 uint16_t tint=taPanelTint[index];
 /* The original game fades the stationary block bank independently at READY. */
 if(index<0x80){unsigned level=palette[0x11]&31u;if(level<26u){unsigned r=(tint&31u)*level/26u,g=((tint>>5)&31u)*level/26u,b=((tint>>10)&31u)*level/26u;tint=(uint16_t)(r|(g<<5)|(b<<10));}}
 return tint;
}
#endif
