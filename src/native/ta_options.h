#ifndef TA_OPTIONS_H
#define TA_OPTIONS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {TA_OPT_MASTER,TA_OPT_MUSIC,TA_OPT_GAME,TA_OPT_SCHEME,TA_OPT_FULLSCREEN,TA_OPT_WINDOW_SIZE,TA_OPT_VSYNC,TA_OPT_COUNT};
enum {TA_DEFAULT_MASTER=100,TA_DEFAULT_MUSIC=10,TA_DEFAULT_GAME=50};
void TA_Options_ResetVolume(void);
void TA_Options_Init(const char*directory);
unsigned TA_Options_Get(unsigned option);
void TA_Options_Set(unsigned option,unsigned value);
int TA_Options_Loaded(void);
int TA_Options_Save(void);
unsigned TA_Options_Assignment(unsigned scheme,unsigned block);
void TA_Options_Swap(unsigned a,unsigned b);
void TA_Options_Rotate(int direction);
void TA_Options_ResetScheme(void);
uint32_t TA_Options_PreviewColor(unsigned scheme,unsigned block);
const char*TA_Options_SchemeName(unsigned scheme);
#ifdef __cplusplus
}
#endif
#endif
