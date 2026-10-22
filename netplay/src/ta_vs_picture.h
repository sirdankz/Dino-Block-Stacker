#ifndef TA_VS_PICTURE_H
#define TA_VS_PICTURE_H
#include <stdint.h>
#include <stddef.h>
#define TA_VS_PIC_W 224u
#define TA_VS_PIC_H 224u
#define TA_VS_PIC_TILES 392u
/* Display-only RGB tiles. No guest RAM pointer or CPU state in this API.
   Refresh corrects dropped tiles without ever delaying local gameplay. */
typedef struct {uint32_t sent[TA_VS_PIC_W*TA_VS_PIC_H],stamp[TA_VS_PIC_TILES];uint8_t valid[TA_VS_PIC_TILES];unsigned cursor;uint8_t fieldOnly;
 uint8_t dirty[TA_VS_PIC_TILES];unsigned pendingCount;uint32_t preparedRevision,refreshTick;
} TaVsPictureTx;
typedef struct {uint32_t pixels[TA_VS_PIC_W*TA_VS_PIC_H],stamp[TA_VS_PIC_TILES];uint8_t valid[TA_VS_PIC_TILES];unsigned updates;} TaVsPictureRx;
size_t ta_vs_picture_packet(TaVsPictureTx*,const uint32_t*rgb,unsigned stride,uint32_t frame,uint8_t*,size_t);
int ta_vs_picture_prepare(TaVsPictureTx*,const uint32_t*rgb,unsigned stride,uint32_t revision);
size_t ta_vs_picture_next(TaVsPictureTx*,uint32_t tick,uint8_t*,size_t);
int ta_vs_picture_receive(TaVsPictureRx*,const uint8_t*,size_t);
#endif
