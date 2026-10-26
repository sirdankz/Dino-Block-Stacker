#ifndef TA_OPTIONS_IMPL_H
#define TA_OPTIONS_IMPL_H
#include "ta_options.h"
#include "ta_panel_colors.h"
#include <stdio.h>
#include <string.h>
#if defined(TA_PC)
extern "C" void TA_PC_AudioOptionsChanged(void);
#endif
static unsigned char optionsValue[TA_OPT_COUNT]={TA_DEFAULT_MASTER,TA_DEFAULT_MUSIC,TA_DEFAULT_GAME,0,0,0,0},optionsMap[6][6];
static bool optionsReady,optionsLoaded;static char optionsFile[512];
extern "C" {unsigned taPanelStyle;uint32_t taPanelRevision=1;uint8_t taPanelIndices[256];uint16_t taPanelTint[256];}
static const uint32_t optionsColors[6][6]={
 {0xff3030,0x20ee20,0x20e8ee,0xffec20,0x3055ff,0xe828ed},
 {0xff8f9c,0x81dfb5,0x81d8f7,0xffe394,0x879de9,0xc7a1ef},
 {0xf45b69,0xf9bd43,0x45c4b0,0xff8b42,0x6556a8,0xc34791},
 {0x17a99a,0x83edbc,0x70d8ff,0xf3ce58,0x3764d8,0xa77be8},
 {0xff5d9e,0xa6e848,0x29d8dc,0xffdb4b,0x5073ff,0xc861eb},
 {0xff3434,0x32df4b,0x29e7ff,0xffee26,0x376bff,0xff942b}};
