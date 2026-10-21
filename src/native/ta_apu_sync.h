#ifndef TA_APU_SYNC_H
#define TA_APU_SYNC_H
#include <stdint.h>
/* Exact debt-to-clock conversion. Common port polling fits one x86 DIV;
   overdue batches retain the full-width reference division. */
static inline int ta_apu_wanted_cycles(int64_t debt, uint32_t denominator) {
  if(debt < (int64_t)denominator) return 0;
  if((uint64_t)debt <= UINT32_MAX)
    return (int)((uint32_t)debt / denominator);
  return (int)(debt / (int64_t)denominator);
}
#endif
