#ifndef TA_VS_PROFILE_LOCATION_H
#define TA_VS_PROFILE_LOCATION_H
#include "ta_vs_profile_store.h"
/* U: is RXDK's persistent per-title UDATA mapping. E: is a dashboard alias,
 * not a guaranteed title mapping. Never use the disc or utility/cache drive. */
#ifndef TA_VS_PROFILE_DIR
#define TA_VS_PROFILE_DIR "U:\\TA_NATIVE_VS"
#endif
/* Prefer current save; migrate a readable old E: save only when no current
 * valid record exists. Old data is never deleted. A failed migration keeps the
 * loaded token usable in memory and is reported by the caller. */
static int ta_profile_location_load(const char*primary,const char*legacy,uint8_t*out,DWORD*error,unsigned*migrated){
 *migrated=0;int source=ta_profile_store_load(primary,out,error);if(source||!strcmp(primary,legacy))return source;
 DWORD oldError=0;source=ta_profile_store_load(legacy,out,&oldError);if(!source)return 0;
 *migrated=ta_profile_store_save(primary,out,error)?1u:2u;return source+3;
}
#endif
