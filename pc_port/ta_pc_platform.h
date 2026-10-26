#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "ta_vs_party_layout.h"
struct Ppu;
typedef struct { IN_ADDR ina; } XNADDR;
#define XNET_GET_XNADDR_STATIC 1u
#define XNET_GET_XNADDR_DHCP 2u
#define XNET_GET_XNADDR_AUTO 4u
#define XNET_GET_XNADDR_PPPOE 8u
#define XNET_GET_XNADDR_PENDING 0u
extern "C" {
const char* TA_PC_SaveDirectory(void);
int XNetRandom(BYTE*,unsigned);
DWORD XNetGetTitleXnAddr(XNADDR*);
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
void TA_Menu_Reset(void);
void TA_Menu_Begin(const char*);
void TA_Menu_Region(int,int,int,int,int,unsigned,unsigned);
unsigned TA_Menu_Read(int*);
int TA_PC_TextInput(char*,unsigned,const char*,const char*,bool,unsigned,int (*)(void));
int ta_pc_chat_input(char*,unsigned,const char*,int (*)(void));
int TA_Lobby_Active(void);
int TA_Lobby_ConfirmExit(void);
void TA_PC_FrontendClose(void);
void TA_PC_ShutdownFrontend(void);
void TA_LanGame_Run(const uint8_t*,unsigned);
}
#define TA_N12_CACHE_DIR TA_PC_SaveDirectory()
#define TA_VS_PROFILE_DIR TA_PC_SaveDirectory()
#define TA_Log(...) ((void)0)
#define TA_LogPath() ""
#define TA_LogClose() ((void)0)
