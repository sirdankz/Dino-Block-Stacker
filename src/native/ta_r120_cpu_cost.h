/* R120 diagnostic-only reference interpreter cost sampling. Not an FPS configuration. */
#ifndef TA_R120_CPU_COST_H
#define TA_R120_CPU_COST_H
#include <stdint.h>
#ifndef TA_R120_CPU_COST_SAMPLE
#define TA_R120_CPU_COST_SAMPLE 0
#endif
#if defined(XBOX_PORT) && TA_R120_CPU_COST_SAMPLE
#ifdef __cplusplus
extern "C" {
#endif
uint32_t ta_r120_sample_count(void);
uint64_t ta_r120_sample_cycles(void);
uint32_t ta_r120_top_pc(unsigned rank,uint32_t*count,uint64_t*cycles,uint8_t*opcode);
uint32_t ta_r120_top_opcode(unsigned rank,uint32_t*count,uint64_t*cycles);
void ta_r120_sample_reset(void);
#ifdef __cplusplus
}
#endif
#endif
#endif
