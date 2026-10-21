/* R113 diagnostic-only CPU/bus coverage. No changes to SNES state or clocks.
 * Counters record host path frequency, not elapsed time and not FPS evidence.
 * Zero-cost when TA_R113_CPU_DIAG=0. */
#ifndef TA_R113_CPU_DIAG_H
#define TA_R113_CPU_DIAG_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct TaR113CpuCounters {
  uint32_t frame_dispatches;
  uint32_t reference_opcodes;
  uint32_t native_candidate_attempts;
  uint32_t native_decoded_accepts;
  uint32_t worker_attempts;
  uint32_t worker_accepts;
  uint32_t r112_entries;
  uint32_t r112_reject_cpu_rom;
  uint32_t r112_reject_bus_frame;
  uint32_t r112_accepted;
  uint32_t r112_opcodes;
  uint32_t r112_budget_exits;
  uint32_t bus_data_reads;
  uint32_t bus_data_writes;
  uint32_t bus_opcode_fetches;
  uint32_t bus_idles;
  uint32_t bus_wait_idles;
} TaR113CpuCounters;
void ta_r113_cpu_read_reset(TaR113CpuCounters *out);
#ifdef __cplusplus
}
#endif
#endif
