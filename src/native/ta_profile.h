/* Low-impact, coarse 65C816 sampling for TA R4 diagnostic builds. */
#ifndef TA_PROFILE_H
#define TA_PROFILE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void ta_profile_sample(uint8_t bank, uint16_t pc);
/* Flags: bit0 M, bit1 X, bit2 E. Captured at first sampled visit. */
void ta_profile_sample_state(uint8_t bank, uint16_t pc, uint8_t db, uint16_t dp, uint8_t flags);
void ta_profile_interpreted(void);
uint32_t ta_profile_interpreted_count(void);
uint32_t ta_profile_exact_collisions(void);
uint32_t ta_profile_top_exact(unsigned rank, uint32_t *hits, uint8_t *db, uint16_t *dp, uint8_t *flags);
void ta_profile_reset(void);
uint32_t ta_profile_samples(void);
/* Returns address region bank:PC (256 bytes) and the number of samples. */
uint32_t ta_profile_top_region(unsigned rank, uint32_t *samples);
#ifdef __cplusplus
}
#endif
#endif
