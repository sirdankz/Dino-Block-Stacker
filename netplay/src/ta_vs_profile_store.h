#ifndef TA_VS_PROFILE_STORE_H
#define TA_VS_PROFILE_STORE_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Xbox/Win32 native HDD I/O. Never stores a password; payload is the existing
 * TAP14 name/resume-token record. Two checked slots avoid FATX rename/replace. */
#define TA_PS_PAYLOAD 392u
#define TA_PS_RECORD 416u
static uint32_t ta_ps_get(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void ta_ps_put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t ta_ps_crc(const uint8_t*p){uint32_t c=~0u;for(unsigned i=0;i<TA_PS_RECORD;i++){c^=(i>=16&&i<20)?0:p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}return ~c;}
static int ta_ps_payload(const uint8_t*p){
 if(memcmp(p,"TAP14",5)||p[5]!=1||p[6]>8||(p[6]?p[7]>=p[6]:p[7]!=0))return 0;
 for(unsigned i=0;i<p[6];i++){const uint8_t*q=p+8+48*i;unsigned k=0,nonzero=0;for(;k<16&&q[k];k++)if(!((q[k]>='A'&&q[k]<='Z')||(q[k]>='0'&&q[k]<='9')||q[k]=='-'||q[k]=='_'))return 0;if(k<3||k==16)return 0;for(k=16;k<48;k++)nonzero|=q[k];if(!nonzero)return 0;}return 1;
}
static int ta_ps_path(char*out,unsigned cap,const char*dir,const char*name){
 #if defined(TA_ANDROID)
 int n=snprintf(out,cap,"%s/%s",dir,name);if(n<0||(unsigned)n>=cap)return 0;return 1;
#else
 int n=snprintf(out,cap,"%s\\%s",dir,name);if(n<0||(unsigned)n>=cap)return 0;for(unsigned i=0;out[i];i++)if(out[i]=='/')out[i]='\\';return 1;
#endif
}
static int ta_ps_read(const char*path,uint8_t*out,unsigned size,DWORD*error){
 HANDLE f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE){*error=GetLastError();return 0;}
 DWORD high=0,length=GetFileSize(f,&high);int ok=!high&&length==size;unsigned at=0;
 while(ok&&at<size){DWORD n=0;if(!ReadFile(f,out+at,size-at,&n,NULL)||!n){*error=GetLastError();ok=0;}else at+=n;}
 if(!ok&&!*error)*error=ERROR_INVALID_DATA;
 CloseHandle(f);return ok;
}
static int ta_ps_slot(const char*dir,unsigned slot,uint8_t*out,DWORD*error){
 char path[192];if(!ta_ps_path(path,sizeof path,dir,slot?"n20-profiles-b.dat":"n20-profiles-a.dat")){*error=ERROR_FILENAME_EXCED_RANGE;return 0;}
 if(!ta_ps_read(path,out,TA_PS_RECORD,error))return 0;
 if(memcmp(out,"TAP20\1\0\0",8)||ta_ps_get(out+12)!=TA_PS_PAYLOAD||ta_ps_get(out+16)!=ta_ps_crc(out)||!ta_ps_payload(out+24)){*error=ERROR_INVALID_DATA;return 0;}return 1;
}
static unsigned ta_ps_latest(int a,int b,const uint8_t*x,const uint8_t*y){return b&&(!a||(int32_t)(ta_ps_get(y+8)-ta_ps_get(x+8))>0);}
/* Returns source1=A,2=B,3=legacy,0=unavailable/invalid. */
static int ta_profile_store_load(const char*dir,uint8_t*out,DWORD*error){
 uint8_t a[TA_PS_RECORD],b[TA_PS_RECORD];*error=0;int av=ta_ps_slot(dir,0,a,error),bv=ta_ps_slot(dir,1,b,error);
 if(av||bv){unsigned last=ta_ps_latest(av,bv,a,b);memcpy(out,(last?b:a)+24,TA_PS_PAYLOAD);*error=0;return (int)last+1;}
 char path[192];if(!ta_ps_path(path,sizeof path,dir,"n14-profiles.dat")){*error=ERROR_FILENAME_EXCED_RANGE;return 0;}
 if(ta_ps_read(path,out,TA_PS_PAYLOAD,error)&&ta_ps_payload(out)){*error=0;return 3;}return 0;
}
static int ta_ps_write(const char*dir,unsigned slot,const uint8_t*record,DWORD*error){
 char path[192];if(!ta_ps_path(path,sizeof path,dir,slot?"n20-profiles-b.dat":"n20-profiles-a.dat")){*error=ERROR_FILENAME_EXCED_RANGE;return 0;}
 HANDLE f=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);if(f==INVALID_HANDLE_VALUE){*error=GetLastError();return 0;}
 unsigned at=0;int ok=1;while(at<TA_PS_RECORD){DWORD n=0;if(!WriteFile(f,record+at,TA_PS_RECORD-at,&n,NULL)||!n){*error=GetLastError();ok=0;break;}at+=n;}
 if(ok&&!FlushFileBuffers(f)){*error=GetLastError();ok=0;}if(!CloseHandle(f)&&ok){*error=GetLastError();ok=0;}if(!ok){DeleteFileA(path);return 0;}
 uint8_t check[TA_PS_RECORD];if(!ta_ps_slot(dir,slot,check,error)||memcmp(check,record,TA_PS_RECORD)){if(!*error)*error=ERROR_INVALID_DATA;DeleteFileA(path);return 0;}return 1;
}
static int ta_profile_store_save(const char*dir,const uint8_t*payload,DWORD*error){
 *error=0;if(!ta_ps_payload(payload)){*error=ERROR_INVALID_DATA;return 0;}
 char path[192];if(!ta_ps_path(path,sizeof path,dir,"")){*error=ERROR_FILENAME_EXCED_RANGE;return 0;}size_t len=strlen(path);if(len&&path[len-1]=='\\')path[len-1]=0;
 if(!CreateDirectoryA(path,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS){*error=GetLastError();return 0;}
 uint8_t a[TA_PS_RECORD],b[TA_PS_RECORD];int av=ta_ps_slot(dir,0,a,error),bv=ta_ps_slot(dir,1,b,error);unsigned last=ta_ps_latest(av,bv,a,b),slot=(av||bv)?1-last:0;
 uint32_t generation=(av||bv)?ta_ps_get((last?b:a)+8)+1:1;uint8_t record[TA_PS_RECORD];memset(record,0,sizeof record);memcpy(record,"TAP20\1\0\0",8);ta_ps_put(record+8,generation);ta_ps_put(record+12,TA_PS_PAYLOAD);memcpy(record+24,payload,TA_PS_PAYLOAD);ta_ps_put(record+16,ta_ps_crc(record));*error=0;
 if(!ta_ps_write(dir,slot,record,error))return 0;
 if(!av&&!bv){DWORD ignored=0;ta_ps_write(dir,1-slot,record,&ignored);}*error=0;return 1;
}
#endif
