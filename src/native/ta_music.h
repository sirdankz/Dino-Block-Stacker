#ifndef TA_MUSIC_H
#define TA_MUSIC_H
#include <stdint.h>
enum {TA_MUSIC_GAME=0,TA_MUSIC_MENU=1,TA_MUSIC_RESULTS=2};
#ifdef __cplusplus
extern "C" {
#endif
void TA_Music_Init(const char*directory);
void TA_Music_Shutdown(void);
void TA_Music_SetScene(unsigned scene);
void TA_Music_MenuFrame(void);
/* One audio owner per platform. Returns zero for duplicate menu APU submits. */
int TA_Music_Process(const int16_t*game,int16_t*output,unsigned frames);
#ifdef __cplusplus
}
#endif
#endif
