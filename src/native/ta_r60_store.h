/* One runtime-confirmed handler, not the seven-table dispatcher. */
#ifndef TA_R60_STORE_H
#define TA_R60_STORE_H
#include "ta_game_service.h"
#include "ta_dispatch_trace.h"
#ifndef TA_R60_NATIVE_STORE
#define TA_R60_NATIVE_STORE 0
#endif
#if TA_R48_SERVICE_ON && TA_R60_NATIVE_STORE && !TA_R60_DISPATCH_TRACE
#define TA_R60_STORE_ON 1
unsigned ta_native_r60_store(Cpu* cpu,unsigned opcodeBudget);
#if defined(TA_R60_HOST_COVERAGE) && TA_R60_HOST_COVERAGE
extern uint64_t ta_r60HostStoreHits;
#endif
#else
#define TA_R60_STORE_ON 0
#endif
#endif
