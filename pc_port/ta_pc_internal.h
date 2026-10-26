#pragma once
#include "ta_pc_platform.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <stdexcept>
enum class PcRequest { Quit, Import };
extern SDL_Window* pcWindow;
extern SDL_Renderer* pcRenderer;
extern bool pcMenuFrame,pcTextMode;
extern std::string pcTyped;
extern bool pcTextSubmit,pcTextCancel,pcTextErase;
std::wstring pcWide(const std::string&);
std::string pcUtf8(const std::wstring&);
void pcStorageInit();
void pcEvents();
void pcMenuCommit();
void pcMouse(float,float,unsigned);
void pcMouseWheel(int);
void pcMenuPoint(float,float,float&,float&);
void pcPace();
void pcSaveWindow();
