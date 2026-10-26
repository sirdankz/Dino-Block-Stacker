#ifndef TA_VS_LOBBY_UI_H
#define TA_VS_LOBBY_UI_H
#include "ta_vs_menu_layout.h"
/* Footer rows are separate touch bands, with four clear pixels between them. */
static inline int ta_lobby_footer_top(int y){return y>=430?430:404;}
static inline int ta_lobby_footer_height(int y){return y>=430?18:22;}
/* A remains an accessible alias; START is the visible lobby ready action. */
static inline int ta_lobby_ready_pressed(unsigned pressed){return (pressed&9u)!=0;}
static inline int ta_lobby_party_can_start(unsigned phase,unsigned self,unsigned host,unsigned members,unsigned present,unsigned ready,unsigned flags,unsigned start,unsigned speedPending){
 unsigned count=0;for(unsigned n=present;n;n>>=1)count+=n&1u;
 return !phase&&self<4&&self==host&&members==present&&count>=2&&ready==present&&(flags&1u)&&!start&&!speedPending;
}
#endif
