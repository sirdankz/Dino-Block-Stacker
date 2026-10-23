#pragma once
#include <xtl.h>
#include <winsockx.h>
#include "ta_360_compat.h"
#include "ta_vs_party_layout.h"
struct Ppu;
extern "C" {
const char* TA_360_SaveDirectory(void);
const uint8_t* TA_360_IPBytes(uint32_t address);
DWORD TA_360_TitleAddress(XNADDR*);
HRESULT TA_Video_Init(void);
void TA_Video_Shutdown(void);
bool TA_Video_ActiveAvailable(void);
void TA_Video_R126Counts(unsigned long[4]);
void TA_Video_ReadReset(unsigned long*,unsigned long*,unsigned long*,unsigned long*);
void TA_Video_ReadResetUploadTicks(uint64_t[3]);
void TA_Video_Submit(const unsigned char*);
bool TA_Video_SubmitActive224(const unsigned char*);
bool TA_Video_SubmitActive224Direct(const Ppu*);
bool TA_Video_SubmitNative224Direct(const Ppu*);
void TA_Video_BeginR126(Ppu*);
bool TA_Video_EndR126(Ppu*);
void TA_Video_ReleaseLanPpu(Ppu*);
bool TA_Video_SetLanOverlay(const uint32_t*,unsigned);
bool TA_Video_SetPartyVisual(const TaPartyVisual*);
bool TA_Video_SetPartyOverlay(const uint32_t*,unsigned);
void TA_Video_SetPartyBackdrop(const uint32_t*);
void TA_Video_PartyStats(unsigned long[4]);
void TA_Video_SetPopulation(unsigned,unsigned,unsigned,unsigned);
void TA_Video_SetDuelMode(unsigned);
HRESULT TA_Audio_Init(void);
void TA_Audio_Submit(const int16_t*);
void TA_Audio_Shutdown(void);
void TA_Input_Init(void);
void TA_Input_Poll(uint16_t[2]);
void TA_Input_Shutdown(void);
void TA_Input_RightStick(int*,int*);
void TA_Input_SetLeaderboard(unsigned);
int TA_Input_LeaderboardScroll(void);
unsigned TA_Input_LeaderboardActions(void);
void TA_Background_Set(void (*)(void));
void TA_Controls_Init(const char*);
unsigned TA_Controls_Get(unsigned,unsigned);
void TA_Controls_Bind(unsigned,unsigned,unsigned);
void TA_Controls_Reset(unsigned);
bool TA_Controls_Save(void);
uint16_t TA_Input_GameButtons(uint16_t,unsigned);
void TA_360_FrontendClose(void);
void TA_LanGame_Run(const uint8_t*,unsigned);
}
#define XNetGetTitleXnAddr TA_360_TitleAddress
#define TA_N12_CACHE_DIR TA_360_SaveDirectory()
#define TA_VS_PROFILE_DIR TA_360_SaveDirectory()
#define TA_Log(...) ((void)0)
#define TA_LogPath() ""
#define TA_LogClose() ((void)0)
