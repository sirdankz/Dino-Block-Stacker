#include "ta_pc_internal.h"
#include "ta_options.h"
#include <SDL3/SDL_main.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <memory>
extern "C" {
#include "snes.h"
#include "ppu.h"
#include "ta_native.h"
#include "ta_rom_sha256.h"
}
namespace fs=std::filesystem;
static void message(const char*text,SDL_MessageBoxFlags flags=SDL_MESSAGEBOX_ERROR){SDL_ShowSimpleMessageBox(flags,"Dino Block Stacker",text,pcWindow);}
static bool readRom(const fs::path&path,std::vector<uint8_t>&rom){std::error_code ec;if(fs::file_size(path,ec)!=1048576||ec)return false;std::ifstream in(path,std::ios::binary);rom.resize(1048576);return in.read((char*)rom.data(),rom.size())&&ta_r60_rom_sha256(rom.data(),(unsigned)rom.size());}
static bool importRom(std::vector<uint8_t>&rom){
 wchar_t name[32768]={0};OPENFILENAMEW picker={};picker.lStructSize=sizeof picker;picker.hwndOwner=(HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(pcWindow),SDL_PROP_WINDOW_WIN32_HWND_POINTER,NULL);picker.lpstrFilter=L"SNES game (*.sfc;*.smc)\0*.sfc;*.smc\0All files\0*.*\0\0";picker.lpstrFile=name;picker.nMaxFile=32768;picker.lpstrTitle=L"Select Tetris Attack (USA) - unheadered 1 MB ROM";picker.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 for(;;){if(!GetOpenFileNameW(&picker))return false;if(!readRom(name,rom)){message("The selected file is not the supported 1 MB unheadered Tetris Attack (USA) ROM.");continue;}
  fs::path saved=fs::path(pcWide(TA_PC_SaveDirectory()))/L"game.sfc",temporary=saved;temporary+=L".tmp";std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out.write((char*)rom.data(),rom.size());out.close();
  if(!out||!MoveFileExW(temporary.c_str(),saved.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::error_code ec;fs::remove(temporary,ec);throw std::runtime_error("The selected ROM could not be saved to LocalAppData.");}return true;
 }
}
struct Offline {
 Snes*s=NULL;fs::path battery;
 explicit Offline(const std::vector<uint8_t>&rom){s=snes_init();if(!s)throw std::runtime_error("Game initialization failed.");if(!snes_loadRom(s,rom.data(),(int)rom.size())||!ta_native_init(rom.data(),(unsigned)rom.size())){snes_free(s);s=NULL;throw std::runtime_error("Game initialization failed.");}snes_setPixelFormat(s,pixelFormatXRGB);battery=fs::path(pcWide(TA_PC_SaveDirectory()))/L"offline.srm";
  std::error_code ec;auto bytes=fs::file_size(battery,ec);if(!ec&&bytes>0&&bytes<=1048576){std::ifstream in(battery,std::ios::binary);std::vector<uint8_t> data((size_t)bytes);if(in.read((char*)data.data(),data.size()))snes_loadBattery(s,data.data(),(int)data.size());}
 }
 void save(){int n=snes_saveBattery(s,NULL);if(n<=0||n>1048576)return;std::vector<uint8_t> data(n);if(snes_saveBattery(s,data.data())!=n)return;auto temp=battery;temp+=L".tmp";std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write((char*)data.data(),data.size());out.close();if(out)MoveFileExW(temp.c_str(),battery.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
 ~Offline(){if(s){try{save();}catch(...){}snes_free(s);}}
 void run(){static int16_t pcm[1600];static uint8_t full[512*480*4];unsigned returnHeld=0,frames=0;
  for(;;){uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12u)==12u){if(++returnHeld>=45)break;}else returnHeld=0;
   for(unsigned p=0;p<2;p++){uint16_t mapped=TA_Input_GameButtons(pads[p],p);for(unsigned b=0;b<12;b++)snes_setButtonState(s,p+1,b,(mapped>>b)&1);}
   snes_runFrame(s);if(!TA_Video_SubmitActive224Direct(s->ppu)){snes_setPixels(s,full);TA_Video_Submit(full);}snes_setSamples(s,pcm,800);TA_Audio_Submit(pcm);if(++frames%1800==0)save();
  }
 }
};
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
 SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
 SDL_SetMainReady();bool winsock=false,timer=false;int result=0;
 try{
  SDL_SetAppMetadata("Dino Block Stacker","1.3.0","org.dinoblockstacker.pc");if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMEPAD))throw std::runtime_error(SDL_GetError());
  pcStorageInit();TA_Options_Init(TA_PC_SaveDirectory());WSADATA data;if(WSAStartup(MAKEWORD(2,2),&data))throw std::runtime_error("Windows networking initialization failed.");winsock=true;pcNetworkInit();timer=timeBeginPeriod(1)==TIMERR_NOERROR;
  if(FAILED(TA_Video_Init()))throw std::runtime_error(SDL_GetError());TA_Input_Init();if(FAILED(TA_Audio_Init()))message("Audio output is unavailable. The game will continue without sound.",SDL_MESSAGEBOX_WARNING);
  std::vector<uint8_t> rom;fs::path requested;int argc=0;LPWSTR*argv=CommandLineToArgvW(GetCommandLineW(),&argc);
  if(argv){for(int i=1;i<argc;i++)if(wcscmp(argv[i],L"--rom")==0&&i+1<argc)requested=argv[++i];else if(wcscmp(argv[i],L"--frame-timings")==0)pcSetFrameTimings(true);LocalFree(argv);}
  bool ready=false;if(!requested.empty()){ready=readRom(requested,rom);if(!ready)message("The file supplied with --rom is not the supported game ROM.");}
  if(!ready)ready=readRom(fs::path(pcWide(TA_PC_SaveDirectory()))/L"game.sfc",rom);if(!ready)ready=readRom(fs::path(pcWide(SDL_GetBasePath()))/L"game.sfc",rom);if(!ready)ready=importRom(rom);
  while(ready){try{TA_LanGame_Run(rom.data(),(unsigned)rom.size());TA_PC_FrontendClose();TA_Video_SetPartyVisual(NULL);TA_Video_SetLanOverlay(NULL,0);Offline game(rom);game.run();}
   catch(PcRequest request){TA_PC_ShutdownFrontend();TA_Video_SetPartyVisual(NULL);TA_Video_SetLanOverlay(NULL,0);if(request==PcRequest::Quit)break;ready=importRom(rom);}
  }
 }catch(PcRequest){/* Closing the window also exits startup and text entry. */}
 catch(const std::exception&e){message(e.what());result=1;}
 TA_PC_ShutdownFrontend();pcSaveWindow();TA_Audio_Shutdown();TA_Input_Shutdown();TA_Video_Shutdown();if(timer)timeEndPeriod(1);pcNetworkShutdown();if(winsock)WSACleanup();SDL_Quit();return result;
}
