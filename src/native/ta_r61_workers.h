/* R61: runtime-selected four-word WRAM workers only. */
#ifndef TA_R61_WORKERS_H
#define TA_R61_WORKERS_H
#include "ta_r60_store.h"
#ifndef TA_R61_NATIVE_WORKERS
#define TA_R61_NATIVE_WORKERS 0
#endif
#ifndef TA_R61_WORKER_COVERAGE
#define TA_R61_WORKER_COVERAGE 0
#endif
#if TA_R48_SERVICE_ON && TA_R61_NATIVE_WORKERS && !TA_R60_DISPATCH_TRACE
#define TA_R61_WORKERS_ON 1
/* Cheap rejection before a function call. Membership is checked inside. */
static inline bool ta_r61_maybe_worker(uint16_t pc){return pc==0x87ac || (pc>=0x87c1 && pc<=0x8d43 && (pc-0x87c1)%30u==0);}
unsigned ta_native_r61_worker(Cpu* cpu,unsigned budget);
unsigned ta_native_r61_worker_count(void);
uint16_t ta_native_r61_worker_pc(unsigned ordinal);
#if TA_R61_WORKER_COVERAGE
uint32_t ta_native_r61_worker_hits(unsigned ordinal);
#endif
#else
#define TA_R61_WORKERS_ON 0
#endif
#endif
