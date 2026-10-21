/* R60 bounded exact execution evidence. No hooks/data in clean builds. */
#ifndef TA_DISPATCH_TRACE_H
#define TA_DISPATCH_TRACE_H
#include "cpu.h"
#ifndef TA_R60_DISPATCH_TRACE
#define TA_R60_DISPATCH_TRACE 0
#endif
#if TA_R60_DISPATCH_TRACE
#ifdef __cplusplus
extern "C" {
#endif
void ta_r60_trace_reset(void);
void ta_r60_trace_before(const Cpu* cpu);
void ta_r60_trace_jsr(const Cpu* cpu, uint16_t site, uint16_t index);
void ta_r60_trace_report(void (*emit)(const char*));
#ifdef __cplusplus
}
#endif
#endif
#endif
