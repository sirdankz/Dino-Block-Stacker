/* Sparse Xbox runtime log. D: is writable only for loose HDD launches; ISO falls back to T:/E:/U:. */
#include "ta_platform.h"
#include <stdarg.h>
static FILE *g_log=NULL;
static char g_log_path[96]={0};
static void ta_log_open(void){
  if(g_log)return;
  const char *paths[]={"D:\\ta-native.log","T:\\ta-native.log","E:\\ta-native.log","U:\\ta-native.log"};
  for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);i++){
    FILE *f=fopen(paths[i],"wb");
    if(f){g_log=f;snprintf(g_log_path,sizeof(g_log_path),"%s",paths[i]);
      fprintf(g_log,"[TA R14] Runtime log opened: %s\n",g_log_path);
      fflush(g_log);
      DbgPrint("TA-XBOX: log opened: %s\n",g_log_path);
      break;
    }
  }
  if(!g_log){DbgPrint("TA-XBOX: writable D:/T:/E:/U: log unavailable; debug prints only\n");}
}
extern "C" void TA_Log(const char *fmt,...){
#if defined(TA_R84_COMPOSE_PROFILE) && TA_R84_COMPOSE_PROFILE
  char line[1024];
#else
  char line[384];
#endif
  va_list args;va_start(args,fmt);
  vsnprintf(line,sizeof(line),fmt,args);va_end(args);
  line[sizeof(line)-1]=0;
  DbgPrint("TA-XBOX: %s",line);
  if(!g_log)ta_log_open();
  if(g_log){fputs(line,g_log);fflush(g_log);}
}
extern "C" const char *TA_LogPath(void){return g_log_path[0]?g_log_path:"(debug only)";}
extern "C" void TA_LogClose(void){if(g_log){fflush(g_log);fclose(g_log);g_log=NULL;}}
