/* Approximate 65C816 bank:256-byte execution-frequency histogram.
   Sampling is deliberately NOT clock/exclusive-time profiling and never
   changes CPU state, timing, memory, or host-reference behavior. */
#include "ta_profile.h"
#include <string.h>
static uint32_t s_sampleTicker;
static uint32_t s_samples;
static uint32_t s_hist[256u * 256u];
/* R19: 4,096 open-addressed slots for *sampled* exact PCs, not all 24-bit PCs.
   48 KiB, zero allocations, bounded scan on each 600-frame report. Collisions
   are counted; never treat this estimate as an exhaustive call graph. */
typedef struct { uint32_t pc24, hits; uint16_t dp; uint8_t db, flags; } TaExactSite;
static TaExactSite s_exact[4096];
static uint32_t s_collisions, s_fallback;

void ta_profile_reset(void) {
  s_sampleTicker = 0;
  s_samples = 0;
  memset(s_hist, 0, sizeof(s_hist));
  memset(s_exact, 0, sizeof(s_exact));
  s_collisions = s_fallback = 0;
}
void ta_profile_sample_state(uint8_t bank, uint16_t pc, uint8_t db, uint16_t dp, uint8_t flags) {
  if ((++s_sampleTicker & 1023u) != 0u) return;
  ++s_samples;
  ++s_hist[((uint32_t)bank << 8) | (pc >> 8)];
  const uint32_t pc24 = ((uint32_t)bank << 16) | pc;
  uint32_t slot = (pc24 * 2654435761u) >> 20;
  for (unsigned probe=0; probe<8; ++probe) {
    TaExactSite *e = &s_exact[(slot + probe) & 4095u];
    if (!e->hits || e->pc24 == pc24) {
      if (!e->hits) {e->pc24 = pc24; e->db = db; e->dp = dp; e->flags = flags;}
      ++e->hits;
      return;
    }
  }
  ++s_collisions;
}
void ta_profile_sample(uint8_t bank, uint16_t pc) {
  ta_profile_sample_state(bank, pc, 0, 0, 0);
}
void ta_profile_interpreted(void) { ++s_fallback; }
uint32_t ta_profile_interpreted_count(void) { return s_fallback; }
uint32_t ta_profile_exact_collisions(void) { return s_collisions; }
uint32_t ta_profile_top_exact(unsigned rank, uint32_t *hits, uint8_t *db, uint16_t *dp, uint8_t *flags) {
  uint32_t used[8], bestIdx = UINT32_MAX, bestHits = 0;
  if (rank >= 8) {if(hits)*hits=0;return UINT32_MAX;}
  for (unsigned n=0; n<=rank; ++n) {
    bestIdx=UINT32_MAX;bestHits=0;
    for (uint32_t i=0; i<4096u; ++i) {
      int skip=0;for(unsigned j=0;j<n;++j)if(used[j]==i)skip=1;
      if(!skip && s_exact[i].hits>bestHits){bestHits=s_exact[i].hits;bestIdx=i;}
    }
    used[n]=bestIdx;
  }
  if(hits)*hits=bestHits;
  if(bestIdx==UINT32_MAX)return UINT32_MAX;
  if(db)*db=s_exact[bestIdx].db;
  if(dp)*dp=s_exact[bestIdx].dp;
  if(flags)*flags=s_exact[bestIdx].flags;
  return s_exact[bestIdx].pc24;
}
uint32_t ta_profile_samples(void) { return s_samples; }
uint32_t ta_profile_top_region(unsigned rank, uint32_t *samples) {
  /* Stable ordering, no heap allocations, run once every 600 frames. */
  uint32_t used[4] = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
  uint32_t best_idx = UINT32_MAX, best_hits = 0;
  if (rank >= 4) { if(samples) *samples = 0; return UINT32_MAX; }
  for (unsigned n = 0; n <= rank; ++n) {
    best_idx = UINT32_MAX; best_hits = 0;
    for (uint32_t i = 0; i < 65536u; ++i) {
      int skip = 0;
      for (unsigned j = 0; j < n; ++j) if (used[j] == i) skip = 1;
      if (!skip && s_hist[i] > best_hits) {
        best_idx = i; best_hits = s_hist[i];
      }
    }
    used[n] = best_idx;
  }
  if(samples) *samples = best_hits;
  return best_idx == UINT32_MAX ? UINT32_MAX :
    (((best_idx >> 8) << 16) | ((best_idx & 255u) << 8));
}