extern "C" const char*TA_Options_SchemeName(unsigned s){static const char*names[]={"CLASSIC","PASTEL","SUNSET","OCEAN","CANDY","HIGH CONTRAST"};return s<6?names[s]:"CLASSIC";}
static uint32_t optionsHash(const unsigned char*p,unsigned n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
static void optionsPalette(){
 static const unsigned base[]={0xa2,0xa5,0xa8,0xab,0xb5,0xb8};unsigned s=optionsValue[TA_OPT_SCHEME];bool same=true;
 memset(taPanelIndices,0,sizeof taPanelIndices);memset(taPanelTint,0,sizeof taPanelTint);
 for(unsigned block=0;block<6;block++){unsigned assigned=optionsMap[s][block];if(assigned!=block)same=false;uint32_t rgb=optionsColors[s][assigned];
  for(unsigned bank=0;bank<2;bank++)for(unsigned tone=0;tone<3;tone++){unsigned index=base[block]+tone-bank*0x90;taPanelIndices[index]=(uint8_t)(base[assigned]+tone-bank*0x90);
   unsigned channels[3];for(unsigned c=0;c<3;c++){unsigned v=(rgb>>(16-c*8))&255u;v=tone==0?v+(255-v)/4:tone==1?v:v*45/100;channels[c]=(v*31+127)/255;}
   taPanelTint[index]=(uint16_t)(channels[0]|channels[1]<<5|channels[2]<<10);
  }
 }
 taPanelStyle=s==0?(same?0u:1u):2u;if(!++taPanelRevision)++taPanelRevision;
}
extern "C" void TA_Options_Init(const char*directory){
 char path[512];if(!directory)return;int n=snprintf(path,sizeof path,"%s/options-v1.bin",directory);if(n<0||(unsigned)n>=sizeof path)return;
 if(optionsReady&&!strcmp(path,optionsFile))return;strcpy(optionsFile,path);optionsReady=true;optionsLoaded=false;
 unsigned char defaults[TA_OPT_COUNT]={TA_DEFAULT_MASTER,TA_DEFAULT_MUSIC,TA_DEFAULT_GAME,0,0,0,0};memcpy(optionsValue,defaults,sizeof defaults);
 for(unsigned s=0;s<6;s++)for(unsigned i=0;i<6;i++)optionsMap[s][i]=(unsigned char)i;
 FILE*f=fopen(optionsFile,"rb");if(f){unsigned char b[57]={0};unsigned got=(unsigned)fread(b,1,sizeof b,f);fclose(f);
  uint32_t crc=(uint32_t)b[52]|(uint32_t)b[53]<<8|(uint32_t)b[54]<<16|(uint32_t)b[55]<<24;
  bool valid=got==56&&!memcmp(b,"DINOOPT1",8)&&crc==optionsHash(b,52)&&b[8]<=100&&b[9]<=100&&b[10]<=100&&b[11]<6&&b[12]<2&&b[13]<4&&b[14]<2;
  for(unsigned s=0;s<6&&valid;s++){unsigned mask=0;for(unsigned i=0;i<6;i++){unsigned v=b[16+s*6+i];if(v>=6||(mask&(1u<<v))){valid=false;break;}mask|=1u<<v;}}
  if(valid){memcpy(optionsValue,b+8,TA_OPT_COUNT);memcpy(optionsMap,b+16,36);optionsLoaded=true;}
 }
 optionsPalette();
#if defined(TA_PC)
 TA_PC_AudioOptionsChanged();
#endif
}
extern "C" unsigned TA_Options_Get(unsigned option){return option<TA_OPT_COUNT?optionsValue[option]:0;}
extern "C" void TA_Options_Set(unsigned option,unsigned v){
 if(option>=TA_OPT_COUNT)return;unsigned limit=option<=TA_OPT_GAME?100:option==TA_OPT_SCHEME?5:option==TA_OPT_WINDOW_SIZE?3:1;if(v>limit)v=limit;
 if(optionsValue[option]==v)return;optionsValue[option]=(unsigned char)v;if(option==TA_OPT_SCHEME)optionsPalette();
#if defined(TA_PC)
 if(option<=TA_OPT_GAME)TA_PC_AudioOptionsChanged();
#endif
}
extern "C" void TA_Options_ResetVolume(void){TA_Options_Set(TA_OPT_MASTER,TA_DEFAULT_MASTER);TA_Options_Set(TA_OPT_MUSIC,TA_DEFAULT_MUSIC);TA_Options_Set(TA_OPT_GAME,TA_DEFAULT_GAME);}
extern "C" int TA_Options_Loaded(void){return optionsLoaded;}
extern "C" int TA_Options_Save(void){
 if(!optionsReady)return 0;unsigned char b[56]={0};memcpy(b,"DINOOPT1",8);memcpy(b+8,optionsValue,TA_OPT_COUNT);memcpy(b+16,optionsMap,36);uint32_t crc=optionsHash(b,52);for(unsigned i=0;i<4;i++)b[52+i]=(unsigned char)(crc>>(8*i));
 FILE*f=fopen(optionsFile,"wb");if(!f)return 0;bool ok=fwrite(b,1,sizeof b,f)==sizeof b;if(fclose(f))ok=false;if(ok)optionsLoaded=true;return ok?1:0;
}
extern "C" unsigned TA_Options_Assignment(unsigned s,unsigned b){return s<6&&b<6?optionsMap[s][b]:0;}
extern "C" void TA_Options_Swap(unsigned a,unsigned b){if(a>=6||b>=6)return;unsigned s=optionsValue[TA_OPT_SCHEME];unsigned char t=optionsMap[s][a];optionsMap[s][a]=optionsMap[s][b];optionsMap[s][b]=t;optionsPalette();}
extern "C" void TA_Options_Rotate(int direction){unsigned s=optionsValue[TA_OPT_SCHEME];unsigned char old[6];memcpy(old,optionsMap[s],6);for(unsigned i=0;i<6;i++)optionsMap[s][i]=old[(i+(direction<0?1:5))%6];optionsPalette();}
extern "C" void TA_Options_ResetScheme(void){unsigned s=optionsValue[TA_OPT_SCHEME];for(unsigned i=0;i<6;i++)optionsMap[s][i]=(unsigned char)i;optionsPalette();}
extern "C" uint32_t TA_Options_PreviewColor(unsigned s,unsigned b){return s<6&&b<6?optionsColors[s][optionsMap[s][b]]:0;}
#endif
