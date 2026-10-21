#ifndef TA_VS_MATCH_H
#define TA_VS_MATCH_H
#include <stdint.h>
#define TA_VS_CHARACTERS 13u
#define TA_VS_OPTIONS_SETUP_FRAMES 2860u
unsigned ta_vs_character_stage(unsigned character);
const char* ta_vs_character_name(unsigned character);
uint16_t ta_vs_setup_options(unsigned frame,unsigned player,unsigned speed,unsigned character);
#endif
