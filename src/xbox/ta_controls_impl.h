#include "ta_vs_controls.h"
#include "ta_options_impl.h"
#if defined(TA_ANDROID)
static unsigned controlsPhysical[2],controlsTouch;static TaControls defaultControls;
#endif
static bool controlsReady;static TaControls controls;static char controlsPath[512];
extern "C" void TA_Controls_Init(const char*directory){
 TA_Options_Init(directory);controlsReady=true;ta_controls_reset(&controls,0);ta_controls_reset(&controls,1);
#if defined(TA_ANDROID)
 defaultControls=controls;controlsPhysical[0]=controlsPhysical[1]=controlsTouch=0;
#endif
 snprintf(controlsPath,sizeof controlsPath,"%s/controls-v1.bin",directory);
 FILE*f=fopen(controlsPath,"rb");if(f){uint8_t b[45];unsigned n=(unsigned)fread(b,1,sizeof b,f);fclose(f);ta_controls_decode(&controls,b,n);}
}
extern "C" unsigned TA_Controls_Get(unsigned p,unsigned a){return p<2&&a<8?controls.bind[p][a]:0;}
extern "C" void TA_Controls_Bind(unsigned p,unsigned a,unsigned bit){ta_controls_bind(&controls,p,a,bit);}
extern "C" void TA_Controls_Reset(unsigned p){ta_controls_reset(&controls,p);}
extern "C" bool TA_Controls_Save(void){uint8_t b[44];ta_controls_encode(&controls,b);FILE*f=fopen(controlsPath,"wb");if(!f)return false;bool ok=fwrite(b,1,sizeof b,f)==sizeof b;return fclose(f)==0&&ok;}
extern "C" uint16_t TA_Input_GameButtons(uint16_t raw,unsigned player){if(!controlsReady)return raw;
#if defined(TA_ANDROID)
 if(player<2)return ta_controls_apply(&controls,player,(uint16_t)controlsPhysical[player])|(player==0?ta_controls_apply(&defaultControls,0,(uint16_t)controlsTouch):0);
#endif
 return ta_controls_apply(&controls,player,raw);}
