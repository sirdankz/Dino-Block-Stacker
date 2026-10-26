#include "ta_pc_internal.h"
#include <bcrypt.h>
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
