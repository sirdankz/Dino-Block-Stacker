#include "ta_pc_internal.h"
#include <bcrypt.h>
#include <iphlpapi.h>
#include <shlobj.h>
#include <filesystem>
#include <stdexcept>
static std::string saveDirectory;
std::wstring pcWide(const std::string&s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),NULL,0);if(!n)throw std::runtime_error("Invalid UTF-8 path.");std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),w.data(),n);return w;}
std::string pcUtf8(const std::wstring&w){if(w.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),NULL,0,NULL,NULL);std::string s(n,0);WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),s.data(),n,NULL,NULL);return s;}
extern "C" const char*TA_PC_SaveDirectory(void){return saveDirectory.c_str();}
void pcStorageInit(){
 PWSTR base=NULL;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,NULL,&base)))throw std::runtime_error("Windows save storage is unavailable.");
 std::filesystem::path dir=std::filesystem::path(base)/L"DinoBlockStacker";CoTaskMemFree(base);std::filesystem::create_directories(dir);saveDirectory=pcUtf8(dir.wstring());
 auto probe=dir/L"write-probe.tmp";HANDLE f=CreateFileW(probe.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
 if(f==INVALID_HANDLE_VALUE)throw std::runtime_error("Dino Block Stacker cannot write its LocalAppData save folder.");CloseHandle(f);
}
extern "C" int XNetRandom(BYTE*out,unsigned bytes){return BCryptGenRandom(NULL,out,bytes,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0?-1:0;}
extern "C" DWORD XNetGetTitleXnAddr(XNADDR*out){
 static uint32_t address;static DWORD last;DWORD now=GetTickCount();
 if(!last||(DWORD)(now-last)>3000){last=now;address=0;
  // A UDP route lookup selects the interface Windows uses for the relay. No packet is sent.
  SOCKET s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(s!=INVALID_SOCKET){sockaddr_in dst={};dst.sin_family=AF_INET;dst.sin_port=htons(34987);InetPtonA(AF_INET,"172.233.145.244",&dst.sin_addr);
   if(connect(s,(sockaddr*)&dst,sizeof dst)==0){sockaddr_in local={};int len=sizeof local;if(getsockname(s,(sockaddr*)&local,&len)==0)address=local.sin_addr.s_addr;}closesocket(s);
  }
  if(!address){ULONG size=16384;std::vector<unsigned char> bytes(size);ULONG status=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,NULL,(IP_ADAPTER_ADDRESSES*)bytes.data(),&size);
   if(status==ERROR_BUFFER_OVERFLOW){bytes.resize(size);status=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,NULL,(IP_ADAPTER_ADDRESSES*)bytes.data(),&size);}
   if(status==NO_ERROR)for(auto a=(IP_ADAPTER_ADDRESSES*)bytes.data();a&&!address;a=a->Next)if(a->OperStatus==IfOperStatusUp&&a->IfType!=IF_TYPE_SOFTWARE_LOOPBACK)for(auto u=a->FirstUnicastAddress;u;u=u->Next)if(u->Address.lpSockaddr->sa_family==AF_INET){auto ip=((sockaddr_in*)u->Address.lpSockaddr)->sin_addr.s_addr;if(ip&&((ip&255)!=127)){address=ip;break;}}
  }
 }
 memset(out,0,sizeof *out);out->ina.s_addr=address;return address?XNET_GET_XNADDR_STATIC:0;
}
static SDL_AudioStream* audioStream;
extern "C" HRESULT TA_Audio_Init(void){SDL_AudioSpec spec={SDL_AUDIO_S16,2,48000};audioStream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,NULL,NULL);return audioStream&&SDL_ResumeAudioStreamDevice(audioStream)?S_OK:E_FAIL;}
extern "C" void TA_Audio_Submit(const int16_t*p){if(!audioStream)return;if(SDL_GetAudioStreamQueued(audioStream)>3200*5)SDL_ClearAudioStream(audioStream);SDL_PutAudioStreamData(audioStream,p,3200);}
extern "C" void TA_Audio_Shutdown(void){if(audioStream)SDL_DestroyAudioStream(audioStream);audioStream=NULL;}
