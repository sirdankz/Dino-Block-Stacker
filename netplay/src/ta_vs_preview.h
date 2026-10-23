#ifndef TA_VS_PREVIEW_H
#define TA_VS_PREVIEW_H
#include <stdint.h>
typedef struct{uint32_t pixels[13][32*32];unsigned ready;}TaVsPreview;
int ta_vs_preview_capture_one(TaVsPreview*,unsigned character,const uint32_t*active512x224);
int ta_vs_preview_capture(TaVsPreview*,const uint32_t*active512x224);
void ta_vs_preview_draw(const TaVsPreview*,unsigned character,uint32_t*screen512x480,unsigned x,unsigned y);
void ta_vs_preview_draw_size(const TaVsPreview*,unsigned character,uint32_t*screen512x480,unsigned x,unsigned y,unsigned size);
#endif
