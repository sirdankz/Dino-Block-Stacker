/* R128 exact per-resource atlas contents, independent of guest state.
 * A valid bit means that eight source colors were successfully uploaded to
 * that physical swizzled tile. Planning is read-only; commit follows Unlock.
 * Omitted palettes retain their resource contents and validity until reused. */
#ifndef TA_R128_ATLAS_H
#define TA_R128_ATLAS_H
#include "ta_r127_indexed.h"
#include <string.h>
typedef struct {
 uint32_t colors[32][256];
 uint32_t valid[32];
} TaR128AtlasCache;
static unsigned ta_r128_plan(const TaR128AtlasCache*c,const TaR127Palette*p,
                             unsigned count,uint16_t dirty[1024]){
 unsigned n=0;
 if(!count||count>32)return 0;
 for(unsigned k=0;k<count+(count<32?1u:0u);k++){
  unsigned v=k<count?k:31;
  for(unsigned t=0;t<32;t++)
   if(!(c->valid[v]&(1u<<t))||memcmp(c->colors[v]+t*8,p[v].color+t*8,32))
    dirty[n++]=(uint16_t)(v*32+t);
 }
 return n;
}
static void ta_r128_commit(TaR128AtlasCache*c,const TaR127Palette*p,
                           const uint16_t*dirty,unsigned n){
 for(unsigned i=0;i<n;i++){
  unsigned v=dirty[i]/32,t=dirty[i]%32;
  memcpy(c->colors[v]+t*8,p[v].color+t*8,32);c->valid[v]|=1u<<t;
 }
}
#endif
