#include "ta_360_platform.h"
extern "C" {
#include "snes.h"
#include "ppu.h"
#include "ta_native.h"
#include "ta_rom_sha256.h"
#include "ta_vs_lan_screen.h"
}
static char saveDirectory[128]="game:\\DinoBlockStacker360Data";
extern "C" const char*TA_360_SaveDirectory(void){return saveDirectory;}
static bool saveContainerMounted;
static void closeSaveContainer(void){
 if(saveContainerMounted){XContentFlush("DinoSave",NULL);XContentClose("DinoSave",NULL);saveContainerMounted=false;}
}
static bool probeStorage(const char*directory){
 if(!CreateDirectoryA(directory,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
 char path[180];int n=snprintf(path,sizeof path,"%s\\write-probe.tmp",directory);if(n<0||(unsigned)n>=sizeof path)return false;
 HANDLE file=CreateFileA(path,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(file==INVALID_HANDLE_VALUE)return false;
 const char expected[4]={'D','I','N','O'};char actual[4]={0};DWORD count=0;
 bool ok=WriteFile(file,expected,sizeof expected,&count,NULL)&&count==sizeof expected&&FlushFileBuffers(file);
 if(ok)ok=SetFilePointer(file,0,NULL,FILE_BEGIN)!=INVALID_SET_FILE_POINTER&&ReadFile(file,actual,sizeof actual,&count,NULL)&&count==sizeof actual&&!memcmp(expected,actual,sizeof actual);
 if(!CloseHandle(file))ok=false;DeleteFileA(path);
 if(ok)strcpy(saveDirectory,directory);return ok;
}
static bool mountSaveContainer(void){
 /* Use the normal Xbox saved-game device instead of writing into a disc or
    read-only game mount. Xenia implements this as writable host content. */
 DWORD user=XUSER_INDEX_NONE;
 for(DWORD i=0;i<XUSER_MAX_COUNT;i++)if(XUserGetSigninState(i)!=eXUserSigninState_NotSignedIn){user=i;break;}
 ULARGE_INTEGER bytes;bytes.QuadPart=64ull*1024ull*1024ull;
 XCONTENTDEVICEID device=XCONTENTDEVICE_ANY;XDEVICE_DATA info;memset(&info,0,sizeof info);
 /* Probe the normal first storage device before invoking a selector. This
    avoids an unnecessary device-selection dialog on Xenia's virtual HDD. */
 DWORD status=XContentGetDeviceData(1,&info);
 if(status==ERROR_SUCCESS&&(info.DeviceType==XCONTENTDEVICETYPE_HDD||info.DeviceType==XCONTENTDEVICETYPE_MU))device=info.DeviceID;
 if(device==XCONTENTDEVICE_ANY){
  XOVERLAPPED operation;memset(&operation,0,sizeof operation);operation.hEvent=CreateEvent(NULL,FALSE,FALSE,NULL);if(!operation.hEvent)return false;
  status=XShowDeviceSelectorUI(user,XCONTENTTYPE_SAVEDGAME,0,bytes,&device,&operation);
  if(status==ERROR_IO_PENDING){
   static uint32_t waiting[512*480];memset(waiting,0,sizeof waiting);ta_vs_lan_text_box(waiting,34,120,444,80,"SELECT SAVE STORAGE",0x0096e3b2,3);
   while(!XHasOverlappedIoCompleted(&operation)){TA_Video_Submit((const BYTE*)waiting);Sleep(1);}
   status=XGetOverlappedResult(&operation,NULL,FALSE);
  }
  CloseHandle(operation.hEvent);if(status!=ERROR_SUCCESS||device==XCONTENTDEVICE_ANY)return false;
 }

 XCONTENT_DATA content;memset(&content,0,sizeof content);content.DeviceID=device;content.dwContentType=XCONTENTTYPE_SAVEDGAME;
 strcpy(content.szFileName,"DinoBlockStacker360Data");const WCHAR name[]=L"Dino Block Stacker";memcpy(content.szDisplayName,name,sizeof name);
 DWORD disposition=0;status=XContentCreateEx(user,"DinoSave",&content,XCONTENTFLAG_OPENALWAYS,&disposition,NULL,128u*1024u,bytes,NULL);
 if(status!=ERROR_SUCCESS)return false;saveContainerMounted=true;
 if(!probeStorage("DinoSave:\\Data")){closeSaveContainer();return false;}
 XContentFlush("DinoSave",NULL);atexit(closeSaveContainer);return true;
}
static bool chooseStorage(void){
 /* Retain existing homebrew saves when the launch folder is writable. */
 const char*roots[]={"game:\\DinoBlockStacker360Data","D:\\DinoBlockStacker360Data","Hdd1:\\DinoBlockStacker360Data","Usb0:\\DinoBlockStacker360Data","Usb1:\\DinoBlockStacker360Data"};
 for(unsigned i=0;i<sizeof roots/sizeof roots[0];i++)if(probeStorage(roots[i]))return true;
 return mountSaveContainer();
}

static void showError(const char*detail){
 static uint32_t frame[512*480];memset(frame,0,sizeof frame);
 for(;;){uint16_t pads[2];TA_Input_Poll(pads);if(pads[0]&0x100){closeSaveContainer();XLaunchNewImage(NULL,0);}
  ta_vs_lan_text_box(frame,34,50,444,30,"DINO BLOCK STACKER",0x007dd9e8,4);
  ta_vs_lan_text_box(frame,34,132,444,190,detail,0x00f1f3f8,3);
  ta_vs_lan_text_box(frame,34,410,444,20,"B EXIT",0x00f1a0aa,2);TA_Video_Submit((const BYTE*)frame);
 }
}
void __cdecl main(void){
 TA_Input_Init();if(FAILED(TA_Video_Init()))XLaunchNewImage(NULL,0);
 if(FAILED(TA_Audio_Init()))showError("AUDIO INITIALIZATION FAILED");
 if(!chooseStorage())showError("SAVE STORAGE UNAVAILABLE. CHECK THE STORAGE DEVICE OR SIGN IN TO A LOCAL CONSOLE PROFILE.");
 static uint8_t rom[1048576];const char*names[]={"game:\\game.sfc","D:\\game.sfc"};FILE*f=NULL;
 for(unsigned i=0;i<2&&!f;i++)f=fopen(names[i],"rb");
 bool valid=f&&fread(rom,1,sizeof rom,f)==sizeof rom&&fgetc(f)==EOF;if(f)fclose(f);
 if(!valid||!ta_r60_rom_sha256(rom,sizeof rom))showError("PLACE THE COMPATIBLE 1 MB UNHEADERED GAME.SFC BESIDE DEFAULT.XEX");
 for(;;){TA_LanGame_Run(rom,sizeof rom);TA_360_FrontendClose();
 Snes*s=snes_init();if(!s||!snes_loadRom(s,rom,sizeof rom)||!ta_native_init(rom,sizeof rom))showError("GAME INITIALIZATION FAILED");
 snes_setPixelFormat(s,pixelFormatXRGB);static int16_t pcm[1600];static uint32_t frame[512*224];
 unsigned returnHeld=0;for(;;){uint16_t pads[2];TA_Input_Poll(pads);if((pads[0]&12u)==12u){if(++returnHeld>=45u)break;}else returnHeld=0;for(unsigned p=0;p<2;p++){uint16_t mapped=TA_Input_GameButtons(pads[p],p);for(unsigned b=0;b<12;b++)snes_setButtonState(s,p+1,b,(mapped>>b)&1);}
  snes_runFrame(s);if(!TA_Video_SubmitActive224Direct(s->ppu)){static uint8_t full[512*480*4];snes_setPixels(s,full);TA_Video_Submit(full);}
  snes_setSamples(s,pcm,800);TA_Audio_Submit(pcm);
 }snes_free(s);TA_Video_SetPartyVisual(NULL);TA_Video_SetLanOverlay(NULL,0);
 }
}
