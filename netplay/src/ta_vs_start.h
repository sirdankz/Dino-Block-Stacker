#ifndef TA_VS_START_H
#define TA_VS_START_H
#include <stddef.h>
#include "snes.h"
#include "ta_vs_preview.h"
#ifdef __cplusplus
extern "C" {
#endif
#define TA_START_CAPACITY 300000u
/* Process-local menu templates, never a live-board rollback mechanism. */
typedef struct{uint8_t data[TA_START_CAPACITY];uint32_t bytes,frame,masterPending;int64_t cycleDebt;}TaVsStart;
int ta_vs_start_save(TaVsStart*,Snes*,unsigned frame);
int ta_vs_start_load(const TaVsStart*,Snes*);
int ta_vs_start_seed(Snes*,uint32_t seed);
int ta_vs_start_read_frame(const char*,const uint8_t romHash[32],unsigned frame,TaVsStart*,TaVsPreview*);
int ta_vs_start_read(const char*,const uint8_t romHash[32],TaVsStart*,TaVsPreview*);
int ta_vs_start_read_memory(const uint8_t*,size_t,const uint8_t romHash[32],unsigned frame,TaVsStart*,TaVsPreview*);
int ta_vs_start_write(const char*,const uint8_t romHash[32],const TaVsStart*,const TaVsPreview*);
#ifdef __cplusplus
}
#endif
#endif
