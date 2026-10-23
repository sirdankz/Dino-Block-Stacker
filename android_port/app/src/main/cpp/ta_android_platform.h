#pragma once
#include <stdint.h>
#include "ta_vs_party_layout.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
typedef uint32_t DWORD;typedef int BOOL;typedef unsigned char BYTE;
typedef intptr_t HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1u
#define OPEN_EXISTING 3u
#define CREATE_ALWAYS 2u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define FILE_FLAG_WRITE_THROUGH 0x80000000u
#define ERROR_FILE_NOT_FOUND 2u
#define ERROR_PATH_NOT_FOUND 3u
#define ERROR_ACCESS_DENIED 5u
#define ERROR_INVALID_DATA 13u
#define ERROR_WRITE_FAULT 29u
#define ERROR_ALREADY_EXISTS 183u
#define ERROR_FILENAME_EXCED_RANGE 206u

typedef struct{DWORD dwLowDateTime,dwHighDateTime;}FILETIME;
typedef struct{struct in_addr ina;}XNADDR;
#define WSAEMSGSIZE 10040
#define XNET_GET_XNADDR_STATIC 1u
#define XNET_GET_XNADDR_DHCP 2u
#define XNET_GET_XNADDR_AUTO 4u
#define XNET_GET_XNADDR_PPPOE 8u
#define MOVEFILE_REPLACE_EXISTING 1u
#define MOVEFILE_WRITE_THROUGH 2u
#ifdef __cplusplus
extern "C" {
#endif
DWORD GetTickCount(void);
void GetSystemTimeAsFileTime(FILETIME*);
BOOL MoveFileExA(const char*,const char*,DWORD);
BOOL CreateDirectoryA(const char*,void*);
DWORD GetLastError(void);
HANDLE CreateFileA(const char*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
DWORD GetFileSize(HANDLE,DWORD*);
BOOL ReadFile(HANDLE,void*,DWORD,DWORD*,void*);
BOOL WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
BOOL FlushFileBuffers(HANDLE);
BOOL CloseHandle(HANDLE);
BOOL DeleteFileA(const char*);

int XNetRandom(BYTE*,unsigned);
DWORD XNetGetTitleXnAddr(XNADDR*);
const char*ta_android_cache_dir(void);
#define TA_N12_CACHE_DIR ta_android_cache_dir()
void TA_Log(const char*,...);
void TA_Input_RightStick(int*,int*);
void TA_Input_SetLeaderboard(unsigned enabled);
int TA_Input_LeaderboardScroll(void);
unsigned TA_Input_LeaderboardActions(void);
/* Display-only 512x224 canvas; only columns 224..511 are drawn. */
bool TA_Video_SetPartyOverlay(const uint32_t*,unsigned);
bool TA_Video_SetPartyVisual(const TaPartyVisual*);
void TA_Video_PartyStats(unsigned long out[4]);
void TA_Video_SetPartyBackdrop(const uint32_t*);
void TA_Video_SetDuelMode(unsigned mode);
void TA_Video_SetPopulation(unsigned accounts,unsigned ingame,unsigned online,unsigned known);
void TA_Input_Poll(uint16_t[2]);
void TA_Background_Set(void (*tick)(void));
void TA_Controls_Init(const char*directory);
unsigned TA_Controls_Get(unsigned player,unsigned action);
void TA_Controls_Bind(unsigned player,unsigned action,unsigned bit);
void TA_Controls_Reset(unsigned player);
bool TA_Controls_Save(void);
uint16_t TA_Input_GameButtons(uint16_t raw,unsigned player);

struct Ppu;
void TA_Video_BeginR126(struct Ppu*);
bool TA_Video_EndR126(struct Ppu*);
bool TA_Video_SubmitNative224Direct(const struct Ppu*);
bool TA_Video_SubmitActive224Direct(const struct Ppu*);
bool TA_Video_SubmitActive224(const unsigned char*);
void TA_Video_Submit(const unsigned char*);
bool TA_Video_SetLanOverlay(const uint32_t*,unsigned);
void TA_Video_ReleaseLanPpu(struct Ppu*);
void TA_Audio_Submit(const int16_t*);
void TA_LanGame_Run(const uint8_t*,unsigned);
void ta_android_frontend_close(void);
#ifdef __cplusplus
}
#endif
