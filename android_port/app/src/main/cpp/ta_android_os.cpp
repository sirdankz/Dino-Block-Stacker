#include "ta_android_platform.h"
#include <chrono>
#include <sys/stat.h>
#include <sys/time.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
static thread_local DWORD profileError;
static void profileErr(void);
using Clock=std::chrono::steady_clock;
extern "C" DWORD GetTickCount(void){return (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();}
extern "C" void GetSystemTimeAsFileTime(FILETIME*f){struct timeval t;gettimeofday(&t,NULL);uint64_t v=((uint64_t)t.tv_sec+11644473600ULL)*10000000ULL+(uint64_t)t.tv_usec*10;f->dwLowDateTime=(DWORD)v;f->dwHighDateTime=(DWORD)(v>>32);}
extern "C" BOOL MoveFileExA(const char*a,const char*b,DWORD){return rename(a,b)==0;}
extern "C" BOOL CreateDirectoryA(const char*p,void*){if(mkdir(p,0700)==0||errno==EEXIST)return 1;profileErr();return 0;}
extern "C" int XNetRandom(BYTE*p,unsigned n){int fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);if(fd<0)return -1;unsigned offset=0;while(offset<n){ssize_t k=read(fd,p+offset,n-offset);if(k<0&&errno==EINTR)continue;if(k<=0){close(fd);return -1;}offset+=(unsigned)k;}close(fd);return 0;}
extern "C" DWORD XNetGetTitleXnAddr(XNADDR*a){a->ina.s_addr=0;struct ifaddrs*list=NULL;if(getifaddrs(&list))return 0;for(struct ifaddrs*i=list;i;i=i->ifa_next){if(!i->ifa_addr||i->ifa_addr->sa_family!=AF_INET||!(i->ifa_flags&IFF_UP)||(i->ifa_flags&IFF_LOOPBACK))continue;uint32_t addr=((struct sockaddr_in*)i->ifa_addr)->sin_addr.s_addr;if(!addr)continue;if(!a->ina.s_addr)a->ina.s_addr=addr;if(!strncmp(i->ifa_name,"wlan",4)||!strncmp(i->ifa_name,"eth",3)){a->ina.s_addr=addr;break;}}freeifaddrs(list);return a->ina.s_addr?XNET_GET_XNADDR_DHCP:0;}

/* Portable implementation of the shared profile store's small native-file API.
 * Handles are private POSIX descriptors; this code never opens external files. */
static void profileErr(void){profileError=(errno==ENOENT||errno==ENOTDIR)?ERROR_PATH_NOT_FOUND:errno==EACCES?ERROR_ACCESS_DENIED:errno==EEXIST?ERROR_ALREADY_EXISTS:ERROR_WRITE_FAULT;}
extern "C" DWORD GetLastError(void){return profileError;}
extern "C" HANDLE CreateFileA(const char*p,DWORD access,DWORD,void*,DWORD create,DWORD,HANDLE){int flags=(access&GENERIC_WRITE)?O_WRONLY:O_RDONLY;if(create==CREATE_ALWAYS)flags|=O_CREAT|O_TRUNC;int fd=open(p,flags|O_CLOEXEC,0600);if(fd<0)profileErr();return (HANDLE)fd;}
extern "C" DWORD GetFileSize(HANDLE h,DWORD*high){struct stat st;if(fstat((int)h,&st)){profileErr();return UINT32_MAX;}if(high)*high=(DWORD)((uint64_t)st.st_size>>32);return (DWORD)st.st_size;}
extern "C" BOOL ReadFile(HANDLE h,void*p,DWORD n,DWORD*out,void*){ssize_t got;do{got=read((int)h,p,n);}while(got<0&&errno==EINTR);if(got<0){profileErr();*out=0;return 0;}*out=(DWORD)got;return 1;}
extern "C" BOOL WriteFile(HANDLE h,const void*p,DWORD n,DWORD*out,void*){ssize_t got;do{got=write((int)h,p,n);}while(got<0&&errno==EINTR);if(got<0){profileErr();*out=0;return 0;}*out=(DWORD)got;return 1;}
extern "C" BOOL FlushFileBuffers(HANDLE h){if(fsync((int)h)){profileErr();return 0;}return 1;}
extern "C" BOOL CloseHandle(HANDLE h){if(close((int)h)){profileErr();return 0;}return 1;}
extern "C" BOOL DeleteFileA(const char*p){if(unlink(p)){profileErr();return 0;}return 1;}
