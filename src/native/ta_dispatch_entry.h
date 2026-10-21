/* R47: reject untranslated frame entries using the EXISTING R40 bank map.
 * The ROM guard and 256 bank pointers are exposed, not copied or expanded.
 * This lookup only reads CPU bank:PC; it changes no machine state or clocks.
 * Verified entries still execute through the original guarded native block.
 * Optional delay-loop descriptors keep their original dispatcher path. */
#ifndef TA_DISPATCH_ENTRY_H
#define TA_DISPATCH_ENTRY_H
#include <stdint.h>
#include <stdbool.h>
#include "cpu.h"
#if defined(XBOX_PORT) && defined(TA_R47_FAST_REJECT) && TA_R47_FAST_REJECT && TA_NATIVE_ENABLED && TA_R10_MICROBLOCKS && TA_R40_DECODED_DISPATCH && TA_R15_NATIVE_LOOKUP && TA_R46_CPU_DISPATCH && !TA_R32_NATIVE_DELAY_LOOP
#define TA_R47_REJECT_ON 1
extern bool ta_r47IdenticalRom;
extern const uint16_t* ta_r47NativeBanks[256];
static inline __attribute__((always_inline)) bool ta_r47_hasNative(const Cpu* cpu) {
  if(!ta_r47IdenticalRom || !(cpu->pc & 0x8000u)) return false;
  const uint16_t* row=ta_r47NativeBanks[cpu->k];
#if defined(TA_R62_NATIVE_SPRITE) && TA_R62_NATIVE_SPRITE
  return row && (row[cpu->pc & 0x7fffu] & (63u|8192u|16384u|32768u));
#elif (defined(TA_R58_TASK_PREFIX) && TA_R58_TASK_PREFIX) || (defined(TA_R58_IDLE_SLOTS) && TA_R58_IDLE_SLOTS) || (defined(TA_R58_COUNTER_BLOCK) && TA_R58_COUNTER_BLOCK)
  return row && (row[cpu->pc & 0x7fffu] & (63u|8192u|16384u));
#else
  return row && (row[cpu->pc & 0x7fffu] & 63u);
#endif
}
#else
#define TA_R47_REJECT_ON 0
#endif
#endif
