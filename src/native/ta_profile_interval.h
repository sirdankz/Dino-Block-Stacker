/* R58: keep fallback samples for the complete 600-frame report window.
 * Call these policies at ordinary logging boundaries; other profiler modes
 * retain their existing reset after every ordinary log interval. */
#ifndef TA_PROFILE_INTERVAL_H
#define TA_PROFILE_INTERVAL_H
#include <stdint.h>
static inline int ta_profile_interval_report_due(uint32_t frame) {
  return frame % 600u == 0u;
}
static inline int ta_profile_interval_reset_due(uint32_t frame) {
#if defined(TA_R56_FALLBACK_PC_PROFILE) && TA_R56_FALLBACK_PC_PROFILE
  return ta_profile_interval_report_due(frame);
#else
  (void)frame;
  return 1;
#endif
}
#endif
