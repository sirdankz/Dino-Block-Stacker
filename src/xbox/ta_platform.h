#pragma once
#include <xtl.h>
#include <stdint.h>
#include "ta_vs_party_layout.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
void TA_Log(const char *fmt,...);
const char *TA_LogPath(void);
void TA_LogClose(void);
/* R120 NoLog: compile out all TA_Log callsites.  Variadic arguments are NOT
   evaluated; ta_logging.cpp is omitted from this configuration, so no file
   handles, fflush, DbgPrint or runtime log strings are linked. */
#if defined(TA_NO_LOG) && TA_NO_LOG
#define TA_Log(...) ((void)0)
#define TA_LogPath() ""
#define TA_LogClose() ((void)0)
#endif
struct Ppu;
void TA_Video_BeginR126(struct Ppu*);
bool TA_Video_EndR126(struct Ppu*);
void TA_Video_R126Counts(unsigned long out[4]);
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
void TA_Video_R127Counts(unsigned long out[2]);
#if defined(TA_R128_GPU_ATLAS_CACHE) && TA_R128_GPU_ATLAS_CACHE
void TA_Video_R128Counts(unsigned long out[4]);
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
void TA_Video_R129Counts(unsigned long out[4]);
#endif
#endif
#endif
HRESULT TA_Video_Init(void);
void TA_Video_Submit(const unsigned char *packed_512x480);
bool TA_Video_ActiveAvailable(void);
/* R6 experimental single-copy path: may return false, then use R5 fallback. */
typedef struct Ppu Ppu;
bool TA_Video_SubmitNative224Direct(const Ppu* ppu);
bool TA_Video_SubmitActive224Direct(const Ppu* ppu);
/* True on successful upload & draw; false permits a full-frame fallback. */
bool TA_Video_SubmitActive224(const unsigned char *packed_512x224);
#if defined(TA_VS_NATIVE_GAME) && TA_VS_NATIVE_GAME
void TA_Video_ReleaseLanPpu(struct Ppu*);
bool TA_Video_SetLanOverlay(const uint32_t* rgb512x224,unsigned revision);
#endif
void TA_Video_ReadReset(unsigned long *upload_ms, unsigned long *draw_ms, unsigned long *present_ms, unsigned long *frames);
void TA_Video_ReadResetUploadTicks(uint64_t ticks[3]);
void TA_Video_Shutdown(void);
HRESULT TA_Audio_Init(void);
void TA_Audio_Submit(const int16_t* stereo800);
void TA_Audio_Shutdown(void);
void TA_Input_Init(void);
void TA_Input_RightStick(int*,int*);
/* Display-only 512x224 canvas; only columns 224..511 are drawn. */
bool TA_Video_SetPartyOverlay(const uint32_t*,unsigned);
bool TA_Video_SetPartyVisual(const TaPartyVisual*);
void TA_Video_PartyStats(unsigned long out[4]);
void TA_Video_SetPartyBackdrop(const uint32_t*);
void TA_Video_SetPopulation(unsigned accounts,unsigned ingame,unsigned online,unsigned known);
void TA_Input_Poll(uint16_t out[2]);
void TA_Background_Set(void (*tick)(void));
void TA_Controls_Init(const char*directory);
unsigned TA_Controls_Get(unsigned player,unsigned action);
void TA_Controls_Bind(unsigned player,unsigned action,unsigned bit);
void TA_Controls_Reset(unsigned player);
bool TA_Controls_Save(void);
uint16_t TA_Input_GameButtons(uint16_t raw,unsigned player);

void TA_Input_Shutdown(void);
#ifdef __cplusplus
}
#endif
