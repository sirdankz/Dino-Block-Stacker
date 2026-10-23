/* Shared pixel-art menu composition for Xbox and Android. */
#ifndef TA_MENU_THEME_H
#define TA_MENU_THEME_H
#include <stdint.h>
#include <string.h>
#include "ta_menu_sprites.h"

static void ta_menu_art_draw(uint32_t* output,uint32_t now){
    // Cache the complete skin between pixel-scroll steps. No runtime PNG decoding.
    static uint8_t tile[512*480];
    static uint32_t composed[512*480];
    static uint32_t colors[TA_MENU_FRAME_COLORS][TA_MENU_BACKGROUND_COLORS];
    static bool ready;
    static uint32_t steppedAt;
    static unsigned offsetX,offsetY;
    bool redraw=!ready;
    if(!ready){
        unsigned pixel=0;
        for(unsigned i=0;i<sizeof ta_menu_background_runs/sizeof ta_menu_background_runs[0];i++){
            unsigned run=ta_menu_background_runs[i],length=(run&511u)+1u;
            memset(tile+pixel,run>>9,length);pixel+=length;
        }
        for(unsigned f=0;f<TA_MENU_FRAME_COLORS;f++){
            uint32_t ink=ta_menu_frame_palette[f];unsigned alpha=ink>>24,keep=255u-alpha;
            for(unsigned b=0;b<TA_MENU_BACKGROUND_COLORS;b++){
                uint32_t paper=ta_menu_background_palette[b];
                unsigned r=(((ink>>16)&255u)*alpha+((paper>>16)&255u)*keep+127u)/255u;
                unsigned g=(((ink>>8)&255u)*alpha+((paper>>8)&255u)*keep+127u)/255u;
                unsigned blue=((ink&255u)*alpha+(paper&255u)*keep+127u)/255u;
                colors[f][b]=(r<<16)|(g<<8)|blue;
            }
        }
        ready=true;steppedAt=now;
    }else if((uint32_t)(now-steppedAt)>=50u){
        unsigned steps=(uint32_t)(now-steppedAt)/50u;steppedAt+=steps*50u;
        offsetX=(offsetX+steps)%512u;offsetY=(offsetY+steps)%480u;redraw=true;
    }
    if(redraw){
        for(unsigned y=0;y<480u;y++){
            const uint8_t* source=tile+((y+offsetY)%480u)*512u;
            uint32_t* dest=composed+y*512u;
            unsigned sx=offsetX;
            for(unsigned i=ta_menu_frame_rows[y];i<ta_menu_frame_rows[y+1];i++){
                unsigned run=ta_menu_frame_runs[i],left=(run&511u)+1u;
                const uint32_t* palette=colors[run>>9];
                while(left){
                    unsigned count=512u-sx;if(count>left)count=left;
                    for(unsigned x=0;x<count;x++)*dest++=palette[source[sx+x]];
                    left-=count;sx+=count;if(sx==512u)sx=0;
                }
            }
        }
    }
    memcpy(output,composed,sizeof composed);
}
#endif
