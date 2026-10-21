/* R48: exact-ROM quiet branch of the game service at $80:9086.
 * Enabled only when opcode observers are absent. Hardware events, work flags,
 * interrupts and unusual stack mappings retain the instruction executor. */
#ifndef TA_GAME_SERVICE_H
#define TA_GAME_SERVICE_H
#include "cpu.h"
#include "ta_dispatch_entry.h"
#if defined(XBOX_PORT) && defined(TA_R48_QUIET_SERVICE) && TA_R48_QUIET_SERVICE && TA_R47_REJECT_ON && TA_R21_BRANCH_CHAINS && TA_R24_LOOP_BACKEDGE && TA_R45_INLINE_CODE_FETCH && TA_R41_BUS_END_LIMIT && !TA_R32_DIAGNOSTICS && !TA_R2_SAMPLE_PC && !TA_R19_SAMPLE_EXACT && !defined(TA_HOST_TRACE) && !TA_R19_HOST_DEEP
#define TA_R48_SERVICE_ON 1
#if defined(TA_R48_HOST_COVERAGE) && TA_R48_HOST_COVERAGE
#include <stdint.h>
extern uint64_t ta_r48HostServiceHits;
extern uint64_t ta_r48HostRejects[11];
#endif
unsigned ta_native_r48_quiet_service(Cpu* cpu, unsigned opcodeBudget);
#else
#define TA_R48_SERVICE_ON 0
#endif
#endif
