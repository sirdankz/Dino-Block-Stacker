#ifndef TA_VS_BOARD_VIEW_H
#define TA_VS_BOARD_VIEW_H
#include <stdint.h>
#include <stddef.h>
/* Exact-ROM read-only discovery adapter. The four arrays are observed panel
 * storage planes, not a claimed complete board/save state or garbage format.
 * Use only with the verified Tetris Attack USA ROM. Never write a received
 * view into local guest WRAM or treat it as attack authority. */
#define TA_VS_BOARD_VIEW_BYTES 1040u
#define TA_VS_BOARD_WRAM_BYTES 131072u
typedef struct {
 uint32_t frame;
 unsigned sourcePlayer;
 uint16_t cursorFields[4];
 uint8_t plane[4][256];
} TaVsBoardView;
#ifdef __cplusplus
extern "C" {
#endif
int ta_vs_board_capture(const uint8_t*wram,size_t bytes,unsigned player,uint32_t frame,uint8_t*out,size_t capacity);
int ta_vs_board_decode(const uint8_t*data,size_t bytes,TaVsBoardView*out);
#ifdef __cplusplus
}
#endif
#endif
