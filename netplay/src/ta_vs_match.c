#include "ta_vs_match.h"
#include "ta_vs_local_game.h"
#if defined(TA_VS_REMATCH) && TA_VS_REMATCH
static const char*const names[13]={"LAKITU","BUMPTY","POOCHY","FLYING WIGGLER","FROGGY","GARGANTUA BLARGG","LUNGE FISH","RAPHAEL RAVEN","YOSHI","HOOKBILL KOOPA","PIRANHA PLANT","KAMEK","BOWSER"};
#else
static const char*const names[13]={"BUMPTY","LAKITU","POOCHY","FLYING WIGGLER","YOSHI","GARGANTUA BLARGG","FROGGY","LUNGE FISH","RAPHAEL RAVEN","HOOKBILL KOOPA","PIRANHA PLANT","KAMEK","BOWSER"};
#endif
unsigned ta_vs_character_stage(unsigned c){return c<8?c:c==8?12:c<13?c-1:255;}
const char*ta_vs_character_name(unsigned c){return c<13?names[c]:"INVALID";}
uint16_t ta_vs_setup_options(unsigned f,unsigned p,unsigned speed,unsigned character){
 if(p>1||speed<1||speed>10||character>=13)return 0;
 unsigned b=ta_vs_setup_buttons(f,p);
 if(f==1983||f==1984||f==2103||f==2104)b=0;
 if(f==2070||f==2071)b=0x100;
 if(f==2320||f==2321)b=8;
 /* SET LEVEL: original left/right input, independently applied to both local
    controller slots. No guessed speed variables or timer writes. */
 unsigned steps=speed>5?speed-5:5-speed;
 if(f>=1930&&f<1930+steps*20&&(f-1930)%20<2)b=speed>5?0x80:0x40;
 /* Original L+R unlock code on BOTH controllers, then move P0's stage cursor.
    Each endpoint owns P0 and selects its own character's real ROM theme. */
 if(character>=9&&f>=2120&&f<2124)b=0xc00;
 if(p==0){unsigned row=character<4?0:character<9?1:2,col=character<4?character:character<9?character-4:character-9;
  if(row&&f>=2140&&f<2142)b=0x20;
  if(row==2&&f>=2160&&f<2162)b=0x20;
  if(f>=2180&&f<2180+col*20&&(f-2180)%20<2)b=0x80;
 }
 return (uint16_t)b;
}
