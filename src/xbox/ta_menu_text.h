#ifndef TA_MENU_TEXT_H
#define TA_MENU_TEXT_H
#include "ta_vs_menu_layout.h"
/* Keep individual menu lines inside their panel. Paragraphs use explicit boxes. */
static void ta_menu_text(uint32_t*p,int x,int y,const char*s,uint32_t color,int requested){
#if defined(TA_DINO_BRAND) && TA_DINO_BRAND
 TaMenuRect b=ta_menu_text_region(y);int h=requested>=4?21:14;
 if(x<b.left)x=b.left;if(x>=b.right)return;
 if(y<b.top)y=b.top;if(y+h>b.bottom)y=b.bottom-h;
 ta_vs_lan_text_box(p,x,y,b.right-x,h,s,color,requested);
#else
 ta_vs_lan_text(p,x,y,s,color,requested);
#endif
}
#endif
