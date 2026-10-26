#include "ta_r82_cost.h"
#include "ta_diagnostics.h"
#include "ta_video_copy.h"
#include "ta_panel_colors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ppu.h"
#if defined(TA_R126_GPU_SUBTRACT) && TA_R126_GPU_SUBTRACT && !TA_R126_DIRECT_TARGET
#error R126 GPU output requires R126 direct-target ownership
#endif
#if defined(TA_R103_GUARD_AUDIT) && TA_R103_GUARD_AUDIT
/* R103 diagnostic-only counters, updated on the renderer's serial thread.
 * Counts vector-eligible BG chunks by active main/sub destinations and the
 * exact number of dual-writer helper calls; not the number of whole frames. */
static uint64_t s_r103Mode2BgCandidates[4];
static uint64_t s_r103Mode2BgDualCalls;
void ppu_taR103Mode2Counts(uint64_t counters[5]) {
 if(!counters)return;
 for(unsigned i=0;i<4u;++i)counters[i]=s_r103Mode2BgCandidates[i];
 counters[4]=s_r103Mode2BgDualCalls;
}
#endif
#include "snes.h"
#include "statehandler.h"

#if defined(TA_R12_OFFSET_CACHE) && TA_R12_OFFSET_CACHE
static uint32_t s_taMode2OffsetMask;
static uint16_t s_taMode2HOffset[32], s_taMode2VOffset[32];
static uint64_t s_taMode2OffsetHits, s_taMode2OffsetMisses;
#endif

// array for layer definitions per mode:
//   0-7: mode 0-7; 8: mode 1 + l3prio; 9: mode 7 + extbg

//   0-3; layers 1-4; 4: sprites; 5: nonexistent
static const int layersPerMode[10][12] = {
  {4, 0, 1, 4, 0, 1, 4, 2, 3, 4, 2, 3},
  {4, 0, 1, 4, 0, 1, 4, 2, 4, 2, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 4, 0, 4, 5, 5, 5, 5, 5, 5},
  {4, 4, 4, 0, 4, 5, 5, 5, 5, 5, 5, 5},
  {2, 4, 0, 1, 4, 0, 1, 4, 4, 2, 5, 5},
  {4, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5, 5}
};

static const int prioritysPerMode[10][12] = {
  {3, 1, 1, 2, 0, 0, 1, 1, 1, 0, 0, 0},
  {3, 1, 1, 2, 0, 0, 1, 1, 0, 0, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 0, 0, 5, 5, 5, 5, 5, 5},
  {3, 2, 1, 0, 0, 5, 5, 5, 5, 5, 5, 5},
  {1, 3, 1, 1, 2, 0, 0, 1, 0, 0, 5, 5},
  {3, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5, 5}
};

static const int layerCountPerMode[10] = {
  12, 10, 8, 8, 8, 8, 6, 5, 10, 7
};

static const int bitDepthsPerMode[10][4] = {
  {2, 2, 2, 2},
  {4, 4, 2, 5},
  {4, 4, 5, 5},
  {8, 4, 5, 5},
  {8, 2, 5, 5},
  {4, 2, 5, 5},
  {4, 5, 5, 5},
  {8, 5, 5, 5},
  {4, 4, 2, 5},
  {8, 7, 5, 5}
};

static const int spriteSizes[8][2] = {
  {8, 16}, {8, 32}, {8, 64}, {16, 32},
  {16, 64}, {32, 64}, {16, 32}, {16, 32}
};

// Hot PPU lookup tables adapted from Dinkc64's LakeSnes renderer.
// The brightness table is bit-exact with the old
//   expanded_5bit * brightness / 15
// expression, but removes six integer divisions from every game pixel.
static const uint8_t s_brightnessLut[16][32] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 17},
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34},
  {0, 1, 3, 4, 6, 8, 9, 11, 13, 14, 16, 18, 19, 21, 23, 24, 26, 28, 29, 31, 33, 34, 36, 37, 39, 41, 42, 44, 46, 47, 49, 51},
  {0, 2, 4, 6, 8, 10, 13, 15, 17, 19, 21, 24, 26, 28, 30, 32, 35, 37, 39, 41, 44, 46, 48, 50, 52, 54, 57, 59, 61, 63, 65, 68},
  {0, 2, 5, 8, 11, 13, 16, 19, 22, 24, 27, 30, 33, 35, 38, 41, 44, 46, 49, 52, 55, 57, 60, 63, 66, 68, 71, 74, 77, 79, 82, 85},
  {0, 3, 6, 9, 13, 16, 19, 22, 26, 29, 32, 36, 39, 42, 46, 49, 52, 56, 59, 62, 66, 69, 72, 75, 79, 82, 85, 88, 92, 95, 98, 102},
  {0, 3, 7, 11, 15, 19, 22, 26, 30, 34, 38, 42, 46, 49, 53, 57, 61, 65, 69, 72, 77, 80, 84, 88, 92, 96, 99, 103, 107, 111, 115, 119},
  {0, 4, 8, 12, 17, 21, 26, 30, 35, 39, 43, 48, 52, 57, 61, 65, 70, 74, 78, 83, 88, 92, 96, 100, 105, 109, 114, 118, 123, 127, 131, 136},
  {0, 4, 9, 14, 19, 24, 29, 34, 39, 44, 49, 54, 59, 64, 69, 73, 79, 84, 88, 93, 99, 103, 108, 113, 118, 123, 128, 133, 138, 143, 148, 153},
  {0, 5, 10, 16, 22, 27, 32, 38, 44, 49, 54, 60, 66, 71, 76, 82, 88, 93, 98, 104, 110, 115, 120, 126, 132, 137, 142, 148, 154, 159, 164, 170},
  {0, 5, 11, 17, 24, 30, 35, 41, 48, 54, 60, 66, 72, 78, 84, 90, 96, 102, 108, 114, 121, 126, 132, 138, 145, 151, 156, 162, 169, 175, 181, 187},
  {0, 6, 12, 19, 26, 32, 39, 45, 52, 59, 65, 72, 79, 85, 92, 98, 105, 112, 118, 124, 132, 138, 144, 151, 158, 164, 171, 177, 184, 191, 197, 204},
  {0, 6, 13, 20, 28, 35, 42, 49, 57, 64, 71, 78, 85, 92, 99, 106, 114, 121, 128, 135, 143, 149, 156, 163, 171, 178, 185, 192, 200, 207, 214, 221},
  {0, 7, 14, 22, 30, 38, 45, 53, 61, 69, 76, 84, 92, 99, 107, 114, 123, 130, 138, 145, 154, 161, 168, 176, 184, 192, 199, 207, 215, 223, 230, 238},
  {0, 8, 16, 24, 33, 41, 49, 57, 66, 74, 82, 90, 99, 107, 115, 123, 132, 140, 148, 156, 165, 173, 181, 189, 198, 206, 214, 222, 231, 239, 247, 255}
};

// Indexed as s_colorClampLut[value + 32] for value in [-32, 63].
static const uint8_t s_colorClampLut[96] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
  31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31
};

static inline int ppu_clampColorFast(int v) {
  return s_colorClampLut[v + 32];
}

static void ppu_handlePixel(Ppu* ppu, int x, int y);
static int ppu_getPixel(Ppu* ppu, int x, int y, bool sub, int* r, int* g, int* b);
static uint16_t ppu_getOffsetValue(Ppu* ppu, int col, int row);
static int ppu_getPixelForBgLayer(Ppu* ppu, int x, int y, int layer, bool priority);
static inline int ppu_getPixelForBgLayerCached(Ppu* ppu, int x, int y, int layer, bool priority);
static void ppu_handleOPT(Ppu* ppu, int layer, int* lx, int* ly);
static void ppu_calculateMode7Starts(Ppu* ppu, int y);
static int ppu_getPixelForMode7(Ppu* ppu, int x, int layer, bool priority);
static bool ppu_getWindowState(Ppu* ppu, int layer, int x);
static bool ppu_getWindowStateOnLine(Ppu* ppu, int layer, int x, int line);
static inline bool ppu_getWindowStateFast(Ppu* ppu, int layer, int x) {
  if(ppu->fastWindowCacheX[layer] != x) {
    ppu->fastWindowCacheX[layer] = x;
    ppu->fastWindowCacheValue[layer] = ppu_getWindowState(ppu, layer, x);
  }
  return ppu->fastWindowCacheValue[layer];
}
/* TA R17: line-local SNES window bitsets for the native Mode 1/2 compositor.
   Building two 256-bit intervals costs 16 words per scanline; repeated window
   queries during BG/object/color-math composition become indexed bit tests.
   Do NOT use this for extended/widescreen output, offscreen x, or generic
   PPU rendering. The generic window test is still the correctness fallback. */
#if defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
static uint32_t s_taWindowMask[6][8];
static bool s_taWindowMaskReady;
static uint32_t s_taWindowBitsetLines;
uint32_t ppu_taWindowBitsetLines(void) { return s_taWindowBitsetLines; }
static inline uint32_t ppu_taIntervalBits(unsigned word, unsigned left, unsigned right) {
  if(left > right || word < (left >> 5) || word > (right >> 5)) return 0;
  uint32_t low = word == (left >> 5) ? (UINT32_MAX << (left & 31)) : UINT32_MAX;
  uint32_t high = word == (right >> 5) ? (UINT32_MAX >> (31 - (right & 31))) : UINT32_MAX;
  return low & high;
}
/* R97: exact current-line window configuration, including all six layers' full
 * mask logic bytes. No hashes/collisions and no cross-line assumption about
 * HDMA or writes. Only reuses derived masks when every input is identical. */
#if defined(TA_R97_WINDOW_CONFIG_CACHE) && TA_R97_WINDOW_CONFIG_CACHE
struct TaR97WindowKey {
  uint8_t edges[4];
  uint16_t layer[6];
};
static struct TaR97WindowKey s_taR97LastWindowKey;
static bool s_taR97WindowKeyValid;
#endif
static void ppu_taPrepareWindowMasks(Ppu* ppu) {
  s_taWindowMaskReady = ppu->extraLeft == 0 && ppu->extraRight == 0;
  if(!s_taWindowMaskReady) return;
#if defined(TA_R97_WINDOW_CONFIG_CACHE) && TA_R97_WINDOW_CONFIG_CACHE
  struct TaR97WindowKey key;
  key.edges[0]=ppu->window1left;
  key.edges[1]=ppu->window1right;
  key.edges[2]=ppu->window2left;
  key.edges[3]=ppu->window2right;
  for(unsigned i=0;i<6;++i) {
    const WindowLayer* w=&ppu->windowLayer[i];
    key.layer[i]=(uint16_t)(!!w->window1enabled |
        ((unsigned)!!w->window2enabled<<1) |
        ((unsigned)!!w->window1inversed<<2) |
        ((unsigned)!!w->window2inversed<<3) |
        ((unsigned)w->maskLogic<<4));
  }
  if(__builtin_expect(s_taR97WindowKeyValid &&
                      memcmp(&key,&s_taR97LastWindowKey,sizeof key)==0,1)) {
    TA_DIAG(++s_taWindowBitsetLines);
    return;
  }
  s_taR97LastWindowKey=key;
  s_taR97WindowKeyValid=true;
  /* Build the two interval words just once per word, not once per BG/OBJ
   * layer. The same interval bits are reused across all six layers. */
  uint32_t w1Words[8],w2Words[8];
  for(unsigned i=0;i<8;++i) {
    w1Words[i]=ppu_taIntervalBits(i,ppu->window1left,ppu->window1right);
    w2Words[i]=ppu_taIntervalBits(i,ppu->window2left,ppu->window2right);
  }
  /* Hoist the layer invariants above the eight 32-pixel mask words. */
  for(unsigned layer=0;layer<6;++layer) {
    const WindowLayer* w=&ppu->windowLayer[layer];
    const bool en1=w->window1enabled, en2=w->window2enabled;
    const bool inv1=w->window1inversed, inv2=w->window2inversed;
    const unsigned logic=w->maskLogic;
    if(!en1 && !en2) {
      memset(s_taWindowMask[layer],0,sizeof s_taWindowMask[layer]);
      continue;
    }
    for(unsigned i=0;i<8;++i) {
      const uint32_t v1=inv1?~w1Words[i]:w1Words[i];
      const uint32_t v2=inv2?~w2Words[i]:w2Words[i];
      uint32_t m=0;
      if(en1 && !en2) m=v1;
      else if(en2 && !en1) m=v2;
      else if(en1 && en2) {
        switch(logic) {
          case 0: m=v1|v2; break;
          case 1: m=v1&v2; break;
          case 2: m=v1^v2; break;
          case 3: m=~(v1^v2); break;
        }
      }
      s_taWindowMask[layer][i]=m;
    }
  }
#else
  for(unsigned i=0;i<8;++i) {
    uint32_t w1=ppu_taIntervalBits(i, ppu->window1left, ppu->window1right);
    uint32_t w2=ppu_taIntervalBits(i, ppu->window2left, ppu->window2right);
    for(unsigned layer=0;layer<6;++layer) {
      const bool en1=ppu->windowLayer[layer].window1enabled;
      const bool en2=ppu->windowLayer[layer].window2enabled;
      const uint32_t v1=ppu->windowLayer[layer].window1inversed ? ~w1 : w1;
      const uint32_t v2=ppu->windowLayer[layer].window2inversed ? ~w2 : w2;
      uint32_t m=0;
      if(en1 && !en2) m=v1;
      else if(en2 && !en1) m=v2;
      else if(en1 && en2) {
        switch(ppu->windowLayer[layer].maskLogic) {
          case 0: m=v1|v2; break;
          case 1: m=v1&v2; break;
          case 2: m=v1^v2; break;
          case 3: m=~(v1^v2); break;
        }
      }
      s_taWindowMask[layer][i]=m;
    }
  }
#endif
  TA_DIAG(++s_taWindowBitsetLines);
}
#if defined(TA_R18_WINDOW_CHUNK) && TA_R18_WINDOW_CHUNK
static uint32_t s_taR18UniformTiles, s_taR18CulledTiles, s_taR18MixedTiles;
void ppu_taR18WindowStats(uint32_t* fast, uint32_t* culled, uint32_t* mixed) {
  if(fast) *fast=s_taR18UniformTiles;
  if(culled) *culled=s_taR18CulledTiles;
  if(mixed) *mixed=s_taR18MixedTiles;
}
#endif
static inline bool ppu_taNativeWindowState(Ppu* ppu, int layer, int x) {
  if(__builtin_expect(s_taWindowMaskReady && (unsigned)x < 256u, 1))
    return (s_taWindowMask[layer][(unsigned)x >> 5] >> (x & 31)) & 1u;
  return ppu_getWindowStateFast(ppu, layer, x);
}
#else
#define ppu_taNativeWindowState ppu_getWindowStateFast
#endif

static void ppu_evaluateSprites(Ppu* ppu, int line);
#if defined(XBOX_PORT)
static void ppu_evaluateSpritesXboxNarrow(Ppu* ppu, int line);
#endif
static uint16_t ppu_getVramRemap(Ppu* ppu);
static bool ppu_wideMapX(Ppu* ppu, int layer, int* x, int* y);
static bool ppu_windowTest(const Ppu* ppu, int x, int left, int right);
static int ppu_spriteX(const Ppu* ppu, uint8_t index);
#if defined(XBOX_PORT)
static inline void ppu_xboxInvalidateTileWord(uint16_t wordAdr);
static inline void ppu_xboxInvalidateTileCache(void);
static inline void ppu_xboxInvalidatePaletteCache(void);
void ppu_xboxInvalidateSpriteCache(void);
#endif

#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
void ppu_r126ResolveAll(Ppu* ppu);
static void ppu_r126Forget(Ppu* ppu);
#endif
Ppu* ppu_init(Snes* snes) {
  Ppu* ppu = malloc(sizeof(Ppu));
  ppu->snes = snes;
  ppu->nativeLineRenderer = NULL;
  ppu->nativeLineRendererCtx = NULL;
  ppu->noPixels = false;
  ppu_setPixelOutputFormat(ppu, ppu_pixelOutputFormatBGRX);
  // Set here and not in `ppu_reset`, because the width of the picture belongs
  // to whoever is displaying it and a game resetting itself is not a reason to
  // take it back to 256.
  ppu->extraLeft = 0;
  ppu->extraRight = 0;
  for(int i = 0; i < 5; i++) ppu->layerWide[i] = ppu_wideAuto;
  memset(ppu->spritePlace, 0, sizeof(ppu->spritePlace));
  memset(ppu->spriteShift, 0, sizeof(ppu->spriteShift));
  for(int i = 0; i < 4; i++) for(int j = 0; j < 2; j++) {
    ppu->centreFillCol[i][j] = -1; ppu->centreFillLine[i][j] = -1; ppu->centreFillFrom[i][j] = -1;
    ppu->centreFillWrap[i][j] = 0; ppu->centreLastFull[i] = -2;
    ppu->centreLastFullH[i] = ppu->centreLastFullV[i] = 0;
    ppu->centreFillH[i][j] = ppu->centreFillV[i][j] = 0;
  }
  for(int i = 0; i < 4; i++) {
    ppu->layerEdgeEmpty[i] = 0;
    ppu->layerScrolled[i] = 0;
    ppu->lastHScroll[i] = 0;
    ppu->layerRaster[i] = 0;
    ppu->hScrollWrites[i] = 0;
  }
  ppu->wideClampLo = -PPU_EXTRA_MAX;
  ppu->wideClampHi = 255 + PPU_EXTRA_MAX;
  memset(ppu->lineHScroll, 0, sizeof(ppu->lineHScroll));
  memset(ppu->lineVScroll, 0, sizeof(ppu->lineVScroll));
  ppu->midFrameWrite = false;
  ppu->midFrameWrites = 0;
  ppu->windowRaster = false;
  return ppu;
}

void ppu_free(Ppu* ppu) {
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  ppu_r126Forget(ppu);
#endif
  free(ppu);
}

void ppu_reset(Ppu* ppu) {
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  ppu_r126ResolveAll(ppu);
#endif
  memset(ppu->vram, 0, sizeof(ppu->vram));
#if defined(XBOX_PORT)
  ppu_xboxInvalidateTileCache();
  ppu_xboxInvalidatePaletteCache();
#endif
  ppu->vramPointer = 0;
  ppu->vramIncrementOnHigh = false;
  ppu->vramIncrement = 1;
  ppu->vramRemapMode = 0;
  ppu->vramReadBuffer = 0;
  memset(ppu->cgram, 0, sizeof(ppu->cgram));
  ppu->cgramPointer = 0;
  ppu->cgramSecondWrite = false;
  ppu->cgramBuffer = 0;
  memset(ppu->oam, 0, sizeof(ppu->oam));
  memset(ppu->highOam, 0, sizeof(ppu->highOam));
  ppu->oamAdr = 0;
  ppu->oamAdrWritten = 0;
  ppu->oamInHigh = false;
  ppu->oamInHighWritten = false;
  ppu->oamSecondWrite = false;
  ppu->oamBuffer = 0;
  ppu->objPriority = false;
  ppu->objTileAdr1 = 0;
  ppu->objTileAdr2 = 0;
  ppu->objSize = 0;
  memset(ppu->objPixelBuffer, 0, sizeof(ppu->objPixelBuffer));
  memset(ppu->objPriorityBuffer, 0, sizeof(ppu->objPriorityBuffer));
  memset(ppu->objRemapOn, 0, sizeof(ppu->objRemapOn));
  memset(ppu->objRemap, 0, sizeof(ppu->objRemap));
  memset(ppu->objFront, 0, sizeof(ppu->objFront));
#if defined(XBOX_PORT)
  ppu_xboxInvalidateSpriteCache();
#endif
  ppu->timeOver = false;
  ppu->rangeOver = false;
  ppu->objInterlace = false;
  for(int i = 0; i < 4; i++) {
    ppu->bgLayer[i].hScroll = 0;
    ppu->bgLayer[i].vScroll = 0;
    ppu->bgLayer[i].tilemapWider = false;
    ppu->bgLayer[i].tilemapHigher = false;
    ppu->bgLayer[i].tilemapAdr = 0;
    ppu->bgLayer[i].tileAdr = 0;
    ppu->bgLayer[i].bigTiles = false;
    ppu->bgLayer[i].mosaicEnabled = false;
  }
  ppu->scrollPrev = 0;
  ppu->scrollPrev2 = 0;
  ppu->mosaicSize = 1;
  ppu->mosaicStartLine = 1;
  for(int i = 0; i < 5; i++) {
    ppu->layer[i].mainScreenEnabled = false;
    ppu->layer[i].subScreenEnabled = false;
    ppu->layer[i].mainScreenWindowed = false;
    ppu->layer[i].subScreenWindowed = false;
  }
  memset(ppu->m7matrix, 0, sizeof(ppu->m7matrix));
  ppu->m7prev = 0;
  ppu->m7largeField = false;
  ppu->m7charFill = false;
  ppu->m7xFlip = false;
  ppu->m7yFlip = false;
  ppu->m7extBg = false;
  ppu->m7startX = 0;
  ppu->m7startY = 0;
  for(int i = 0; i < 6; i++) {
    ppu->windowLayer[i].window1enabled = false;
    ppu->windowLayer[i].window2enabled = false;
    ppu->windowLayer[i].window1inversed = false;
    ppu->windowLayer[i].window2inversed = false;
    ppu->windowLayer[i].maskLogic = 0;
  }
  ppu->window1left = 0;
  ppu->window1right = 0;
  ppu->window2left = 0;
  ppu->window2right = 0;
  ppu->clipMode = 0;
  ppu->preventMathMode = 0;
  ppu->addSubscreen = false;
  ppu->subtractColor = false;
  ppu->halfColor = false;
  memset(ppu->mathEnabled, 0, sizeof(ppu->mathEnabled));
  ppu->fixedColorR = 0;
  ppu->fixedColorG = 0;
  ppu->fixedColorB = 0;
  ppu->forcedBlank = true;
  ppu->brightness = 0;
  ppu->mode = 0;
  ppu->bg3priority = false;
  ppu->evenFrame = false;
  ppu->pseudoHires = false;
  ppu->overscan = false;
  ppu->frameOverscan = false;
  ppu->interlace = false;
  ppu->frameInterlace = false;
  ppu->directColor = false;
  ppu->hCount = 0;
  ppu->vCount = 0;
  ppu->hCountSecond = false;
  ppu->vCountSecond = false;
  ppu->countersLatched = false;
  ppu->ppu1openBus = 0;
  ppu->ppu2openBus = 0;
  memset(ppu->pixelBuffer, 0, sizeof(ppu->pixelBuffer));
#if defined(XBOX_PORT)
  memset(ppu->taNative256Row, 0, sizeof(ppu->taNative256Row));
#endif
}

void ppu_handleState(Ppu* ppu, StateHandler* sh) {
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  ppu_r126ResolveAll(ppu);
#endif
  sh_handleBools(sh,
    &ppu->vramIncrementOnHigh, &ppu->cgramSecondWrite, &ppu->oamInHigh, &ppu->oamInHighWritten, &ppu->oamSecondWrite,
    &ppu->objPriority, &ppu->timeOver, &ppu->rangeOver, &ppu->objInterlace, &ppu->m7largeField, &ppu->m7charFill,
    &ppu->m7xFlip, &ppu->m7yFlip, &ppu->m7extBg, &ppu->addSubscreen, &ppu->subtractColor, &ppu->halfColor,
    &ppu->mathEnabled[0], &ppu->mathEnabled[1], &ppu->mathEnabled[2], &ppu->mathEnabled[3], &ppu->mathEnabled[4],
    &ppu->mathEnabled[5], &ppu->forcedBlank, &ppu->bg3priority, &ppu->evenFrame, &ppu->pseudoHires, &ppu->overscan,
    &ppu->frameOverscan, &ppu->interlace, &ppu->frameInterlace, &ppu->directColor, &ppu->hCountSecond, &ppu->vCountSecond,
    &ppu->countersLatched, NULL
  );
  sh_handleBytes(sh,
    &ppu->vramRemapMode, &ppu->cgramPointer, &ppu->cgramBuffer, &ppu->oamAdr, &ppu->oamAdrWritten, &ppu->oamBuffer,
    &ppu->objSize, &ppu->scrollPrev, &ppu->scrollPrev2, &ppu->mosaicSize, &ppu->mosaicStartLine, &ppu->m7prev,
    &ppu->window1left, &ppu->window1right, &ppu->window2left, &ppu->window2right, &ppu->clipMode, &ppu->preventMathMode,
    &ppu->fixedColorR, &ppu->fixedColorG, &ppu->fixedColorB, &ppu->brightness, &ppu->mode,
    &ppu->ppu1openBus, &ppu->ppu2openBus, NULL
  );
  sh_handleWords(sh,
    &ppu->vramPointer, &ppu->vramIncrement, &ppu->vramReadBuffer, &ppu->objTileAdr1, &ppu->objTileAdr2,
    &ppu->hCount, &ppu->vCount, NULL
  );
  sh_handleWordsS(sh,
    &ppu->m7matrix[0], &ppu->m7matrix[1], &ppu->m7matrix[2], &ppu->m7matrix[3], &ppu->m7matrix[4], &ppu->m7matrix[5],
    &ppu->m7matrix[6], &ppu->m7matrix[7], NULL
  );
  sh_handleIntsS(sh, &ppu->m7startX, &ppu->m7startY, NULL);
  for(int i = 0; i < 4; i++) {
    sh_handleBools(sh,
      &ppu->bgLayer[i].tilemapWider, &ppu->bgLayer[i].tilemapHigher, &ppu->bgLayer[i].bigTiles,
      &ppu->bgLayer[i].mosaicEnabled, NULL
    );
    sh_handleWords(sh,
      &ppu->bgLayer[i].hScroll, &ppu->bgLayer[i].vScroll, &ppu->bgLayer[i].tilemapAdr, &ppu->bgLayer[i].tileAdr, NULL
    );
  }
  for(int i = 0; i < 5; i++) {
    sh_handleBools(sh,
      &ppu->layer[i].mainScreenEnabled, &ppu->layer[i].subScreenEnabled, &ppu->layer[i].mainScreenWindowed,
      &ppu->layer[i].subScreenWindowed, NULL
    );
  }
  for(int i = 0; i < 6; i++) {
    sh_handleBools(sh,
      &ppu->windowLayer[i].window1enabled, &ppu->windowLayer[i].window1inversed, &ppu->windowLayer[i].window2enabled,
      &ppu->windowLayer[i].window2inversed, NULL
    );
    sh_handleBytes(sh, &ppu->windowLayer[i].maskLogic, NULL);
  }
  sh_handleWordArray(sh, ppu->vram, 0x8000);
  sh_handleWordArray(sh, ppu->cgram, 0x100);
  sh_handleWordArray(sh, ppu->oam, 0x100);
  sh_handleByteArray(sh, ppu->highOam, 0x20);
  sh_handleByteArray(sh, ppu->objPixelBuffer, 256);
  sh_handleByteArray(sh, ppu->objPriorityBuffer, 256);
#if defined(XBOX_PORT)
  if(!sh->saving) {
    ppu_xboxInvalidateTileCache();
    ppu_xboxInvalidatePaletteCache();
    ppu_xboxInvalidateSpriteCache();
  }
#endif
}

bool ppu_checkOverscan(Ppu* ppu) {
  // called at (0,225)
  ppu->frameOverscan = ppu->overscan; // set if we have a overscan-frame
  return ppu->frameOverscan;
}

void ppu_handleVblank(Ppu* ppu) {
  // called either right after ppu_checkOverscan at (0,225), or at (0,240)
  if(!ppu->forcedBlank) {
    ppu->oamAdr = ppu->oamAdrWritten;
    ppu->oamInHigh = ppu->oamInHighWritten;
    ppu->oamSecondWrite = false;
#if defined(XBOX_PORT)
    ppu_xboxInvalidateSpriteCache();
#endif
  }
  ppu->frameInterlace = ppu->interlace; // set if we have a interlaced frame
}

// The tilemap word covering layer coordinates (x, y) -- the same address
// `ppu_getPixelForBgLayer` works out, without the pixel.
static uint16_t ppu_tilemapWord(const Ppu* ppu, int layer, int x, int y) {
  bool wideTiles = ppu->bgLayer[layer].bigTiles || ppu->mode == 5 || ppu->mode == 6;
  int tileBitsX = wideTiles ? 4 : 3;
  int tileHighBitX = wideTiles ? 0x200 : 0x100;
  int tileBitsY = ppu->bgLayer[layer].bigTiles ? 4 : 3;
  int tileHighBitY = ppu->bgLayer[layer].bigTiles ? 0x200 : 0x100;
  x &= 0x3ff;
  y &= 0x3ff;
  uint16_t adr = ppu->bgLayer[layer].tilemapAdr + (((y >> tileBitsY) & 0x1f) << 5 | ((x >> tileBitsX) & 0x1f));
  if((x & tileHighBitX) && ppu->bgLayer[layer].tilemapWider) adr += 0x400;
  if((y & tileHighBitY) && ppu->bgLayer[layer].tilemapHigher) adr += ppu->bgLayer[layer].tilemapWider ? 0x800 : 0x400;
  return ppu->vram[adr & 0x7fff];
}

// Is this character all zeroes -- eight rows of nothing, whatever palette the
// tilemap word that names it asks for? Colour zero is transparent on a
// background layer, so a character of nothing but zeroes draws nothing.
static bool ppu_charEmpty(const Ppu* ppu, int layer, int tileNum, int bitDepth) {
  const int words = 4 * bitDepth;
  const uint16_t base = (uint16_t)(ppu->bgLayer[layer].tileAdr + (tileNum & 0x3ff) * words);
  for(int i = 0; i < words; i++) {
    if(ppu->vram[(base + i) & 0x7fff] != 0) return false;
  }
  return true;
}

// Does this layer draw anything at all in the tilemap column under screen
// pixel `sx`?
//
// This is the question that decides what a layer does with the margins, and it
// is deliberately about *pixels* rather than about tilemap words. Two different
// blank tiles -- the same character in two palettes -- are two different words
// and the same nothing, and the game uses both: the LucasArts logo layer pads
// its edges with palette 2's blank tile and the legal screen pads its edges
// with palette 7's. A test that compared words would call those layers textured
// and repeat them into the margins, which is how a screenful of legal text came
// to be printed three times.
static bool ppu_columnEmpty(const Ppu* ppu, int layer, int sx) {
  int actMode = ppu->mode == 1 && ppu->bg3priority ? 8 : ppu->mode;
  const int bitDepth = bitDepthsPerMode[actMode][layer];
  // 5 and 7 are the table's markers for a layer this mode does not have.
  if(bitDepth != 2 && bitDepth != 4 && bitDepth != 8) return false;
  const bool big = ppu->bgLayer[layer].bigTiles;
  const int step = big ? 16 : 8;
  const int x = sx + ppu->bgLayer[layer].hScroll;
  const int top = ppu->bgLayer[layer].vScroll;
  // Every tile row the 224-line display touches, plus one for the row the
  // scroll leaves half on screen.
  for(int y = top; y < top + 224 + step; y += step) {
    const int n = ppu_tilemapWord(ppu, layer, x, y) & 0x3ff;
    if(!ppu_charEmpty(ppu, layer, n, bitDepth)) return false;
    // A 16x16 tilemap entry is four characters: the named one, the one after
    // it, and the two a row of sixteen below.
    if(big && (!ppu_charEmpty(ppu, layer, n + 1, bitDepth) ||
               !ppu_charEmpty(ppu, layer, n + 0x10, bitDepth) ||
               !ppu_charEmpty(ppu, layer, n + 0x11, bitDepth))) return false;
  }
  return true;
}

void ppu_handleFrameStart(Ppu* ppu) {
  // called at (0, 0)
  ppu->mosaicStartLine = 1;
  ppu->rangeOver = false;
  ppu->timeOver = false;
  ppu->evenFrame = !ppu->evenFrame;
  ppu->midFrameWrite = false;
  ppu->midFrameWrites = 0;
  ppu->windowRaster = false;
  // Once a frame, and only when there are margins to fill: which of the
  // backgrounds have nothing at the console's edges, and so nothing to say
  // beyond them. Done here because the answer has to hold for the whole frame
  // -- a layer that changed its mind halfway down would tear -- and because by
  // line 0 this frame's tilemap uploads are in, having gone up during the
  // vblank just past.
  // Which layers the game is scrolling. Latched for as long as the screen
  // stands, and cleared when it is taken down -- every screen in this game is
  // built behind a forced blank or a faded-out one -- so the answer is "has
  // this layer moved on this screen" rather than "did it move since last
  // frame". Tracked whether or not there are margins, because the answer has to
  // be right on the first frame after widescreen is switched on.
  for(int i = 0; i < 4; i++) {
    if(ppu->forcedBlank || ppu->brightness == 0) {
      ppu->layerScrolled[i] = 0;
    } else if(ppu->bgLayer[i].hScroll != ppu->lastHScroll[i]) {
      // Horizontally, and only horizontally: it is the vertical seam of the
      // tilemap that the margins would repeat, and a card whose text slides up
      // the screen has never crossed it. The level-name card is exactly that --
      // `hScroll` nailed to 0 while `vScroll` runs -- and reading its motion as
      // permission to wrap printed the end of its last line down both sides.
      ppu->layerScrolled[i] = 1;
    }
    ppu->lastHScroll[i] = ppu->bgLayer[i].hScroll;
    if(ppu->forcedBlank || ppu->brightness == 0) ppu->layerRaster[i] = 0;
    else if(ppu->hScrollWrites[i] >= PPU_RASTER_WRITES) ppu->layerRaster[i] = 1;
    ppu->hScrollWrites[i] = 0;
  }
  if(ppu->extraLeft != 0 || ppu->extraRight != 0) {
    for(int i = 0; i < 4; i++) {
      ppu->layerEdgeEmpty[i] = (ppu->mode != 7 &&
                                ppu->layerWide[i] == ppu_wideAuto &&
                                ppu_columnEmpty(ppu, i, 0) &&
                                ppu_columnEmpty(ppu, i, 255));
    }
  }
}


#if defined(XBOX_PORT)
static uint64_t s_xboxMode1FastLines = 0;
static uint64_t s_taMode2FastLines = 0;
static uint64_t s_taMode2RejectedLines = 0;
static uint64_t s_xboxGenericLines = 0;
static uint64_t s_xboxPackedTiles = 0;
static uint64_t s_xboxScalarTileChunks = 0;
static uint64_t s_xboxMmxChunks = 0;
static uint64_t s_xboxFixedMathLines = 0;
static uint64_t s_xboxSubMathLines = 0;
static uint64_t s_xboxNarrowSpriteLines = 0;
static uint64_t s_xboxTransparentBgRows = 0;
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
static uint64_t s_xboxSpriteCacheBuilds = 0;
static uint64_t s_xboxSpriteCachedLines = 0;
static uint64_t s_xboxSpriteMmxSlivers = 0;
static uint64_t s_xboxSpriteScalarSlivers = 0;
#endif

#if defined(ZAMN_R32_PPU_HOISTS)
/*
 * R32: hot Mode-1 invariants.
 *
 * z priorities and layer ids are tiny byte values, but the R23 MMX compositor
 * used four instructions on every 8-pixel chunk to broadcast each byte across
 * an MMX register.  A 16-entry L1-resident table replaces those unpack chains
 * with one movq.  Values above 15 are never used by this renderer.
 */
static const uint64_t s_xboxByteBroadcast[16] __attribute__((aligned(32))) = {
  0x0000000000000000ULL, 0x0101010101010101ULL,
  0x0202020202020202ULL, 0x0303030303030303ULL,
  0x0404040404040404ULL, 0x0505050505050505ULL,
  0x0606060606060606ULL, 0x0707070707070707ULL,
  0x0808080808080808ULL, 0x0909090909090909ULL,
  0x0a0a0a0a0a0a0a0aULL, 0x0b0b0b0b0b0b0b0bULL,
  0x0c0c0c0c0c0c0c0cULL, 0x0d0d0d0d0d0d0d0dULL,
  0x0e0e0e0e0e0e0e0eULL, 0x0f0f0f0f0f0f0f0fULL
};
#endif

/*
 * R19: Snes9x-style decoded VRAM tile cache.
 *
 * Mode 1 reuses the same 2bpp/4bpp character data for many scanlines and
 * frames.  Decoding SNES planar pixels again for every sliver is pure host
 * work and has no hardware-visible side effect, so cache the decoded 8x8
 * pixels and invalidate only the tile touched by a VRAM write.
 *
 * VRAM is 32K 16-bit words.  A 4bpp tile occupies 16 words (2048 possible
 * aligned tiles); a 2bpp tile occupies 8 words (4096 possible aligned tiles).
 * The cache is intentionally small/simple and 32-byte aligned for the Xbox
 * Pentium III cache line.
 */
static uint8_t s_xboxTile4[2048][64] __attribute__((aligned(32)));
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
/* R35: pre-reversed 4bpp tiles for horizontally flipped objects. The old
   sprite path reversed every 8-pixel sliver one byte at a time. Filling both
   orientations on the rare decode miss turns either orientation into the same
   MMX object-compositor input. */
static uint8_t s_xboxTile4Flip[2048][64] __attribute__((aligned(32)));
#endif
static uint8_t s_xboxTile2[4096][64] __attribute__((aligned(32)));
static uint8_t s_xboxTile4Valid[2048] __attribute__((aligned(32)));
static uint8_t s_xboxTile2Valid[4096] __attribute__((aligned(32)));
static uint32_t s_xboxTileCacheHits = 0;
static uint32_t s_xboxTileCacheMisses = 0;

/*
 * R22 resolved tile-row cache.
 *
 * R19 stopped re-decoding SNES bitplanes, but the Mode-1 renderer still
 * repeated flip/palette/tile-row setup for ~20K background chunks per frame.
 * Cache the final eight CGRAM indices for a tile row. The entry is exactly
 * 16 bytes so the 2048-entry table is 32 KiB, small enough to coexist with the
 * decoded-tile cache without consuming another huge slice of Xbox memory.
 *
 * Any VRAM write bumps the epoch. That is intentionally conservative: tilemap
 * and pattern changes can never reuse a stale resolved row, while ordinary
 * gameplay (where VRAM is stable for long stretches) gets near-free hits.
 */
#define XBOX_ROW_CACHE_BITS 11
#define XBOX_ROW_CACHE_SIZE (1u << XBOX_ROW_CACHE_BITS)
typedef struct {
  uint32_t key0;      /* tile word | resolved pattern base << 16 */
  uint16_t key1;      /* row | layer << 3 */
  uint16_t epoch;
  uint8_t pixels[8];  /* palette-adjusted, h-flip-resolved CGRAM indices */
} XboxResolvedRow;
/* R65: opt-in two-way cache with the SAME original 2048 hash indices.
 * Existing one-way layout and instructions are preserved when ways != 2.
 * On two-way builds there are 4096 entries (64 KiB) and a 2 KiB LRU bit
 * table. Exact key/epoch validity and row bytes remain unchanged.
 */
#if defined(TA_R65_ROW_CACHE_WAYS) && TA_R65_ROW_CACHE_WAYS == 2
static XboxResolvedRow s_xboxResolvedRows[XBOX_ROW_CACHE_SIZE * 2u]
    __attribute__((aligned(32)));
static uint8_t s_taR65Victim[XBOX_ROW_CACHE_SIZE];
#else
static XboxResolvedRow s_xboxResolvedRows[XBOX_ROW_CACHE_SIZE]
    __attribute__((aligned(32)));
#endif
/* R66: leave the existing one-way primary cache and its fast hit intact.
 * Only an exact primary key conflict consults the optional direct-mapped
 * victim table, which stores a copy of the previously evicted primary row.
 * A victim hit is returned without disturbing the primary hot row.
 * 9 bits = 512 x 16-byte entries = 8 KiB; 0 disables this path entirely.
 */
#if defined(TA_R66_VICTIM_BITS) && TA_R66_VICTIM_BITS > 0
#define TA_R66_VICTIM_SIZE (1u << TA_R66_VICTIM_BITS)
static XboxResolvedRow s_taR66VictimRows[TA_R66_VICTIM_SIZE]
    __attribute__((aligned(32)));
#if defined(TA_R66_VICTIM_PROFILE) && TA_R66_VICTIM_PROFILE
static uint64_t s_taR66Probes[2], s_taR66Hits[2], s_taR66Fills[2];
void ppu_taR66VictimStats(unsigned layer, uint64_t values[3]) {
  if(!values) return;
  if(layer >= 2u) { values[0]=values[1]=values[2]=0; return; }
  values[0]=s_taR66Probes[layer];
  values[1]=s_taR66Hits[layer];
  values[2]=s_taR66Fills[layer];
}
#endif
#endif
static uint16_t s_xboxRowEpoch = 1;
#if defined(TA_R106_MODE2_CHAR_EPOCH) && TA_R106_MODE2_CHAR_EPOCH
/* Mode 2 uses aligned 16-word 4bpp characters. Map/OPT writes elsewhere
 * cannot change the eight resolved palette indices. Keep the 16-byte row
 * entries and original fast global-epoch hit. On a stale exact-key hit,
 * the side token can revalidate the row without expanding its pixels.
 * This experiment intentionally uses the production one-way/no-victim cache. */
#if (defined(TA_R65_ROW_CACHE_WAYS) && TA_R65_ROW_CACHE_WAYS == 2) || (defined(TA_R66_VICTIM_BITS) && TA_R66_VICTIM_BITS > 0)
#error R106 requires the production one-way cache without R66 victims
#endif
static uint16_t s_taR106CharEpoch[2048] __attribute__((aligned(32)));
static uint16_t s_taR106RowCharEpoch[XBOX_ROW_CACHE_SIZE];
#endif

/* R30 diagnostic: resolved-row cache behavior split by the two Mode-2 BGs.
 * Kept separate from perf-critical Release: no counter mutation when disabled.
 * Misses are split into epoch-invalid (VRAM write or cold) and key-conflict
 * cases, showing whether an associative cache or VRAM invalidation strategy
 * could be worth investigating next. */
#if defined(TA_R30_ROW_CACHE_PROFILE) && TA_R30_ROW_CACHE_PROFILE
static uint64_t s_taR30RowHits[2], s_taR30RowEpochMiss[2], s_taR30RowConflict[2];
void ppu_taR30RowStats(unsigned layer, uint64_t values[3]) {
  if(!values) return;
  if(layer >= 2u) { values[0]=values[1]=values[2]=0; return; }
  values[0]=s_taR30RowHits[layer];
  values[1]=s_taR30RowEpochMiss[layer];
  values[2]=s_taR30RowConflict[layer];
}
#endif


#if defined(TA_R43_LINE_OUTPUT_CACHE) && TA_R43_LINE_OUTPUT_CACHE && defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
/* Complete native Mode-2 output, keyed by validated R32 backgrounds and the
 * exact sprite/color inputs. Sprite evaluation and all hardware clocks still
 * execute. There are no frame hashes or probabilistic comparisons here. */
typedef struct {
  uint32_t paletteEpoch;
  Layer objects;
  WindowLayer window[2];
  uint8_t edges[4],brightness,fixed[3];
  bool math[6];
} TaR43OutputKey;
typedef struct {
  TaR43OutputKey key;
  bool valid;
  uint8_t objPixels[256],objPriorities[256];
  uint32_t colors[256];
} TaR43OutputLine;
static TaR43OutputLine s_taR43Output[PPU_LINES] __attribute__((aligned(32)));
static uint16_t s_taR43Palette[256];
static uint32_t s_taR43PaletteEpoch=1;
static bool s_taR43BgHit,s_taR43OutputComplete,s_taR43OutputTried;
static uint32_t* s_taR43OutputTarget;
#if defined(TA_R43_CACHE_PROFILE) && TA_R43_CACHE_PROFILE
static uint64_t s_taR43OutputHits,s_taR43OutputMisses;
void ppu_taR43OutputStats(uint64_t* hits,uint64_t* misses) {
  *hits=s_taR43OutputHits;*misses=s_taR43OutputMisses;
}
#endif
static inline void ppu_taR43InvalidateOutput(void) {
  for(unsigned i=0;i<PPU_LINES;++i)s_taR43Output[i].valid=false;
}
static inline void ppu_taR43ObservePalette(const Ppu* ppu) {
  if(memcmp(s_taR43Palette,ppu->cgram,sizeof s_taR43Palette)!=0) {
    memcpy(s_taR43Palette,ppu->cgram,sizeof s_taR43Palette);
    if(++s_taR43PaletteEpoch==0) {s_taR43PaletteEpoch=1;ppu_taR43InvalidateOutput();}
  }
}
static bool ppu_taR43RestoreOutput(const Ppu* ppu,unsigned row,uint32_t* out) {
  if(row>=PPU_LINES)return false;
  TaR43OutputLine* e=&s_taR43Output[row];
  TaR43OutputKey key;memset(&key,0,sizeof key);
  key.paletteEpoch=s_taR43PaletteEpoch;key.objects=ppu->layer[4];
  key.window[0]=ppu->windowLayer[4];key.window[1]=ppu->windowLayer[5];
  key.edges[0]=ppu->window1left;key.edges[1]=ppu->window1right;
  key.edges[2]=ppu->window2left;key.edges[3]=ppu->window2right;
  key.brightness=ppu->brightness;key.fixed[0]=ppu->fixedColorR;
  key.fixed[1]=ppu->fixedColorG;key.fixed[2]=ppu->fixedColorB;
  memcpy(key.math,ppu->mathEnabled,sizeof key.math);
  const bool hit=s_taR43BgHit && e->valid && memcmp(&key,&e->key,sizeof key)==0 &&
      memcmp(e->objPixels,ppu->objPixelBuffer,256)==0 &&
      memcmp(e->objPriorities,ppu->objPriorityBuffer,256)==0;
  if(hit) {
#if defined(TA_R43_CACHE_PROFILE) && TA_R43_CACHE_PROFILE
    ++s_taR43OutputHits;
#endif
    memcpy(out,e->colors,sizeof e->colors);return true;
  }
#if defined(TA_R43_CACHE_PROFILE) && TA_R43_CACHE_PROFILE
  ++s_taR43OutputMisses;
#endif
  e->valid=false;e->key=key;
  memcpy(e->objPixels,ppu->objPixelBuffer,256);memcpy(e->objPriorities,ppu->objPriorityBuffer,256);
  return false;
}
static inline void ppu_taR43StoreOutput(unsigned row,const uint32_t* out) {
  if(row<PPU_LINES) {
    TaR43OutputLine* e=&s_taR43Output[row];memcpy(e->colors,out,sizeof e->colors);e->valid=true;
  }
}
#endif

/* Startup metadata only; no work is added to the rendering hot path. */
uint32_t ppu_taR43OutputCacheBytes(void) {
#if defined(TA_R43_LINE_OUTPUT_CACHE) && TA_R43_LINE_OUTPUT_CACHE && defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
  return (uint32_t)(sizeof s_taR43Output+sizeof s_taR43Palette);
#else
  return 0;
#endif
}

/* R32: reuse the fully composed Mode-2 BACKGROUNDS, before sprites and
 * color output. Each entry records the exact rendering registers and only
 * the VRAM pages actually consumed (map, OPT and character data). Changed
 * writes invalidate by page generation; same-value DMA writes do no work.
 * Sprite evaluation, window preparation, color math and clocks still run.
 * 128-word pages: an aligned 4bpp/2bpp character never crosses a page. */
#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
typedef struct {
  BgLayer bg[3];
  Layer layer[2];
  WindowLayer window[2];
  uint8_t edges[4];
  bool needSub, trackLayer;
} TaR32BGKey;
typedef struct {
  TaR32BGKey key;
  uint32_t generation[256];
  uint8_t page[256];
  uint16_t count;
  bool valid;
#if defined(TA_R71_MODE2_VRAM_STAMP) && TA_R71_MODE2_VRAM_STAMP
  uint32_t validatedVramStamp; /* after exact page checks or a full BG build */
#endif
  uint8_t mainPri[256], mainPix[256], mainLayer[256];
  uint8_t subPri[256], subPix[256], subLayer[256];
} TaR32BGLine;
#if defined(TA_R88_PACKED_BG_COPY) && TA_R88_PACKED_BG_COPY
/* Verify the cached source planes are contiguous as well as the targets. */
typedef char TaR88CacheMainPixOffset[(offsetof(TaR32BGLine,mainPix)-offsetof(TaR32BGLine,mainPri)==256)?1:-1];
typedef char TaR88CacheMainLayerOffset[(offsetof(TaR32BGLine,mainLayer)-offsetof(TaR32BGLine,mainPri)==512)?1:-1];
typedef char TaR88CacheSubPriOffset[(offsetof(TaR32BGLine,subPri)-offsetof(TaR32BGLine,mainPri)==768)?1:-1];
typedef char TaR88CacheSubPixOffset[(offsetof(TaR32BGLine,subPix)-offsetof(TaR32BGLine,subPri)==256)?1:-1];
typedef char TaR88CacheSubLayerOffset[(offsetof(TaR32BGLine,subLayer)-offsetof(TaR32BGLine,subPri)==512)?1:-1];
#endif
static TaR32BGLine s_taR32Lines[PPU_LINES] __attribute__((aligned(32)));
#if defined(TA_R110_MODE2_FIRST_BG_CACHE) && TA_R110_MODE2_FIRST_BG_CACHE
static TaR32BGLine s_taR110FirstBG[PPU_LINES] __attribute__((aligned(32)));
#endif
#if defined(TA_R70_MODE1_LINE_CACHE) && TA_R70_MODE1_LINE_CACHE
/* Keep R32's proven Mode-2 cache and its lookup cost unchanged.  Mode 1
 * requires BG3's rendering inputs plus the BG3-priority-order bit in the key.
 * Separate per-line entries prevent Mode 1/2 evicting one another. */
typedef struct {
  BgLayer bg3;
  Layer layer3;
  WindowLayer window3;
  uint8_t bg3priority;
} TaR70Mode1ExtraKey;
static TaR32BGLine s_taR70Mode1Lines[PPU_LINES] __attribute__((aligned(32)));
static TaR70Mode1ExtraKey s_taR70Mode1Extras[PPU_LINES];
#if defined(TA_R70_CACHE_PROFILE) && TA_R70_CACHE_PROFILE
static uint64_t s_taR70Mode1Hits, s_taR70Mode1Misses;
void ppu_taR70Mode1Stats(uint64_t* hits, uint64_t* misses) {
  if(hits) *hits=s_taR70Mode1Hits;
  if(misses) *misses=s_taR70Mode1Misses;
}
#endif
#endif

static uint32_t s_taR32PageGeneration[256];
#if defined(TA_R123_REVERSE_MODE2_DIRTY) && TA_R123_REVERSE_MODE2_DIRTY
/* R123: reverse VRAM-page dependencies for the existing 224-row Mode2 BG
 * cache. A write only queues the page (O(1)); a subsequent cache-hit query
 * merges the queued page's dependent rows. Exact old associations are
 * removed before a cache entry is rebuilt, preventing historical growth.
 * The normal per-page generation scan remains unchanged when disabled. */
#define TA_R123_ROW_WORDS ((PPU_LINES + 31u) / 32u)
static uint32_t s_taR123Reverse[256][TA_R123_ROW_WORDS];
static uint32_t s_taR123DirtyRows[TA_R123_ROW_WORDS];
static uint8_t s_taR123QueuedPages[256];
static uint8_t s_taR123PageQueued[256];
static unsigned s_taR123QueuedCount;
static inline void ppu_taR123QueueWrite(unsigned page) {
  if(!s_taR123PageQueued[page]) {
    s_taR123PageQueued[page]=1;
    s_taR123QueuedPages[s_taR123QueuedCount++]=(uint8_t)page;
  }
}
static inline void ppu_taR123ApplyQueued(void) {
  const unsigned n=s_taR123QueuedCount;
  if(!n) return;
  for(unsigned i=0;i<n;++i) {
    const unsigned page=s_taR123QueuedPages[i];
    s_taR123PageQueued[page]=0;
    for(unsigned word=0;word<TA_R123_ROW_WORDS;++word)
      s_taR123DirtyRows[word] |= s_taR123Reverse[page][word];
  }
  s_taR123QueuedCount=0;
}
static inline void ppu_taR123UnregisterMode2(TaR32BGLine* e) {
  /* Pointer range check mirrors R88's existing Mode2 cache identity test. */
  const uintptr_t ptr=(uintptr_t)e;
  const uintptr_t begin=(uintptr_t)&s_taR32Lines[0];
  const uintptr_t end=(uintptr_t)&s_taR32Lines[PPU_LINES];
  if(ptr<begin || ptr>=end) return;
  const unsigned row=(unsigned)((ptr-begin)/sizeof(TaR32BGLine));
  const uint32_t keep=~(1u<<(row&31u));
  for(unsigned i=0;i<e->count;++i)
    s_taR123Reverse[e->page[i]][row>>5] &= keep;
  /* The new BG render will read post-write state and replace this row. */
  s_taR123DirtyRows[row>>5] &= keep;
}
static inline void ppu_taR123RegisterMode2(TaR32BGLine* e) {
  const uintptr_t ptr=(uintptr_t)e;
  const uintptr_t begin=(uintptr_t)&s_taR32Lines[0];
  const uintptr_t end=(uintptr_t)&s_taR32Lines[PPU_LINES];
  if(ptr<begin || ptr>=end) return;
  const unsigned row=(unsigned)((ptr-begin)/sizeof(TaR32BGLine));
  const uint32_t bit=1u<<(row&31u);
  for(unsigned i=0;i<e->count;++i)
    s_taR123Reverse[e->page[i]][row>>5] |= bit;
}
#endif

#if defined(TA_R71_MODE2_VRAM_STAMP) && TA_R71_MODE2_VRAM_STAMP
/* No new cache or per-pixel work. Every changed VRAM word advances this
 * counter through the EXISTING tile-word invalidation entry point. A Mode-2
 * cache hit validated at the current stamp cannot have stale page deps.
 * Any write forces the original exact per-page checks again. */
static uint32_t s_taR71VramStamp=1;
#if defined(TA_R71_STAMP_PROFILE) && TA_R71_STAMP_PROFILE
static uint64_t s_taR71BypassCount, s_taR71CheckedCount;
void ppu_taR71StampStats(uint64_t* bypass, uint64_t* checked) {
  if(bypass) *bypass=s_taR71BypassCount;
  if(checked) *checked=s_taR71CheckedCount;
}
#endif
#endif

#if defined(TA_R107_MODE2_OPT_TABLE) && TA_R107_MODE2_OPT_TABLE
static bool s_taR107OptValid;
#endif
static uint32_t s_taR32Seen[8];
static TaR32BGLine* s_taR32Recording;
static bool s_taR32CacheActive;
#if defined(TA_R86_KEY_AUDIT) && TA_R86_KEY_AUDIT
static uint32_t s_taR86KeyMatches, s_taR86KeyMisses;
void ppu_taR86KeyAudit(uint32_t* matches, uint32_t* misses) {
  if(matches) *matches=s_taR86KeyMatches;
  if(misses) *misses=s_taR86KeyMisses;
}
#endif
#if defined(TA_R32_CACHE_PROFILE) && TA_R32_CACHE_PROFILE
static uint32_t s_taR32Hits, s_taR32Misses;
void ppu_taR32CacheStats(uint32_t* hits, uint32_t* misses) {
  *hits=s_taR32Hits; *misses=s_taR32Misses;
}
#endif
static inline void ppu_taR32InvalidateAll(void) {
#if defined(TA_R107_MODE2_OPT_TABLE) && TA_R107_MODE2_OPT_TABLE
  s_taR107OptValid=false;
#endif
  for(unsigned i=0;i<PPU_LINES;++i) s_taR32Lines[i].valid=false;
#if defined(TA_R110_MODE2_FIRST_BG_CACHE) && TA_R110_MODE2_FIRST_BG_CACHE
  for(unsigned i=0;i<PPU_LINES;++i) s_taR110FirstBG[i].valid=false;
#endif
#if defined(TA_R70_MODE1_LINE_CACHE) && TA_R70_MODE1_LINE_CACHE
  for(unsigned i=0;i<PPU_LINES;++i) s_taR70Mode1Lines[i].valid=false;
#endif

#if defined(TA_R123_REVERSE_MODE2_DIRTY) && TA_R123_REVERSE_MODE2_DIRTY
  /* Every cached entry was invalidated above; no pending row can survive. */
  memset(s_taR123DirtyRows,0,sizeof s_taR123DirtyRows);
  memset(s_taR123PageQueued,0,sizeof s_taR123PageQueued);
  s_taR123QueuedCount=0;
#endif
  s_taR32Recording=NULL; s_taR32CacheActive=false;
#if defined(TA_R43_LINE_OUTPUT_CACHE) && TA_R43_LINE_OUTPUT_CACHE
  ppu_taR43InvalidateOutput();
#endif
}
static inline void ppu_taR32Dependency(uint16_t adr) {
  if(s_taR32Recording) {
    const unsigned page=(adr & 0x7fffu)>>7;
    const uint32_t bit=1u<<(page & 31u);
    if(!(s_taR32Seen[page>>5] & bit)) {
      s_taR32Seen[page>>5] |= bit;
      const unsigned n=s_taR32Recording->count++;
      s_taR32Recording->page[n]=(uint8_t)page;
      s_taR32Recording->generation[n]=s_taR32PageGeneration[page];
    }
  }
}
#endif

static inline void ppu_xboxBumpRowEpoch(void) {
  if(++s_xboxRowEpoch == 0) {
    memset(s_xboxResolvedRows, 0, sizeof(s_xboxResolvedRows));
#if defined(TA_R66_VICTIM_BITS) && TA_R66_VICTIM_BITS > 0
    /* Wrap must clear all old epoch-1 victims too; they cannot revalidate. */
    memset(s_taR66VictimRows, 0, sizeof(s_taR66VictimRows));
#endif
    s_xboxRowEpoch = 1;
  }
}

/* R20 native-width Xbox target. The SNES is 256 pixels wide; the old frontend
   made the software PPU write every pixel twice horizontally and then copied
   every scanline twice vertically into a 512x480 texture. During the normal
   non-interlaced Xbox path, fast Mode-1 scanlines can land directly in the
   locked D3D texture at 256x224/239. Generic scanlines are decimated at the
   end of the frame, preserving their exact renderer as a fallback. */
/* R39: store each native Mode-1/2 color once. Rows retain their legacy stride,
 * so generic/high-resolution rows can coexist without losing either pixel.
 * Consumers requesting the legacy image get exact horizontal duplication. */
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
/* Presentation-only aliases. The guest machine cannot observe framebuffer
 * storage. Resolve before public pixel reads, fallback rendering, reset or
 * reusing a texture allocation. No guest/save-state fields are added. */
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
#include "ta_r127_indexed.h"
static TaR127Palette s_r127Palettes[3][TA_R127_PALETTES];
static const TaR127Palette* s_r127Alias[478];
static bool s_r127Rows[240],s_r127Enabled,s_r127Ready;
static unsigned s_r127Slot,s_r127Count,s_r127CurrentIndex,s_r127FixedCount,s_r127FixedIndex;
static uint32_t s_r127PaletteSerial=1,s_r127LastSerial;
static uint32_t s_r127LastFixed;
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
static bool s_r129Alias[478],s_r129Rows[240],s_r129Enabled,s_r129Ready;
#endif
#endif
static Ppu* s_r126Owner;
static const uint32_t *s_r126External[478],*s_r126Second[478];
static uint8_t* s_r126GpuSub;
static unsigned s_r126GpuPitch;
static bool s_r126GpuRows[240],s_r126GpuReady,s_r126Managed,s_r126CaptureValid,s_r126CaptureEven;
static uint32_t ppu_r126Color(uint32_t a,uint32_t b){
 uint32_t d=(((a>>3)&0x1f1f1fu)|0x202020u)-((b>>3)&0x1f1f1fu);
 uint32_t five=d&(((d>>5)&0x10101u)*31u);
 return (five<<3)|((five>>2)&0x70707u);
}
static void ppu_r126ResolveRow(Ppu*p,unsigned row){
 if(p!=s_r126Owner||row>=478||!s_r126External[row])return;
 uint32_t*out=(uint32_t*)&p->pixelBuffer[row*PPU_ROW_BYTES];
 const uint32_t*a=s_r126External[row],*b=s_r126Second[row];
 #if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
 if(s_r127Alias[row]){
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
  if(s_r129Alias[row]){
   for(unsigned x=0;x<256;x++)out[x]=s_r127Alias[row]->color[a[x]>>24];
   s_r129Alias[row]=false;
  }else
#endif
  for(unsigned x=0;x<256;x++)out[x]=ta_r127_decode(a[x],s_r127Alias[row]);
  s_r127Alias[row]=NULL;
 }else
#endif
 if(b){for(unsigned x=0;x<256;x++)out[x]=ppu_r126Color(a[x],b[x]);}
 else memcpy(out,a,1024);
 p->taNative256Row[row]=1;s_r126External[row]=s_r126Second[row]=NULL;
}
void ppu_r126ResolveAll(Ppu*p){if(p==s_r126Owner)for(unsigned row=0;row<478;row++)ppu_r126ResolveRow(p,row);}
#if defined(TA_VS_ACTIVE_CAPTURE) && TA_VS_ACTIVE_CAPTURE
/* A non-interlaced active-image read consumes one field only. Keep the other
 * field's exact aliases alive until a full read or target reuse needs them.
 * Reading those GPU texture rows here was unnecessary CPU readback in LAN. */
static void ppu_r126ResolveActive224(Ppu*p){
 if(p==s_r126Owner){unsigned base=p->evenFrame?0u:239u;for(unsigned y=0;y<224;y++)ppu_r126ResolveRow(p,base+y);}
}
#endif
static void ppu_r126Forget(Ppu*p){if(p==s_r126Owner){
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
 memset(s_r127Alias,0,sizeof s_r127Alias);s_r127Enabled=false;
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
 memset(s_r129Alias,0,sizeof s_r129Alias);s_r129Enabled=false;
#endif
#endif
memset(s_r126External,0,sizeof s_r126External);memset(s_r126Second,0,sizeof s_r126Second);s_r126Owner=NULL;}}
void ppu_r126Detach(Ppu*p,const uint8_t*base,unsigned capacity){
 if(p!=s_r126Owner)return;uintptr_t lo=(uintptr_t)base,hi=lo+capacity;
 for(unsigned row=0;row<478;row++){uintptr_t a=(uintptr_t)s_r126External[row],b=(uintptr_t)s_r126Second[row];
  if((a>=lo&&a<hi)||(b>=lo&&b<hi))ppu_r126ResolveRow(p,row);}
}
void ppu_r126SetGpuTarget(uint8_t*sub,unsigned pitch){s_r126GpuSub=sub;s_r126GpuPitch=pitch;s_r126Managed=true;}
bool ppu_r126GpuReady(void){return s_r126GpuReady;}
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
void ppu_r127SetIndexedTarget(Ppu*p,bool enabled,unsigned slot){
 slot%=3;
 /* Palette snapshots have the same rotating lifetime as their source texture.
  * Materialize outstanding references before this slot is repopulated. */
 for(unsigned row=0;row<478;row++){
  uintptr_t a=(uintptr_t)s_r127Alias[row],lo=(uintptr_t)&s_r127Palettes[slot][0];
  if(a>=lo&&a<lo+sizeof s_r127Palettes[slot])ppu_r126ResolveRow(p,row);
 }
 s_r127Slot=slot;s_r127Enabled=enabled;s_r127Ready=false;s_r127Count=0;s_r127FixedCount=0;
 s_r127LastSerial=0;memset(s_r127Rows,0,sizeof s_r127Rows);
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
 memset(s_r129Rows,0,sizeof s_r129Rows);s_r129Enabled=s_r129Ready=false;
#endif
}
bool ppu_r127IndexedReady(void){return s_r127Ready;}
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
void ppu_r129SetDirectTarget(bool enabled){s_r129Enabled=enabled;}
bool ppu_r129DirectReady(void){return s_r129Ready;}
#endif
const TaR127Palette* ppu_r127PaletteData(unsigned* count){if(count)*count=s_r127Count;return s_r127Palettes[s_r127Slot];}
#endif
#endif
static inline void ppu_taMaterializeRow(Ppu* ppu, unsigned row) {
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  ppu_r126ResolveRow(ppu,row);
#endif
#if defined(TA_R39_NATIVE256) && TA_R39_NATIVE256
  if(row < 478u && ppu->taNative256Row[row]) {
    uint32_t* pixels=(uint32_t*)&ppu->pixelBuffer[row * PPU_ROW_BYTES];
    for(int x=255;x>=0;--x) {
      const uint32_t color=pixels[x];
      pixels[2*x]=color; pixels[2*x+1]=color;
    }
    ppu->taNative256Row[row]=0;
  }
#else
  (void)ppu; (void)row;
#endif
}
void ppu_taMaterializeNativeRows(Ppu* ppu) {
#if defined(TA_R39_NATIVE256) && TA_R39_NATIVE256
  for(unsigned row=0;row<478u;++row) ppu_taMaterializeRow(ppu,row);
#else
  (void)ppu;
#endif
}
static uint8_t* s_xboxFrameTarget = NULL;
static int s_xboxFramePitch = 0;
static uint8_t s_xboxLineNative[PPU_LINES];

#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
/*
 * R35 native object pipeline.
 *
 * OAM is normally uploaded during vblank and then remains unchanged while the
 * visible frame is drawn. R23 still scanned all 128 OAM entries independently
 * for every one of ~224 visible lines. Build the exact SNES-order list once
 * per OAM/config epoch instead. A mid-frame OAM/config write bumps the epoch,
 * so the next line rebuilds from the new state rather than using stale data.
 */
static uint32_t s_xboxSpriteEpoch = 1;
static uint32_t s_xboxSpriteBuiltEpoch = 0;
static uint8_t s_xboxSpriteLineCount[PPU_LINES];
static uint8_t s_xboxSpriteLineRangeOver[PPU_LINES];
static uint8_t s_xboxSpriteLineIndex[PPU_LINES][32] __attribute__((aligned(32)));
#endif

void ppu_xboxInvalidateSpriteCache(void) {
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
  if(++s_xboxSpriteEpoch == 0) {
    s_xboxSpriteEpoch = 1;
    s_xboxSpriteBuiltEpoch = 0;
  }
#endif
}

static inline void ppu_xboxInvalidateTileWord(uint16_t wordAdr) {
  const unsigned w = wordAdr & 0x7fffu;
#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
  if(s_taR32CacheActive) {
#if defined(TA_R71_MODE2_VRAM_STAMP) && TA_R71_MODE2_VRAM_STAMP
    /* A 32-bit wrap invalidates every previously validated cache entry. */
    if(++s_taR71VramStamp==0) {ppu_taR32InvalidateAll(); s_taR71VramStamp=1;}
#endif
    if(++s_taR32PageGeneration[w >> 7] == 0) ppu_taR32InvalidateAll();
#if defined(TA_R123_REVERSE_MODE2_DIRTY) && TA_R123_REVERSE_MODE2_DIRTY
    ppu_taR123QueueWrite(w >> 7);
#endif
  }
#endif
#if defined(TA_R106_MODE2_CHAR_EPOCH) && TA_R106_MODE2_CHAR_EPOCH
  /* Run in every PPU mode, even before the full-line cache is active.
   * Clear before recycling a token so an old row can never resurrect. */
  if(++s_taR106CharEpoch[w >> 4] == 0)
    memset(s_xboxResolvedRows, 0, sizeof s_xboxResolvedRows);
#endif
  s_xboxTile4Valid[w >> 4] = 0;
  s_xboxTile2Valid[w >> 3] = 0;
  ppu_xboxBumpRowEpoch();
}

static inline void ppu_xboxInvalidateTileCache(void) {
#if defined(TA_R106_MODE2_CHAR_EPOCH) && TA_R106_MODE2_CHAR_EPOCH
  /* State loads/reset can replace VRAM without individual word writes. */
  memset(s_xboxResolvedRows, 0, sizeof s_xboxResolvedRows);
#endif
#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
  ppu_taR32InvalidateAll();
#endif
  memset(s_xboxTile4Valid, 0, sizeof(s_xboxTile4Valid));
  memset(s_xboxTile2Valid, 0, sizeof(s_xboxTile2Valid));
  ppu_xboxBumpRowEpoch();
}

static inline const uint8_t* ppu_xboxTile4Pixels(Ppu* ppu, uint16_t base) {
  const unsigned idx = (base & 0x7fffu) >> 4;
  if(__builtin_expect(!s_xboxTile4Valid[idx], 0)) {
    uint8_t* dst = s_xboxTile4[idx];
    const uint16_t tileBase = (uint16_t)(idx << 4);
    for(int row = 0; row < 8; ++row) {
      const uint16_t p0 = ppu->vram[(tileBase + row) & 0x7fff];
      const uint16_t p1 = ppu->vram[(tileBase + 8 + row) & 0x7fff];
      uint8_t* d = dst + row * 8;
      for(int x = 0; x < 8; ++x) {
        const int bit = 7 - x;
        const uint8_t px = (uint8_t)(((p0 >> bit) & 1) |
                         (((p0 >> (8 + bit)) & 1) << 1) |
                         (((p1 >> bit) & 1) << 2) |
                         (((p1 >> (8 + bit)) & 1) << 3));
        d[x] = px;
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
        s_xboxTile4Flip[idx][row * 8 + (7 - x)] = px;
#endif
      }
    }
    s_xboxTile4Valid[idx] = 1;
#if !defined(ZAMN_R20_LIGHT_PROF)
    TA_DIAG(++s_xboxTileCacheMisses);
#endif
  } else {
#if !defined(ZAMN_R20_LIGHT_PROF)
    TA_DIAG(++s_xboxTileCacheHits);
#endif
  }
  return s_xboxTile4[idx];
}

#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
static inline const uint8_t* ppu_xboxTile4PixelsFlipped(Ppu* ppu, uint16_t base) {
  const unsigned idx = (base & 0x7fffu) >> 4;
  (void)ppu_xboxTile4Pixels(ppu, base);
  return s_xboxTile4Flip[idx];
}
#endif

static inline const uint8_t* ppu_xboxTile2Pixels(Ppu* ppu, uint16_t base) {
  const unsigned idx = (base & 0x7fffu) >> 3;
  if(__builtin_expect(!s_xboxTile2Valid[idx], 0)) {
    uint8_t* dst = s_xboxTile2[idx];
    const uint16_t tileBase = (uint16_t)(idx << 3);
    for(int row = 0; row < 8; ++row) {
      const uint16_t p0 = ppu->vram[(tileBase + row) & 0x7fff];
      uint8_t* d = dst + row * 8;
      for(int x = 0; x < 8; ++x) {
        const int bit = 7 - x;
        d[x] = (uint8_t)(((p0 >> bit) & 1) |
                         (((p0 >> (8 + bit)) & 1) << 1));
      }
    }
    s_xboxTile2Valid[idx] = 1;
#if !defined(ZAMN_R20_LIGHT_PROF)
    TA_DIAG(++s_xboxTileCacheMisses);
#endif
  } else {
#if !defined(ZAMN_R20_LIGHT_PROF)
    TA_DIAG(++s_xboxTileCacheHits);
#endif
  }
  return s_xboxTile2[idx];
}

static inline const uint8_t* ppu_xboxResolvedRowPixels(
    Ppu* ppu, int layer, uint16_t tile, uint16_t base, int row
#if defined(ZAMN_R32_PPU_HOISTS) || (defined(TA_R67_BG_FAST) && TA_R67_BG_FAST)
    , uint16_t key1, uint32_t hashSalt
#endif
    ) {
#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
  ppu_taR32Dependency(base);
#endif
  const uint32_t key0 = (uint32_t)tile | ((uint32_t)base << 16);
#if !defined(ZAMN_R32_PPU_HOISTS) && !(defined(TA_R67_BG_FAST) && TA_R67_BG_FAST)
  uint16_t key1 = (uint16_t)((row & 7) | ((layer & 3) << 3));
  const uint32_t hashSalt = (uint32_t)key1 * 0x9e3779b1u;
#endif
  uint32_t h = key0 ^ hashSalt;
  h ^= h >> 16;
#if defined(TA_R65_ROW_CACHE_WAYS) && TA_R65_ROW_CACHE_WAYS == 2
  /* Keep the R22 hash unchanged, and probe both exact entries in that set.
   * A two-way hit never decodes any tile bytes or changes the rendered row.
   * Invalid ways are used first. When both are live, evict the LRU way.
   * Returned pointers remain valid until another cache lookup evicts them,
   * just as in the original single-way cache. */
  const unsigned set = h & (XBOX_ROW_CACHE_SIZE - 1u);
  XboxResolvedRow* const first = &s_xboxResolvedRows[set * 2u];
  XboxResolvedRow* const second = first + 1;
  XboxResolvedRow* e;
  if(__builtin_expect(first->epoch == s_xboxRowEpoch &&
                      first->key0 == key0 && first->key1 == key1, 1)) {
    s_taR65Victim[set] = 1;
#if defined(TA_R30_ROW_CACHE_PROFILE) && TA_R30_ROW_CACHE_PROFILE
    if((unsigned)layer < 2u) ++s_taR30RowHits[layer];
#endif
    return first->pixels;
  }
  if(second->epoch == s_xboxRowEpoch &&
     second->key0 == key0 && second->key1 == key1) {
    s_taR65Victim[set] = 0;
#if defined(TA_R30_ROW_CACHE_PROFILE) && TA_R30_ROW_CACHE_PROFILE
    if((unsigned)layer < 2u) ++s_taR30RowHits[layer];
#endif
    return second->pixels;
  }
#if defined(TA_R30_ROW_CACHE_PROFILE) && TA_R30_ROW_CACHE_PROFILE
  if((unsigned)layer < 2u) {
    if(first->epoch != s_xboxRowEpoch && second->epoch != s_xboxRowEpoch)
      ++s_taR30RowEpochMiss[layer];
    else ++s_taR30RowConflict[layer];
  }
#endif
  e = first->epoch != s_xboxRowEpoch ? first :
      (second->epoch != s_xboxRowEpoch ? second :
       first + (s_taR65Victim[set] & 1u));
  s_taR65Victim[set] = (uint8_t)(e == first ? 1u : 0u);
#else
  /* Original direct-map fast hit. R66 adds ZERO per-hit metadata writes. */
  XboxResolvedRow* e = &s_xboxResolvedRows[h & (XBOX_ROW_CACHE_SIZE - 1u)];

  if(__builtin_expect(e->epoch == s_xboxRowEpoch &&
                      e->key0 == key0 && e->key1 == key1, 1)) {
#if defined(TA_R30_ROW_CACHE_PROFILE) && TA_R30_ROW_CACHE_PROFILE
    if((unsigned)layer < 2u) ++s_taR30RowHits[layer];
#endif
    return e->pixels;
  }
#if defined(TA_R106_MODE2_CHAR_EPOCH) && TA_R106_MODE2_CHAR_EPOCH
  /* Preserve the original fast hit above. Only stale exact-key Mode2 rows
   * consult character versions. Nonzero epoch proves this entry was filled;
   * full invalidation and either token wrap clear it before tokens recycle.
   * Key1 already separates bit depth/layer; both modes resolve identical
   * bytes for identical keys, so validated rows may safely share the table. */
  if(ppu->mode == 2 && (unsigned)layer < 2u && e->epoch != 0 &&
     e->key0 == key0 && e->key1 == key1 &&
     s_taR106RowCharEpoch[h & (XBOX_ROW_CACHE_SIZE - 1u)] ==
         s_taR106CharEpoch[(base & 0x7fffu) >> 4]) {
    e->epoch = s_xboxRowEpoch;
#if defined(TA_R30_ROW_CACHE_PROFILE) && TA_R30_ROW_CACHE_PROFILE
    ++s_taR30RowHits[layer];
#endif
    return e->pixels;
  }
#endif
#if defined(TA_R66_VICTIM_BITS) && TA_R66_VICTIM_BITS > 0
  /* A primary epoch miss cannot have a matching victim under this epoch
   * for this primary set, since only a live primary row can fill a victim. */
  if(e->epoch == s_xboxRowEpoch) {
    XboxResolvedRow* const victim =
        &s_taR66VictimRows[h & (TA_R66_VICTIM_SIZE - 1u)];
#if defined(TA_R66_VICTIM_PROFILE) && TA_R66_VICTIM_PROFILE
    if((unsigned)layer < 2u) ++s_taR66Probes[layer];
#endif
    if(victim->epoch == s_xboxRowEpoch &&
       victim->key0 == key0 && victim->key1 == key1) {
#if defined(TA_R66_VICTIM_PROFILE) && TA_R66_VICTIM_PROFILE
      if((unsigned)layer < 2u) ++s_taR66Hits[layer];
#endif
#if defined(TA_R30_ROW_CACHE_PROFILE) && TA_R30_ROW_CACHE_PROFILE
      if((unsigned)layer < 2u) ++s_taR30RowHits[layer];
#endif
      return victim->pixels;
    }
    /* True conflict: retain the primary row before replacing it.
     * The original primary's exact keys, epoch and eight pixels are copied.
     * The victim isn't promoted on a hit, avoiding primary-cache thrashing. */
    *victim = *e;
#if defined(TA_R66_VICTIM_PROFILE) && TA_R66_VICTIM_PROFILE
    if((unsigned)layer < 2u) ++s_taR66Fills[layer];
#endif
  }
#endif
#if defined(TA_R30_ROW_CACHE_PROFILE) && TA_R30_ROW_CACHE_PROFILE
  if((unsigned)layer < 2u) {
    if(e->epoch != s_xboxRowEpoch) ++s_taR30RowEpochMiss[layer];
    else ++s_taR30RowConflict[layer];
  }
#endif
#endif

  const int bitDepth = (layer == 2) ? 2 : 4;
  const int paletteSize = 1 << bitDepth;
  const int paletteNum = (tile & 0x1c00) >> 10;
  const bool hflip = (tile & 0x4000) != 0;
  const uint8_t* decoded = bitDepth == 4
      ? ppu_xboxTile4Pixels(ppu, base)
      : ppu_xboxTile2Pixels(ppu, base);
  const uint8_t* src = decoded + (row & 7) * 8;
  const int palBase = paletteSize * paletteNum;

#if defined(TA_R31_ROW_PACKED) && TA_R31_ROW_PACKED
  /* Exact 8-byte palette expansion: decoded pixel is 0..15 (or 0..3).
     Each nonzero byte receives the same palette base; zero is transparent.
     The arithmetic is lane-local because each result fits in one byte. */
  uint32_t a, b;
  memcpy(&a, src, 4);
  memcpy(&b, src + 4, 4);
  if(hflip) {
    const uint32_t t = __builtin_bswap32(b);
    b = __builtin_bswap32(a);
    a = t;
  }
  const uint32_t add = (uint32_t)palBase * 0x01010101u;
  const uint32_t am = (((a & 0x7f7f7f7fu) + 0x7f7f7f7fu) | a) & 0x80808080u;
  const uint32_t bm = (((b & 0x7f7f7f7fu) + 0x7f7f7f7fu) | b) & 0x80808080u;
  a += add & ((am >> 7) * 255u);
  b += add & ((bm >> 7) * 255u);
  memcpy(e->pixels, &a, 4);
  memcpy(e->pixels + 4, &b, 4);
#else
  for(int i = 0; i < 8; ++i) {
    const int p = hflip ? src[7 - i] : src[i];
    e->pixels[i] = p ? (uint8_t)(p + palBase) : 0;
  }
#endif
#if defined(TA_R106_MODE2_CHAR_EPOCH) && TA_R106_MODE2_CHAR_EPOCH
  /* Record even Mode1 fills: the exact same 4bpp key can later be used in
   * Mode2. Layer2 entries cannot match a Mode2 BG1/BG2 key. */
  s_taR106RowCharEpoch[h & (XBOX_ROW_CACHE_SIZE - 1u)] =
      s_taR106CharEpoch[(base & 0x7fffu) >> 4];
#endif
  e->key0 = key0;
  e->key1 = key1;
  e->epoch = s_xboxRowEpoch;
  return e->pixels;
}

#if defined(TA_R27_SPRITE_GROUP_SKIP) && TA_R27_SPRITE_GROUP_SKIP
static uint64_t s_taR27SpriteEmptyGroups;
static uint64_t s_taR27SpriteLiveGroups;
uint64_t ppu_taR27EmptySpriteGroups(void) { return s_taR27SpriteEmptyGroups; }
uint64_t ppu_taR27LiveSpriteGroups(void) { return s_taR27SpriteLiveGroups; }
#endif

/* R28: sampled StageClock diagnostic. Read the cycle counter on one in 32
 * visible Mode-2 scanlines, not every pixel or tile. No PPU behavior changes.
 * Ticks are relative and sampling incurs some overhead; compare stage shares,
 * not these values as absolute wall-clock time. */
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
static uint64_t s_taR28Ticks[2][5]; /* mode1/2 x setup, BG1, BG2, sprites, output */
static uint32_t s_taR28Samples[2], s_taR28ColorPath[2][4];
/* Diagnostic: histogram of common Mode-2 color-math register choices. */
static uint32_t s_taR28ColorConfig[256];
static uint32_t s_taR28MaskedLines;
uint32_t ppu_taR28MaskedLines(void) { return s_taR28MaskedLines; }
void ppu_taR28ConfigTop(unsigned rank, unsigned *key, uint32_t *hits) {
  unsigned best=256;uint32_t count=0;
  /* Return Nth-most-common config without sorting the live array. */
  uint32_t after=UINT32_MAX;
  unsigned past=0;
  for(unsigned i=0;i<=rank;i++) {
    best=256;count=0;
    for(unsigned j=0;j<256;j++) {
      const uint32_t v=s_taR28ColorConfig[j];
      if(!v || v>after || (v==after && j<=past && i>0))continue;
      if(v>count || (v==count && j<best)) {count=v;best=j;}
    }
    if(best>=256)break;
    after=count;past=best;
  }
  if(key)*key=best;
  if(hits)*hits=count;
}

static inline void ppu_taR28Record(unsigned mode, uint64_t t0, uint64_t t1,
                                   uint64_t t2, uint64_t t3, uint64_t t4) {
  const uint64_t t5 = __builtin_readcyclecounter();
  const unsigned j=mode - 1;
  s_taR28Ticks[j][0] += t1 - t0;
  s_taR28Ticks[j][1] += t2 - t1;
  s_taR28Ticks[j][2] += t3 - t2;
  s_taR28Ticks[j][3] += t4 - t3;
  s_taR28Ticks[j][4] += t5 - t4;
  ++s_taR28Samples[j];
}
void ppu_taR28StageRead(unsigned mode, uint64_t ticks[5], uint32_t *samples, uint32_t paths[4]) {
  const unsigned j=mode==2 ? 1u : 0u;
  if(ticks) memcpy(ticks, s_taR28Ticks[j], sizeof(s_taR28Ticks[j]));
  if(samples) *samples = s_taR28Samples[j];
  if(paths) memcpy(paths, s_taR28ColorPath[j], sizeof(s_taR28ColorPath[j]));
}
#endif
/* R29: sampled BG tile-stage profiler. The hot path is instrumented only
 * once per 32 visible Mode-2 scanlines. Slots: tilemap/OPT, resolved row,
 * priority/window compositor. Branches never alter pixels or SNES state. */
#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
static bool s_taR29BGSample;
static uint64_t s_taR29BGTicks[2][3];
static uint64_t s_taR29BGCounts[2][4]; /* vector, scalar, transparent, culled */
void ppu_taR29BGRead(unsigned layer, uint64_t ticks[3], uint64_t counts[4]) {
  if(layer > 1) return;
  if(ticks) memcpy(ticks,s_taR29BGTicks[layer],sizeof s_taR29BGTicks[layer]);
  if(counts) memcpy(counts,s_taR29BGCounts[layer],sizeof s_taR29BGCounts[layer]);
}
#endif
void ppu_xboxPerfReset(void) {
#if defined(TA_R66_VICTIM_BITS) && TA_R66_VICTIM_BITS > 0 && defined(TA_R66_VICTIM_PROFILE) && TA_R66_VICTIM_PROFILE
  memset(s_taR66Probes, 0, sizeof s_taR66Probes);
  memset(s_taR66Hits, 0, sizeof s_taR66Hits);
  memset(s_taR66Fills, 0, sizeof s_taR66Fills);
#endif
#if defined(TA_R30_ROW_CACHE_PROFILE) && TA_R30_ROW_CACHE_PROFILE
  memset(s_taR30RowHits, 0, sizeof s_taR30RowHits);
  memset(s_taR30RowEpochMiss, 0, sizeof s_taR30RowEpochMiss);
  memset(s_taR30RowConflict, 0, sizeof s_taR30RowConflict);
#endif

#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
  memset(s_taR29BGTicks,0,sizeof s_taR29BGTicks);
  memset(s_taR29BGCounts,0,sizeof s_taR29BGCounts);
#endif
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
  memset(s_taR28Ticks, 0, sizeof(s_taR28Ticks));
  memset(s_taR28ColorPath, 0, sizeof(s_taR28ColorPath));
  memset(s_taR28Samples, 0, sizeof(s_taR28Samples));
  memset(s_taR28ColorConfig, 0, sizeof(s_taR28ColorConfig));
  s_taR28MaskedLines = 0;
#endif
#if defined(TA_R27_SPRITE_GROUP_SKIP) && TA_R27_SPRITE_GROUP_SKIP
  s_taR27SpriteEmptyGroups = s_taR27SpriteLiveGroups = 0;
#endif
#if defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
  s_taWindowBitsetLines = 0;
#if defined(TA_R18_WINDOW_CHUNK) && TA_R18_WINDOW_CHUNK
  s_taR18UniformTiles=s_taR18CulledTiles=s_taR18MixedTiles=0;
#endif
#endif
#if defined(TA_R12_OFFSET_CACHE) && TA_R12_OFFSET_CACHE
  s_taMode2OffsetHits = s_taMode2OffsetMisses = 0;
#endif
  s_xboxMode1FastLines = s_xboxGenericLines = 0;
  s_taMode2FastLines = s_taMode2RejectedLines = 0;
  s_xboxPackedTiles = s_xboxScalarTileChunks = 0;
  s_xboxTileCacheHits = s_xboxTileCacheMisses = 0;
  s_xboxMmxChunks = s_xboxFixedMathLines = s_xboxNarrowSpriteLines = 0;
  s_xboxSubMathLines = 0;
  s_xboxTransparentBgRows = 0;
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
  s_xboxSpriteCacheBuilds = 0;
  s_xboxSpriteCachedLines = 0;
  s_xboxSpriteMmxSlivers = 0;
  s_xboxSpriteScalarSlivers = 0;
#endif
}
uint64_t ppu_taMode2OffsetHits(void) {
#if defined(TA_R12_OFFSET_CACHE) && TA_R12_OFFSET_CACHE
  return s_taMode2OffsetHits;
#else
  return 0;
#endif
}
uint64_t ppu_taMode2OffsetMisses(void) {
#if defined(TA_R12_OFFSET_CACHE) && TA_R12_OFFSET_CACHE
  return s_taMode2OffsetMisses;
#else
  return 0;
#endif
}
uint64_t ppu_xboxPerfFastLines(void) { return s_xboxMode1FastLines; }
uint64_t ppu_taMode2FastLines(void) { return s_taMode2FastLines; }
uint64_t ppu_taMode2RejectedLines(void) { return s_taMode2RejectedLines; }
uint64_t ppu_xboxPerfGenericLines(void) { return s_xboxGenericLines; }
uint64_t ppu_xboxPerfPackedTiles(void) { return s_xboxPackedTiles; }
uint64_t ppu_xboxPerfScalarTileChunks(void) { return s_xboxScalarTileChunks; }
uint64_t ppu_xboxPerfTileCacheHits(void) { return s_xboxTileCacheHits; }
uint64_t ppu_xboxPerfTileCacheMisses(void) { return s_xboxTileCacheMisses; }
uint64_t ppu_xboxPerfMmxChunks(void) { return s_xboxMmxChunks; }
uint64_t ppu_xboxPerfFixedMathLines(void) { return s_xboxFixedMathLines; }
uint64_t ppu_xboxPerfSubMathLines(void) { return s_xboxSubMathLines; }
uint64_t ppu_xboxPerfNarrowSpriteLines(void) { return s_xboxNarrowSpriteLines; }
uint64_t ppu_xboxPerfTransparentBgRows(void) { return s_xboxTransparentBgRows; }
uint64_t ppu_xboxPerfSpriteCacheBuilds(void) {
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
  return s_xboxSpriteCacheBuilds;
#else
  return 0;
#endif
}
uint64_t ppu_xboxPerfSpriteCachedLines(void) {
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
  return s_xboxSpriteCachedLines;
#else
  return 0;
#endif
}
uint64_t ppu_xboxPerfSpriteMmxSlivers(void) {
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
  return s_xboxSpriteMmxSlivers;
#else
  return 0;
#endif
}
uint64_t ppu_xboxPerfSpriteScalarSlivers(void) {
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
  return s_xboxSpriteScalarSlivers;
#else
  return 0;
#endif
}

/*
 * R16: Mode-1 whole-line fast path, adapted from the strategy used by the
 * Super Mario World Xbox port's PPU renderer.  ZAMN spends most of its heavy
 * scenes in mode 1 (4bpp BG1/BG2 + 2bpp BG3).  Instead of asking the generic
 * compositor to rediscover the same tile data and priority ordering for every
 * candidate layer of every pixel, decode each background scanline once into
 * compact pixel/priority planes, then composite at most four candidates
 * (BG1/BG2/BG3/sprite) per screen pixel.
 *
 * The generic renderer remains the correctness fallback for hires and mosaic.
 */
/*
 * R17: take the SMW renderer one step further.  R16 decoded each background
 * into a pixel/prio plane and then walked all three planes again for every
 * output pixel (twice when the subscreen was needed).  SMW instead resolves
 * priority while it draws each tile.  These two compact Z buffers do the same:
 * high nibble = priority, next nibble = source layer, low byte = CGRAM index.
 */
/*
 * R23: split the packed 16-bit z-buffer into byte planes.  The old hot loop
 * loaded a 16-bit entry, shifted it to recover priority, compared it, then
 * repacked priority/layer/pixel for every visible BG pixel.  On the Xbox
 * Pentium III the three byte planes are cheaper scalar state and, more
 * importantly, let eight cached tile pixels be resolved with MMX at once.
 */
#if defined(TA_R88_PACKED_BG_COPY) && TA_R88_PACKED_BG_COPY
/* R88: every 256-byte plane retains its own 32-byte alignment. Keeping
 * planes adjacent permits two exact byte-range copies on Mode 2 cache hits
 * instead of 4-6 independent memcpy calls. No buffer aliases or pointers
 * survive a line: the renderer continues to address the same six planes. */
typedef struct {
  uint8_t mainPri[256], mainPix[256], mainLayer[256];
  uint8_t subPri[256], subPix[256], subLayer[256];
} TaR88BGPlanes;
static TaR88BGPlanes s_taR88BGPlanes __attribute__((aligned(32)));
#define s_mode1MainPri   (s_taR88BGPlanes.mainPri)
#define s_mode1MainPix   (s_taR88BGPlanes.mainPix)
#define s_mode1MainLayer (s_taR88BGPlanes.mainLayer)
#define s_mode1SubPri    (s_taR88BGPlanes.subPri)
#define s_mode1SubPix    (s_taR88BGPlanes.subPix)
#define s_mode1SubLayer  (s_taR88BGPlanes.subLayer)
/* A broken contiguity assumption must fail compilation, never rendering. */
typedef char TaR88MainPixOffset[(offsetof(TaR88BGPlanes,mainPix)==256)?1:-1];
typedef char TaR88MainLayerOffset[(offsetof(TaR88BGPlanes,mainLayer)==512)?1:-1];
typedef char TaR88SubPriOffset[(offsetof(TaR88BGPlanes,subPri)==768)?1:-1];
typedef char TaR88SubPixOffset[(offsetof(TaR88BGPlanes,subPix)==1024)?1:-1];
typedef char TaR88SubLayerOffset[(offsetof(TaR88BGPlanes,subLayer)==1280)?1:-1];
#else
static uint8_t s_mode1MainPri[256] __attribute__((aligned(32)));
static uint8_t s_mode1MainPix[256] __attribute__((aligned(32)));
static uint8_t s_mode1MainLayer[256] __attribute__((aligned(32)));
static uint8_t s_mode1SubPri[256] __attribute__((aligned(32)));
static uint8_t s_mode1SubPix[256] __attribute__((aligned(32)));
static uint8_t s_mode1SubLayer[256] __attribute__((aligned(32)));
#endif
#if defined(TA_R93_READONLY_SUB_BG) && TA_R93_READONLY_SUB_BG
#if !defined(TA_R88_PACKED_BG_COPY) || !TA_R88_PACKED_BG_COPY || \
    !defined(TA_R32_BG_LINE_CACHE) || !TA_R32_BG_LINE_CACHE
#error TA_R93_READONLY_SUB_BG requires TA_R88_PACKED_BG_COPY and TA_R32_BG_LINE_CACHE
#endif
/* Only output reads these views. All BG/sprite writes still target scratch.
 * A validated Mode-2 hit with subscreen OBJ disabled has no remaining writer
 * to its subscreen: BG drawing is skipped, then output reads the cached pixels.
 * Never alias mutable scratch to persistent BG storage. The synchronous line
 * renderer cannot step the guest or change OBJ enable between hit and output. */
static const uint8_t* s_mode1ReadSubPix = s_mode1SubPix;
static const uint8_t* s_mode1ReadSubLayer = s_mode1SubLayer;
static inline void ppu_taR93ResetSubView(void) {
  s_mode1ReadSubPix = s_mode1SubPix;
  s_mode1ReadSubLayer = s_mode1SubLayer;
}
#else
#define s_mode1ReadSubPix s_mode1SubPix
#define s_mode1ReadSubLayer s_mode1SubLayer
#define ppu_taR93ResetSubView() ((void)0)
#endif
#if defined(TA_R94_READONLY_MAIN_BG) && TA_R94_READONLY_MAIN_BG
#if !defined(TA_R93_READONLY_SUB_BG) || !TA_R93_READONLY_SUB_BG || \
    !defined(TA_R88_PACKED_BG_COPY) || !TA_R88_PACKED_BG_COPY
#error TA_R94_READONLY_MAIN_BG requires R93 subscreen views and R88 contiguous planes
#endif
/* Main background pixels are only modified after a cache hit by the OBJ
 * compositor. When main OBJ is disabled or every OBJ pixel is transparent,
 * sprite writes are impossible and output can read validated BG cache planes
 * directly. The mutable main scratch remains the only write destination.
 * Reset on every scanline and before every cache-restore attempt. */
static const uint8_t *s_taR94ReadMainPix=s_mode1MainPix;
static const uint8_t *s_taR94ReadMainLayer=s_mode1MainLayer;
static inline void ppu_taR94ResetMainView(void) {
  s_taR94ReadMainPix=s_mode1MainPix;
  s_taR94ReadMainLayer=s_mode1MainLayer;
}
static inline bool ppu_taR94MainObjInactive(const Ppu *ppu) {
  if(!ppu->layer[4].mainScreenEnabled) return true;
  /* bytewise scan matches objPixelBuffer semantics exactly; exiting at first
   * nonzero avoids the 256-byte scan on sprite-heavy lines. No alias casts. */
  const uint8_t *pixels=ppu->objPixelBuffer;
  for(unsigned i=0;i<256u;++i) if(pixels[i]) return false;
  return true;
}
#else
#define s_taR94ReadMainPix s_mode1MainPix
#define s_taR94ReadMainLayer s_mode1MainLayer
#define ppu_taR94ResetMainView() ((void)0)
#endif
#if defined(TA_R109_FUSED_MODE2_COMPOSE) && TA_R109_FUSED_MODE2_COMPOSE
#if !defined(TA_R94_READONLY_MAIN_BG) || !TA_R94_READONLY_MAIN_BG || !defined(TA_R50_PACKED_COLOR) || !TA_R50_PACKED_COLOR
#error R109 requires R94 background views and R50 exact packed color math
#endif
static bool s_taR109Fuse;
static const uint8_t *s_taR109MainPri=s_mode1MainPri,*s_taR109SubPri=s_mode1SubPri;
#if defined(TA_R109_COMPOSE_PROFILE) && TA_R109_COMPOSE_PROFILE
static uint64_t s_taR109Counts[6];
void ppu_taR109Counts(uint64_t out[6]) {memcpy(out,s_taR109Counts,sizeof s_taR109Counts);}
#define TA_R109_COUNT(I) (++s_taR109Counts[I])
#else
#define TA_R109_COUNT(I) ((void)0)
#endif
#endif
static uint32_t s_mode1ColorXrgb[256];
#if defined(TA_R99_PALETTE_LANES) && TA_R99_PALETTE_LANES
static uint32_t s_taR99Color5[256];
#endif
static uint32_t s_mode1MathXrgb[256];
#if defined(TA_R26_COMPACT_SUBMATH) && TA_R26_COMPACT_SUBMATH
/* 32 x 32 five-bit component pair table: 1 KiB instead of 256 KiB. */
static uint8_t s_taR26MathChannel[32 * 32] __attribute__((aligned(32)));
#else
static uint32_t s_mode1SubMathXrgb[256 * 256] __attribute__((aligned(64)));
#endif
static uint32_t s_mode1SubFixedXrgb[256];
static bool s_mode1PaletteValid = false;
static uint8_t s_mode1PaletteBrightness = 0xff;
static bool s_mode1MathPaletteValid = false;
static uint32_t s_mode1MathKey = 0;
static bool s_mode1SubMathValid = false;
#if defined(TA_R32_COLOR_PAIR_CACHE) && TA_R32_COLOR_PAIR_CACHE
/* Demand-built 8 KiB pair cache; no 65,536-color rebuild after CGRAM writes.
 * Epoch changes whenever the original R26 channel table is rebuilt. */
typedef struct {uint32_t tag,color;} TaR32ColorPair;
static TaR32ColorPair s_taR32ColorPairs[1024] __attribute__((aligned(32)));
static uint16_t s_taR32ColorEpoch=1;
#endif
static uint32_t s_mode1SubMathKey = 0;

static inline void ppu_xboxMode1Put(uint8_t* pri, uint8_t* pix, uint8_t* lay,
                                    int x, int z, int layer, int pixel,
                                    bool trackLayer) {
  if(pixel && z > pri[x]) {
    pri[x] = (uint8_t)z;
    pix[x] = (uint8_t)pixel;
    if(trackLayer) lay[x] = (uint8_t)layer;
  }
}

#if defined(ZAMN_R23_MMX_PPU)
/*
 * RXDK's Clang 23 headers annotate several legacy _mm_* MMX intrinsics as
 * requiring SSE2.  The OG Xbox Pentium III has MMX/SSE1, not SSE2, so use
 * the real MMX instruction set directly instead of lying to the compiler
 * with -msse2.
 */
static inline void ppu_xboxMode1Put8(uint8_t* pri, uint8_t* pix, uint8_t* lay,
                                     const uint8_t* src,
#if defined(ZAMN_R32_PPU_HOISTS)
                                     const uint64_t* zQ,
                                     const uint64_t* layerQ,
#else
                                     int z, int layer,
#endif
                                     bool trackLayer) {
#if !defined(ZAMN_R32_PPU_HOISTS)
  const uint32_t zByte = (uint8_t)z;
  const uint32_t layerByte = (uint8_t)layer;
#endif

  if(trackLayer) {
    __asm__ volatile(
      /* mm0 = source pixels. R31 checks transparency from this same load,
         instead of R30's extra 64-bit C pre-load before entering MMX. */
      "movq (%[src]), %%mm0\n\t"
      "pxor %%mm7, %%mm7\n\t"
      "movq %%mm0, %%mm3\n\t"
      "pcmpeqb %%mm7, %%mm3\n\t"
#if defined(ZAMN_R31_MMX_TRANSPARENT_EARLYOUT)
      /* Pentium III SSE1/MMX: if all 8 source bytes are zero, nothing in
         priority/pixel/layer state changes. pmovmskb avoids a second load. */
      "pmovmskb %%mm3, %%eax\n\t"
      "cmp $255, %%eax\n\t"
      "je 9f\n\t"
#endif
      "movq (%[pri]), %%mm1\n\t"

      /* mm4 = z replicated to all 8 bytes */
#if defined(ZAMN_R32_PPU_HOISTS)
      "movq (%[zq]), %%mm4\n\t"
#else
      "movd %[z], %%mm4\n\t"
      "punpcklbw %%mm4, %%mm4\n\t"
      "punpcklwd %%mm4, %%mm4\n\t"
      "punpckldq %%mm4, %%mm4\n\t"
#endif

      /* mm2 = (z > old_priority) byte mask */
      "movq %%mm4, %%mm2\n\t"
      "pcmpgtb %%mm1, %%mm2\n\t"

      /* mm3 already holds src_is_zero from above. */
      "pandn %%mm2, %%mm3\n\t"       /* mm3 = (~src_is_zero) & mask */

      /* pri = old ^ ((old ^ z) & mask) */
      "movq %%mm4, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[pri])\n\t"

      /* pix = old ^ ((old ^ src) & mask) */
      "movq (%[pix]), %%mm1\n\t"
      "movq %%mm0, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[pix])\n\t"

      /* mm4 = layer replicated to all 8 bytes */
#if defined(ZAMN_R32_PPU_HOISTS)
      "movq (%[layerq]), %%mm4\n\t"
#else
      "movd %[layer], %%mm4\n\t"
      "punpcklbw %%mm4, %%mm4\n\t"
      "punpcklwd %%mm4, %%mm4\n\t"
      "punpckldq %%mm4, %%mm4\n\t"
#endif

      /* lay = old ^ ((old ^ layer) & mask) */
      "movq (%[lay]), %%mm1\n\t"
      "movq %%mm4, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[lay])\n\t"
#if defined(ZAMN_R31_MMX_TRANSPARENT_EARLYOUT)
      "9:\n\t"
#endif
      :
#if defined(ZAMN_R32_PPU_HOISTS)
      : [pri] "r" (pri), [pix] "r" (pix), [lay] "r" (lay),
        [src] "r" (src), [zq] "r" (zQ), [layerq] "r" (layerQ)
#else
      : [pri] "r" (pri), [pix] "r" (pix), [lay] "r" (lay),
        [src] "r" (src), [z] "m" (zByte), [layer] "m" (layerByte)
#endif
      : "eax", "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm7", "memory"
    );
  } else {
    __asm__ volatile(
      "movq (%[src]), %%mm0\n\t"
      "pxor %%mm7, %%mm7\n\t"
      "movq %%mm0, %%mm3\n\t"
      "pcmpeqb %%mm7, %%mm3\n\t"
#if defined(ZAMN_R31_MMX_TRANSPARENT_EARLYOUT)
      "pmovmskb %%mm3, %%eax\n\t"
      "cmp $255, %%eax\n\t"
      "je 9f\n\t"
#endif
      "movq (%[pri]), %%mm1\n\t"
#if defined(ZAMN_R32_PPU_HOISTS)
      "movq (%[zq]), %%mm4\n\t"
#else
      "movd %[z], %%mm4\n\t"
      "punpcklbw %%mm4, %%mm4\n\t"
      "punpcklwd %%mm4, %%mm4\n\t"
      "punpckldq %%mm4, %%mm4\n\t"
#endif
      "movq %%mm4, %%mm2\n\t"
      "pcmpgtb %%mm1, %%mm2\n\t"
      "pandn %%mm2, %%mm3\n\t"

      "movq %%mm4, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[pri])\n\t"

      "movq (%[pix]), %%mm1\n\t"
      "movq %%mm0, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[pix])\n\t"
#if defined(ZAMN_R31_MMX_TRANSPARENT_EARLYOUT)
      "9:\n\t"
#endif
      :
#if defined(ZAMN_R32_PPU_HOISTS)
      : [pri] "r" (pri), [pix] "r" (pix), [src] "r" (src), [zq] "r" (zQ)
#else
      : [pri] "r" (pri), [pix] "r" (pix), [src] "r" (src), [z] "m" (zByte)
#endif
      : "eax", "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm7", "memory"
    );
  }
}
#if defined(TA_R69_MMX_NO_WIN_SKIP) && TA_R69_MMX_NO_WIN_SKIP
/* R69 native MMX: mask is computed by the existing exact pixel+priority
 * logic. Only Mode 1's drawing loop invokes this alternate compositor.
 * If all eight mask bytes are zero, none of priority/pixel/layer can change.
 * Skip the original store sequence. Transparent-row earlyout remains intact.
 * This version is intentionally separately compiled; Mode 2 retains R68's
 * original compositor with no extra branch or mask test. */
static inline void ppu_xboxMode1Put8R69(uint8_t* pri, uint8_t* pix, uint8_t* lay,
                                     const uint8_t* src,
#if defined(ZAMN_R32_PPU_HOISTS)
                                     const uint64_t* zQ,
                                     const uint64_t* layerQ,
#else
                                     int z, int layer,
#endif
                                     bool trackLayer) {
#if !defined(ZAMN_R32_PPU_HOISTS)
  const uint32_t zByte = (uint8_t)z;
  const uint32_t layerByte = (uint8_t)layer;
#endif

  if(trackLayer) {
    __asm__ volatile(
      /* mm0 = source pixels. R31 checks transparency from this same load,
         instead of R30's extra 64-bit C pre-load before entering MMX. */
      "movq (%[src]), %%mm0\n\t"
      "pxor %%mm7, %%mm7\n\t"
      "movq %%mm0, %%mm3\n\t"
      "pcmpeqb %%mm7, %%mm3\n\t"
#if defined(ZAMN_R31_MMX_TRANSPARENT_EARLYOUT)
      /* Pentium III SSE1/MMX: if all 8 source bytes are zero, nothing in
         priority/pixel/layer state changes. pmovmskb avoids a second load. */
      "pmovmskb %%mm3, %%eax\n\t"
      "cmp $255, %%eax\n\t"
      "je 9f\n\t"
#endif
      "movq (%[pri]), %%mm1\n\t"

      /* mm4 = z replicated to all 8 bytes */
#if defined(ZAMN_R32_PPU_HOISTS)
      "movq (%[zq]), %%mm4\n\t"
#else
      "movd %[z], %%mm4\n\t"
      "punpcklbw %%mm4, %%mm4\n\t"
      "punpcklwd %%mm4, %%mm4\n\t"
      "punpckldq %%mm4, %%mm4\n\t"
#endif

      /* mm2 = (z > old_priority) byte mask */
      "movq %%mm4, %%mm2\n\t"
      "pcmpgtb %%mm1, %%mm2\n\t"

      /* mm3 already holds src_is_zero from above. */
      "pandn %%mm2, %%mm3\n\t"
#if defined(ZAMN_R31_MMX_TRANSPARENT_EARLYOUT)
      /* No source lane can beat priority: preserve all 24 output bytes. */
      "pmovmskb %%mm3, %%eax\n\t"
      "testl %%eax, %%eax\n\t"
      "je 9f\n\t"
#endif       /* mm3 = (~src_is_zero) & mask */

      /* pri = old ^ ((old ^ z) & mask) */
      "movq %%mm4, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[pri])\n\t"

      /* pix = old ^ ((old ^ src) & mask) */
      "movq (%[pix]), %%mm1\n\t"
      "movq %%mm0, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[pix])\n\t"

      /* mm4 = layer replicated to all 8 bytes */
#if defined(ZAMN_R32_PPU_HOISTS)
      "movq (%[layerq]), %%mm4\n\t"
#else
      "movd %[layer], %%mm4\n\t"
      "punpcklbw %%mm4, %%mm4\n\t"
      "punpcklwd %%mm4, %%mm4\n\t"
      "punpckldq %%mm4, %%mm4\n\t"
#endif

      /* lay = old ^ ((old ^ layer) & mask) */
      "movq (%[lay]), %%mm1\n\t"
      "movq %%mm4, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[lay])\n\t"
#if defined(ZAMN_R31_MMX_TRANSPARENT_EARLYOUT)
      "9:\n\t"
#endif
      :
#if defined(ZAMN_R32_PPU_HOISTS)
      : [pri] "r" (pri), [pix] "r" (pix), [lay] "r" (lay),
        [src] "r" (src), [zq] "r" (zQ), [layerq] "r" (layerQ)
#else
      : [pri] "r" (pri), [pix] "r" (pix), [lay] "r" (lay),
        [src] "r" (src), [z] "m" (zByte), [layer] "m" (layerByte)
#endif
      : "eax", "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm7", "memory"
    );
  } else {
    __asm__ volatile(
      "movq (%[src]), %%mm0\n\t"
      "pxor %%mm7, %%mm7\n\t"
      "movq %%mm0, %%mm3\n\t"
      "pcmpeqb %%mm7, %%mm3\n\t"
#if defined(ZAMN_R31_MMX_TRANSPARENT_EARLYOUT)
      "pmovmskb %%mm3, %%eax\n\t"
      "cmp $255, %%eax\n\t"
      "je 9f\n\t"
#endif
      "movq (%[pri]), %%mm1\n\t"
#if defined(ZAMN_R32_PPU_HOISTS)
      "movq (%[zq]), %%mm4\n\t"
#else
      "movd %[z], %%mm4\n\t"
      "punpcklbw %%mm4, %%mm4\n\t"
      "punpcklwd %%mm4, %%mm4\n\t"
      "punpckldq %%mm4, %%mm4\n\t"
#endif
      "movq %%mm4, %%mm2\n\t"
      "pcmpgtb %%mm1, %%mm2\n\t"
      "pandn %%mm2, %%mm3\n\t"
#if defined(ZAMN_R31_MMX_TRANSPARENT_EARLYOUT)
      /* No source lane can beat priority: preserve all 24 output bytes. */
      "pmovmskb %%mm3, %%eax\n\t"
      "testl %%eax, %%eax\n\t"
      "je 9f\n\t"
#endif

      "movq %%mm4, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[pri])\n\t"

      "movq (%[pix]), %%mm1\n\t"
      "movq %%mm0, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[pix])\n\t"
#if defined(ZAMN_R31_MMX_TRANSPARENT_EARLYOUT)
      "9:\n\t"
#endif
      :
#if defined(ZAMN_R32_PPU_HOISTS)
      : [pri] "r" (pri), [pix] "r" (pix), [src] "r" (src), [zq] "r" (zQ)
#else
      : [pri] "r" (pri), [pix] "r" (pix), [src] "r" (src), [z] "m" (zByte)
#endif
      : "eax", "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm7", "memory"
    );
  }
}
#else
#define ppu_xboxMode1Put8R69 ppu_xboxMode1Put8
#endif

#endif

static inline void ppu_xboxInvalidatePaletteCache(void) {
  s_mode1PaletteValid = false;
  s_mode1MathPaletteValid = false;
  s_mode1SubMathValid = false;
}

static inline void ppu_xboxBuildPalette(Ppu* ppu) {
  static uint32_t optionRevision;
  if(optionRevision!=taPanelRevision){optionRevision=taPanelRevision;ppu_xboxInvalidatePaletteCache();}
  const uint8_t br = ppu->brightness & 0x0f;
  if(__builtin_expect(s_mode1PaletteValid && s_mode1PaletteBrightness == br, 1)) return;
#if defined(TA_R43_LINE_OUTPUT_CACHE) && TA_R43_LINE_OUTPUT_CACHE && defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
  ppu_taR43ObservePalette(ppu);
#endif
  const uint8_t* bright = s_brightnessLut[br];
  for(int i = 0; i < 256; ++i) {
    const uint16_t c = ta_panel_color(ppu->cgram,i,ppu->mode);
    const int r = c & 0x1f;
    const int g = (c >> 5) & 0x1f;
    const int b = (c >> 10) & 0x1f;
#if defined(TA_R99_PALETTE_LANES) && TA_R99_PALETTE_LANES
    s_taR99Color5[i]=(uint32_t)b | ((uint32_t)g<<8) | ((uint32_t)r<<16);
#endif
    s_mode1ColorXrgb[i] = (uint32_t)bright[b] |
                          ((uint32_t)bright[g] << 8) |
                          ((uint32_t)bright[r] << 16);
  }
  s_mode1PaletteBrightness = br;
  s_mode1PaletteValid = true;
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
  if(++s_r127PaletteSerial==0)s_r127PaletteSerial=1;
#endif
  s_mode1MathPaletteValid = false;
}

/* Fixed-colour maths is a palette transform when the sub screen is not part
 * of the equation.  ZAMN uses this heavily in gameplay.  Build all 256
 * results once when the CGRAM/brightness/fixed-colour state changes instead
 * of doing six channel operations, clamp and brightness work per pixel. */
static inline void ppu_xboxBuildMathPalette(Ppu* ppu) {
  const uint32_t key =
      ((uint32_t)(ppu->brightness & 0x0f)) |
      ((uint32_t)(ppu->fixedColorR & 0x1f) << 4) |
      ((uint32_t)(ppu->fixedColorG & 0x1f) << 9) |
      ((uint32_t)(ppu->fixedColorB & 0x1f) << 14) |
      ((uint32_t)(ppu->subtractColor ? 1u : 0u) << 19) |
      ((uint32_t)(ppu->halfColor ? 1u : 0u) << 20);
  if(__builtin_expect(s_mode1MathPaletteValid && s_mode1MathKey == key, 1)) return;
  const uint8_t* bright = s_brightnessLut[ppu->brightness & 0x0f];
  for(int i = 0; i < 256; ++i) {
    const uint16_t c = ta_panel_color(ppu->cgram,i,ppu->mode);
    int r = c & 0x1f, g = (c >> 5) & 0x1f, b = (c >> 10) & 0x1f;
    if(ppu->subtractColor) {
      r -= ppu->fixedColorR; g -= ppu->fixedColorG; b -= ppu->fixedColorB;
    } else {
      r += ppu->fixedColorR; g += ppu->fixedColorG; b += ppu->fixedColorB;
    }
    if(ppu->halfColor) { r >>= 1; g >>= 1; b >>= 1; }
    r = ppu_clampColorFast(r); g = ppu_clampColorFast(g); b = ppu_clampColorFast(b);
    s_mode1MathXrgb[i] = (uint32_t)bright[b] |
                         ((uint32_t)bright[g] << 8) |
                         ((uint32_t)bright[r] << 16);
  }
  s_mode1MathKey = key;
  s_mode1MathPaletteValid = true;
}

/*
 * R24: ZAMN's heavy gameplay uses add-subscreen colour math rather than the
 * fixed-colour-only path R23 accelerated.  Precompute the exact main/sub
 * result for every CGRAM pair.  256 KiB is cheap on the Xbox and replaces
 * three channel extracts, add/subtract, half/clamp and brightness work for
 * every math-enabled output pixel in the hot gameplay path.
 *
 * When the subscreen winner is backdrop, the SNES uses the fixed colour and
 * (with addSubscreen set) does NOT apply the half step.  Keep that as its own
 * 256-entry table so the fast path stays bit-identical to the scalar code.
 */
static inline void ppu_xboxBuildSubMathTables(Ppu* ppu) {
  const uint32_t key =
      ((uint32_t)(ppu->brightness & 0x0f)) |
      ((uint32_t)(ppu->fixedColorR & 0x1f) << 4) |
      ((uint32_t)(ppu->fixedColorG & 0x1f) << 9) |
      ((uint32_t)(ppu->fixedColorB & 0x1f) << 14) |
      ((uint32_t)(ppu->subtractColor ? 1u : 0u) << 19) |
      ((uint32_t)(ppu->halfColor ? 1u : 0u) << 20);
  if(__builtin_expect(s_mode1SubMathValid && s_mode1SubMathKey == key, 1)) return;
#if defined(TA_R32_COLOR_PAIR_CACHE) && TA_R32_COLOR_PAIR_CACHE
  if(++s_taR32ColorEpoch==0) {
    memset(s_taR32ColorPairs,0,sizeof s_taR32ColorPairs);
    s_taR32ColorEpoch=1;
  }
#endif

  const uint8_t* bright = s_brightnessLut[ppu->brightness & 0x0f];
#if defined(TA_R26_COMPACT_SUBMATH) && TA_R26_COMPACT_SUBMATH
  for(int a = 0; a < 32; ++a) {
    for(int b = 0; b < 32; ++b) {
      int v = ppu->subtractColor ? a - b : a + b;
      if(ppu->halfColor) v >>= 1;
      s_taR26MathChannel[(a << 5) | b] = bright[ppu_clampColorFast(v)];
    }
  }
#endif
  for(int m = 0; m < 256; ++m) {
    const uint16_t cm = ta_panel_color(ppu->cgram,m,ppu->mode);
    const int mr = cm & 0x1f;
    const int mg = (cm >> 5) & 0x1f;
    const int mb = (cm >> 10) & 0x1f;

    /* Backdrop on the subscreen means fixed colour and no halving. */
    int fr = ppu->subtractColor ? mr - ppu->fixedColorR : mr + ppu->fixedColorR;
    int fg = ppu->subtractColor ? mg - ppu->fixedColorG : mg + ppu->fixedColorG;
    int fb = ppu->subtractColor ? mb - ppu->fixedColorB : mb + ppu->fixedColorB;
    fr = ppu_clampColorFast(fr);
    fg = ppu_clampColorFast(fg);
    fb = ppu_clampColorFast(fb);
    s_mode1SubFixedXrgb[m] = (uint32_t)bright[fb] |
        ((uint32_t)bright[fg] << 8) | ((uint32_t)bright[fr] << 16);

#if !defined(TA_R26_COMPACT_SUBMATH) || !TA_R26_COMPACT_SUBMATH
    uint32_t* row = &s_mode1SubMathXrgb[m << 8];
    for(int sub = 0; sub < 256; ++sub) {
      const uint16_t cs = ta_panel_color(ppu->cgram,sub,ppu->mode);
      int r = ppu->subtractColor ? mr - (cs & 0x1f) : mr + (cs & 0x1f);
      int g = ppu->subtractColor ? mg - ((cs >> 5) & 0x1f) : mg + ((cs >> 5) & 0x1f);
      int b = ppu->subtractColor ? mb - ((cs >> 10) & 0x1f) : mb + ((cs >> 10) & 0x1f);
      if(ppu->halfColor) { r >>= 1; g >>= 1; b >>= 1; }
      r = ppu_clampColorFast(r);
      g = ppu_clampColorFast(g);
      b = ppu_clampColorFast(b);
      row[sub] = (uint32_t)bright[b] | ((uint32_t)bright[g] << 8) |
                 ((uint32_t)bright[r] << 16);
    }
#endif
  }
  s_mode1SubMathKey = key;
  s_mode1SubMathValid = true;
}

#if defined(TA_R26_COMPACT_SUBMATH) && TA_R26_COMPACT_SUBMATH
static inline uint32_t ppu_taR26SubMathColor(const Ppu* ppu, int mainPixel, int subPixel) {
#if defined(TA_R32_COLOR_PAIR_CACHE) && TA_R32_COLOR_PAIR_CACHE
  const uint32_t pair=((uint32_t)mainPixel<<8)|(uint32_t)subPixel;
  const uint32_t tag=((uint32_t)s_taR32ColorEpoch<<16)|pair;
  TaR32ColorPair* e=&s_taR32ColorPairs[(pair*0x9e3779b1u)>>22];
  if(e->tag==tag)return e->color;
#endif
  const uint16_t a = ta_panel_color(ppu->cgram,mainPixel,ppu->mode);
  const uint16_t b = ta_panel_color(ppu->cgram,subPixel,ppu->mode);
  const uint8_t* const ch = s_taR26MathChannel;
  const uint32_t color = (uint32_t)ch[((a >> 10 & 31) << 5) | (b >> 10 & 31)] |
         ((uint32_t)ch[((a >> 5 & 31) << 5) | (b >> 5 & 31)] << 8) |
         ((uint32_t)ch[((a & 31) << 5) | (b & 31)] << 16);
#if defined(TA_R32_COLOR_PAIR_CACHE) && TA_R32_COLOR_PAIR_CACHE
  e->tag=tag;e->color=color;
#endif
  return color;
}
#endif

static inline uint32_t ppu_xboxMode1DirectColor(int pixel) {
  return s_mode1ColorXrgb[(uint8_t)pixel];
}

#include "ppu_ta_r107_opt.h"

/* TA R12: line-local Mode 2 BG3 offset-per-tile cache.  Each BG1/BG2
 * layer requests the same BG3 offset words by screen column, and this cache
 * shares that decoding within a single fully composed scanline. Never reuse
 * values across scanlines, because games may update VRAM/scroll via HDMA. */
#if defined(TA_R12_OFFSET_CACHE) && TA_R12_OFFSET_CACHE
static inline void ppu_taOffsetPair(Ppu* ppu, int col,
                                    uint16_t *h, uint16_t *v) {
  /* col is the BG3 offset column (screen column minus one). */
  if((unsigned)col >= 32u) {
    *h = ppu_getOffsetValue(ppu, col, 0);
    *v = ppu_getOffsetValue(ppu, col, 1);
    return;
  }
  const uint32_t bit = 1u << (unsigned)col;
  if(s_taMode2OffsetMask & bit) {
    TA_DIAG(++s_taMode2OffsetHits);
  } else {
#if defined(TA_R107_MODE2_OPT_TABLE) && TA_R107_MODE2_OPT_TABLE
    if(ppu->mode == 2) ppu_taR107PrepareOffsets(ppu);
    else {
      s_taR107OptValid=false;
#endif
    s_taMode2HOffset[col] = ppu_getOffsetValue(ppu, col, 0);
    s_taMode2VOffset[col] = ppu_getOffsetValue(ppu, col, 1);
    s_taMode2OffsetMask |= bit;
#if defined(TA_R107_MODE2_OPT_TABLE) && TA_R107_MODE2_OPT_TABLE
    }
#endif
    TA_DIAG(++s_taMode2OffsetMisses);
  }
  *h = s_taMode2HOffset[col];
  *v = s_taMode2VOffset[col];
}
#endif

/* R67: compile Mode 2 BG3 offset words into a line-local screen-column table.
 * Important: no scanline work is introduced on an R32 full-BG cache hit.
 * Both horizontal and vertical validity bits are preserved independently
 * for BG1 (bit 13) and BG2 (bit 14). Rebuild before each new line, never
 * across a scanline or VRAM/HDMA boundary. Column 0 has no override. */
#if defined(TA_R67_BG_FAST) && TA_R67_BG_FAST && defined(TA_R12_OFFSET_CACHE) && TA_R12_OFFSET_CACHE
static uint16_t s_taR67Horizontal[2][33];
static uint16_t s_taR67Vertical[2][33];
static uint8_t s_taR67Flags[2][33];
static bool s_taR67OffsetsReady;
static inline void ppu_taR67BuildOffsets(Ppu* ppu) {
  if(s_taR67OffsetsReady) return;
  for(unsigned col=1; col<=32; ++col) {
    uint16_t hOff, vOff;
    ppu_taOffsetPair(ppu, (int)col-1, &hOff, &vOff);
    for(unsigned bg=0; bg<2; ++bg) {
      const uint16_t valid=(uint16_t)(bg==0 ? 0x2000u : 0x4000u);
      const uint8_t flags=(uint8_t)(((hOff&valid) ? 1u : 0u) |
                                    ((vOff&valid) ? 2u : 0u));
      s_taR67Flags[bg][col]=flags;
      s_taR67Horizontal[bg][col]=(uint16_t)(((hOff&0x3f8u)+col*8u)&0x3ffu);
      s_taR67Vertical[bg][col]=(uint16_t)(vOff&0x3ffu);
    }
  }
  s_taR67OffsetsReady=true;
}
#endif

static void ppu_xboxMode1DrawLayer(Ppu* ppu, int layer, int y,
                                   const uint8_t zbg[3][2], bool needSub,
                                   bool trackLayer) {
  TA_R82_SCOPE(TA82_BG);
  const Layer* ls = &ppu->layer[layer];
  const bool mainEnabled = ls->mainScreenEnabled;
  const bool subEnabled = needSub && ls->subScreenEnabled;
  if(!mainEnabled && !subEnabled) return;

  const bool mainWindowed = mainEnabled && ls->mainScreenWindowed;
  const bool subWindowed = subEnabled && ls->subScreenWindowed;
  const bool anyWindowed = mainWindowed || subWindowed;
  const int bitDepth = (layer == 2) ? 2 : 4;
  const bool big = ppu->bgLayer[layer].bigTiles;
  const int tileBitsX = big ? 4 : 3;
  const int tileHighBitX = big ? 0x200 : 0x100;
  const int tileBitsY = big ? 4 : 3;
  const int tileHighBitY = big ? 0x200 : 0x100;
  const int originalLy = (y + ppu->bgLayer[layer].vScroll) & 0x3ff;
#if defined(TA_R67_BG_FAST) && TA_R67_BG_FAST
  /* The BG3 per-column Y offset is optional: when absent, reuse these
   * exact tilemap/flip/row-hash terms throughout the 256px scanline. */
  const unsigned r67BaseY=(unsigned)originalLy;
  const unsigned r67MapY=(r67BaseY >> tileBitsY) & 31u;
  const uint16_t r67TileMapBase=(uint16_t)(ppu->bgLayer[layer].tilemapAdr +
      (r67MapY<<5) + ((r67BaseY & (unsigned)tileHighBitY) &&
       ppu->bgLayer[layer].tilemapHigher ?
       (ppu->bgLayer[layer].tilemapWider ? 0x800u : 0x400u) : 0u));
  const uint16_t r67NormalRow=(uint16_t)(r67BaseY & 7u);
  const uint16_t r67FlipRow=(uint16_t)(7u-r67NormalRow);
  const uint16_t r67NormalKey=(uint16_t)(r67NormalRow | ((unsigned)(layer&3)<<3));
  const uint16_t r67FlipKey=(uint16_t)(r67FlipRow | ((unsigned)(layer&3)<<3));
  const uint32_t r67NormalSalt=(uint32_t)r67NormalKey*0x9e3779b1u;
  const uint32_t r67FlipSalt=(uint32_t)r67FlipKey*0x9e3779b1u;
#if defined(TA_R12_OFFSET_CACHE) && TA_R12_OFFSET_CACHE
  if(ppu->mode==2 && layer<2) ppu_taR67BuildOffsets(ppu);
#endif
#endif
#if defined(ZAMN_R32_PPU_HOISTS)
  const int ly = originalLy;
  /* R32: everything below is invariant for this layer/scanline.  R31 still
     rebuilt these values for every 8-pixel tile chunk. */
  const int lyRow = ly & 7;
  const int lyRowFlip = 7 - lyRow;
  const bool bigYHalf = big && ((ly & 8) != 0);
  uint16_t tilemapRowBase = (uint16_t)(ppu->bgLayer[layer].tilemapAdr +
      (((ly >> tileBitsY) & 0x1f) << 5));
  if((ly & tileHighBitY) && ppu->bgLayer[layer].tilemapHigher)
    tilemapRowBase = (uint16_t)(tilemapRowBase +
        (ppu->bgLayer[layer].tilemapWider ? 0x800 : 0x400));

  const uint16_t rowKeyNormal =
      (uint16_t)((lyRow & 7) | ((layer & 3) << 3));
  const uint16_t rowKeyFlip =
      (uint16_t)((lyRowFlip & 7) | ((layer & 3) << 3));
  const uint32_t rowHashNormal =
      (uint32_t)rowKeyNormal * 0x9e3779b1u;
  const uint32_t rowHashFlip =
      (uint32_t)rowKeyFlip * 0x9e3779b1u;
  const uint64_t* const layerQ = &s_xboxByteBroadcast[layer & 15];
#endif
  int sx = 0;
#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
  const bool r29BGProbe = s_taR29BGSample && layer < 2;
#define TA_R29_BG_FINISH(KIND) do { \
  if(r29BGProbe) { \
    s_taR29BGTicks[layer][2] += __builtin_readcyclecounter()-r29BGRowTime; \
    ++s_taR29BGCounts[layer][(KIND)]; \
  } \
} while(0)
#else
#define TA_R29_BG_FINISH(KIND) ((void)0)
#endif
  while(sx < 256) {
#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
    const uint64_t r29BGStart = r29BGProbe ? __builtin_readcyclecounter() : 0;
#endif
    const int lxRaw = sx + ppu->bgLayer[layer].hScroll;
    int bx = (lxRaw & ~7) & 0x3ff;
#if !defined(ZAMN_R32_PPU_HOISTS)
    int ly = originalLy;
#endif
    /* SNES Mode 2 offset-per-tile: BG3 provides per-eight-screen-pixel
       X and Y overrides. The source x/y must be resolved BEFORE fetching
       the tilemap row. As in ppu_handleOPT, the first column cannot offset.
       This preserves both BG-specific validity bits and the unmodified
       original pixel's low 3 bits. Only one map lookup per chunk, instead
       of per layer/priority/pixel in the generic reference path. */
#if defined(TA_R67_BG_FAST) && TA_R67_BG_FAST && defined(TA_R12_OFFSET_CACHE) && TA_R12_OFFSET_CACHE && !defined(ZAMN_R32_PPU_HOISTS)
    if(ppu->mode == 2 && layer < 2) {
      /* Original column formula simplifies exactly to (sx + scroll%8)/8.
       * Every valid screen column is 1..32, and column zero is unchanged. */
      const unsigned column=((unsigned)sx + ((unsigned)ppu->bgLayer[layer].hScroll & 7u)) >> 3;
      if(column) {
        const unsigned flags=s_taR67Flags[layer][column];
        if(flags&1u) bx=s_taR67Horizontal[layer][column];
        if(flags&2u) ly=((int)s_taR67Vertical[layer][column]+y)&0x3ff;
      }
    }
#else
    if(ppu->mode == 2) {
      const int column = ((lxRaw & ~7) -
          (ppu->bgLayer[layer].hScroll & ~7)) >> 3;
      if(column > 0) {
        const uint16_t valid = layer == 0 ? 0x2000u : 0x4000u;
        uint16_t hOff, vOff;
#if defined(TA_R12_OFFSET_CACHE) && TA_R12_OFFSET_CACHE
        ppu_taOffsetPair(ppu, column - 1, &hOff, &vOff);
#else
        hOff = ppu_getOffsetValue(ppu, column - 1, 0);
        vOff = ppu_getOffsetValue(ppu, column - 1, 1);
#endif
        if(hOff & valid)
          bx = ((hOff & 0x3f8) + column * 8) & 0x3ff;
#if !defined(ZAMN_R32_PPU_HOISTS)
        if(vOff & valid)
          ly = ((vOff & 0x3ff) + y) & 0x3ff;
#endif
      }
    }
#endif
#if defined(ZAMN_R32_PPU_HOISTS)
    uint16_t tilemapAdr =
        (uint16_t)(tilemapRowBase + ((bx >> tileBitsX) & 0x1f));
    if((bx & tileHighBitX) && ppu->bgLayer[layer].tilemapWider)
      tilemapAdr = (uint16_t)(tilemapAdr + 0x400);
#else
#if defined(TA_R67_BG_FAST) && TA_R67_BG_FAST
    uint16_t tilemapAdr;
    if(__builtin_expect(ly == originalLy, 1)) {
      tilemapAdr=(uint16_t)(r67TileMapBase + ((bx >> tileBitsX)&31));
      if((bx & tileHighBitX) && ppu->bgLayer[layer].tilemapWider)
        tilemapAdr=(uint16_t)(tilemapAdr+0x400);
    } else {
      tilemapAdr=(uint16_t)(ppu->bgLayer[layer].tilemapAdr +
          (((ly >> tileBitsY)&31)<<5 | ((bx >> tileBitsX)&31)));
      if((bx & tileHighBitX) && ppu->bgLayer[layer].tilemapWider) tilemapAdr+=0x400;
      if((ly & tileHighBitY) && ppu->bgLayer[layer].tilemapHigher)
        tilemapAdr+=ppu->bgLayer[layer].tilemapWider ? 0x800 : 0x400;
    }
#else
    uint16_t tilemapAdr = ppu->bgLayer[layer].tilemapAdr +
      (((ly >> tileBitsY) & 0x1f) << 5 | ((bx >> tileBitsX) & 0x1f));
    if((bx & tileHighBitX) && ppu->bgLayer[layer].tilemapWider) tilemapAdr += 0x400;
    if((ly & tileHighBitY) && ppu->bgLayer[layer].tilemapHigher)
      tilemapAdr += ppu->bgLayer[layer].tilemapWider ? 0x800 : 0x400;
#endif
#endif

#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
    ppu_taR32Dependency(tilemapAdr);
#endif
    const uint16_t tile = ppu->vram[tilemapAdr & 0x7fff];
#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
    const uint64_t r29BGTileTime = r29BGProbe ? __builtin_readcyclecounter() : 0;
    if(r29BGProbe) s_taR29BGTicks[layer][0] += r29BGTileTime-r29BGStart;
#endif
    const int prio = (tile >> 13) & 1;
    const int z = zbg[layer][prio];
    const bool hflip = (tile & 0x4000) != 0;
    const bool vflip = (tile & 0x8000) != 0;
#if defined(ZAMN_R32_PPU_HOISTS)
    const int row = vflip ? lyRowFlip : lyRow;
#else
    const int row = vflip ? 7 - (ly & 7) : (ly & 7);
#endif
    int tileNum = tile & 0x3ff;
    if(big && (((bool)(bx & 8)) ^ hflip)) tileNum += 1;
#if defined(ZAMN_R32_PPU_HOISTS)
    if(big && (bigYHalf ^ vflip)) tileNum += 0x10;
#else
    if(big && (((bool)(ly & 8)) ^ vflip)) tileNum += 0x10;
#endif

    const uint16_t base = (uint16_t)(ppu->bgLayer[layer].tileAdr +
      ((tileNum & 0x3ff) * 4 * bitDepth));
#if defined(ZAMN_R32_PPU_HOISTS)
    const uint16_t rowKey = vflip ? rowKeyFlip : rowKeyNormal;
    const uint32_t rowHash = vflip ? rowHashFlip : rowHashNormal;
    const uint8_t* tileRow =
        ppu_xboxResolvedRowPixels(ppu, layer, tile, base, row,
                                  rowKey, rowHash);
#else
#if defined(TA_R67_BG_FAST) && TA_R67_BG_FAST
    const bool r67DefaultY=(ly==originalLy);
    const uint16_t r67Key=r67DefaultY
        ? (vflip ? r67FlipKey : r67NormalKey)
        : (uint16_t)((row&7)|((layer&3)<<3));
    const uint32_t r67Salt=r67DefaultY
        ? (vflip ? r67FlipSalt : r67NormalSalt)
        : ((uint32_t)r67Key*0x9e3779b1u);
    const uint8_t* tileRow=
        ppu_xboxResolvedRowPixels(ppu,layer,tile,base,row,r67Key,r67Salt);
#else
    const uint8_t* tileRow =
        ppu_xboxResolvedRowPixels(ppu, layer, tile, base, row);
#endif
#endif

#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
    const uint64_t r29BGRowTime = r29BGProbe ? __builtin_readcyclecounter() : 0;
    if(r29BGProbe) s_taR29BGTicks[layer][1] += r29BGRowTime-r29BGTileTime;
#endif
    const int off = lxRaw & 7;
    int n = 8 - off;
    if(n > 256 - sx) n = 256 - sx;

    /*
     * R18 / SMW packed-tile path.
     *
     * R17 still extracted the four SNES bitplanes independently for every
     * output pixel. The SMW Xbox renderer reads the two VRAM words once and
     * then uses fixed shifts for all eight pixels. Almost every chunk after
     * the first scrolled partial chunk is a full tile, so unroll that common
     * case and keep the scalar path only for clipping/windows.
     */
    /* R18: fully visible or fully hidden 8-pixel chunks are eligible for
       the vector compositor, even with active SNES layer windows. We only
       make this decision when the R17 line-local mask is valid, preserving
       the old per-pixel path at mixed window edges and unusual widths. */
    bool vectorMain = mainEnabled, vectorSub = subEnabled;
    bool vectorEligible = off == 0 && n == 8 && !anyWindowed;
#if defined(TA_R18_WINDOW_CHUNK) && TA_R18_WINDOW_CHUNK && defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
    if(off == 0 && n == 8 && anyWindowed && s_taWindowMaskReady) {
      const unsigned mask = (s_taWindowMask[layer][(unsigned)sx >> 5] >> (sx & 31)) & 255u;
      if(mask == 0u || mask == 255u) {
        vectorEligible = true;
        if(mask == 255u) {
          vectorMain = mainEnabled && !mainWindowed;
          vectorSub = subEnabled && !subWindowed;
        }
        TA_DIAG(++s_taR18UniformTiles);
      } else {
        TA_DIAG(++s_taR18MixedTiles);
      }
    }
#endif
    if(vectorEligible) {
      if(!vectorMain && !vectorSub) {
#if defined(TA_R18_WINDOW_CHUNK) && TA_R18_WINDOW_CHUNK && defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
        TA_DIAG(++s_taR18CulledTiles);
#endif
        sx += 8;
        TA_R29_BG_FINISH(3);
        continue;
      }
#if defined(ZAMN_R30_PPU_TRANSPARENT_SKIP)
      /* R30: a resolved row containing eight zero CGRAM indices is fully
         transparent.  The old path still entered the MMX priority compositor
         and discovered that lane by lane.  One 64-bit load lets the common
         empty-row case skip the compositor completely.  Resolved rows are
         eight bytes wide; memcpy keeps this strict-aliasing/alignment safe and
         Clang lowers the constant-size copy to a single load. */
      uint64_t transparentRowBits;
      memcpy(&transparentRowBits, tileRow, sizeof transparentRowBits);
      if(__builtin_expect(transparentRowBits == 0, 0)) {
        TA_DIAG(++s_xboxTransparentBgRows);
        sx += 8;
        TA_R29_BG_FINISH(2);
        continue;
      }
#endif
#if defined(ZAMN_R23_MMX_PPU)
#if defined(ZAMN_R32_PPU_HOISTS)
      const uint64_t* const zQ = &s_xboxByteBroadcast[z & 15];
      if(vectorMain)
        ppu_xboxMode1Put8R69(s_mode1MainPri + sx, s_mode1MainPix + sx,
                          s_mode1MainLayer + sx, tileRow, zQ, layerQ, trackLayer);
      if(vectorSub)
        ppu_xboxMode1Put8R69(s_mode1SubPri + sx, s_mode1SubPix + sx,
                          s_mode1SubLayer + sx, tileRow, zQ, layerQ, trackLayer);
#else
      if(vectorMain)
        ppu_xboxMode1Put8R69(s_mode1MainPri + sx, s_mode1MainPix + sx,
                          s_mode1MainLayer + sx, tileRow, z, layer, trackLayer);
      if(vectorSub)
        ppu_xboxMode1Put8R69(s_mode1SubPri + sx, s_mode1SubPix + sx,
                          s_mode1SubLayer + sx, tileRow, z, layer, trackLayer);
#endif
#if !defined(ZAMN_R24_RELEASE_FAST)
      TA_DIAG(++s_xboxMmxChunks);
#endif
#else
      for(int i = 0; i < 8; ++i) {
        const int p = tileRow[i];
        if(p) {
          const int x = sx + i;
          if(vectorMain)
            ppu_xboxMode1Put(s_mode1MainPri, s_mode1MainPix, s_mode1MainLayer,
                             x, z, layer, p, trackLayer);
          if(vectorSub)
            ppu_xboxMode1Put(s_mode1SubPri, s_mode1SubPix, s_mode1SubLayer,
                             x, z, layer, p, trackLayer);
        }
      }
#endif
#if !defined(ZAMN_R20_LIGHT_PROF)
      TA_DIAG(++s_xboxPackedTiles);
#endif
      sx += 8;
      TA_R29_BG_FINISH(0);
      continue;
    }

#if !defined(ZAMN_R20_LIGHT_PROF)
    TA_DIAG(++s_xboxScalarTileChunks);
#endif
    for(int i = 0; i < n; ++i) {
      const int local = off + i;
      const int pixel = tileRow[local];
      if(pixel) {
        const int x = sx + i;
        bool window = false;
        if(anyWindowed) window = ppu_taNativeWindowState(ppu, layer, x);
        if(mainEnabled && (!mainWindowed || !window))
          ppu_xboxMode1Put(s_mode1MainPri, s_mode1MainPix, s_mode1MainLayer,
                           x, z, layer, pixel, trackLayer);
        if(subEnabled && (!subWindowed || !window))
          ppu_xboxMode1Put(s_mode1SubPri, s_mode1SubPix, s_mode1SubLayer,
                           x, z, layer, pixel, trackLayer);
      }
    }
    sx += n;
    TA_R29_BG_FINISH(1);
  }
#undef TA_R29_BG_FINISH
}


/* R68 hybrid: keep R67's scanline hoists in Mode 1 only.  Use the unmodified
 * R66 tilemap/offset drawing loop for Mode 2; do not execute R67's expensive
 * BG3 column predecode or per-tile dynamic-y fast-path logic there.
 * Two separately compiled draw functions avoid hot-loop mode selection. */
#if defined(TA_R68_MODE1_ONLY) && TA_R68_MODE1_ONLY && defined(TA_R67_BG_FAST) && TA_R67_BG_FAST
#if defined(TA_R96_CULL_TEST_STATS) && TA_R96_CULL_TEST_STATS
static unsigned s_taR96CulledBgTiles;
#endif

#if defined(TA_R98_FIRST_BG_WRITE) && TA_R98_FIRST_BG_WRITE && defined(ZAMN_R23_MMX_PPU) && !defined(ZAMN_R32_PPU_HOISTS)
/* R98: fresh Mode-2 BG destinations have zero priority and pixel bytes.
 * Every chunk of a layer covers disjoint output x coordinates. If no earlier
 * BG is enabled on this screen, every nontransparent pixel wins. Keep stale
 * layer bytes at transparent pixels exactly as the general compositor does.
 * No OPT/row-cache semantics, cache keys or cross-line state are changed. */
static inline void ppu_taR98FirstPut8(uint8_t* pri,uint8_t* pix,uint8_t* lay,
                                    const uint8_t* src,int z,int layer,bool trackLayer) {
  const uint32_t zb=(uint8_t)z,lb=(uint8_t)layer;
  __asm__ volatile(
    "movq (%[src]), %%mm0\n\t"
    "pxor %%mm1, %%mm1\n\t"
    "pcmpeqb %%mm0, %%mm1\n\t"
    "movd %[z], %%mm2\n\t"
    "punpcklbw %%mm2, %%mm2\n\t"
    "punpcklwd %%mm2, %%mm2\n\t"
    "punpckldq %%mm2, %%mm2\n\t"
    "movq %%mm1, %%mm3\n\t"
    "pandn %%mm2, %%mm3\n\t"
    "movq %%mm3, (%[pri])\n\t"
    "movq %%mm0, (%[pix])\n\t"
    : : [src] "r"(src),[pri] "r"(pri),[pix] "r"(pix),[z] "m"(zb)
    : "mm0","mm1","mm2","mm3","memory");
  if(trackLayer) {
    __asm__ volatile(
      "movq (%[src]), %%mm0\n\t"
      "pxor %%mm1, %%mm1\n\t"
      "pcmpeqb %%mm0, %%mm1\n\t"
      "movq (%[lay]), %%mm2\n\t"
      "pand %%mm1, %%mm2\n\t"
      "movd %[layer], %%mm3\n\t"
      "punpcklbw %%mm3, %%mm3\n\t"
      "punpcklwd %%mm3, %%mm3\n\t"
      "punpckldq %%mm3, %%mm3\n\t"
      "pandn %%mm3, %%mm1\n\t"
      "por %%mm2, %%mm1\n\t"
      "movq %%mm1, (%[lay])\n\t"
      : : [src] "r"(src),[lay] "r"(lay),[layer] "m"(lb)
      : "mm0","mm1","mm2","mm3","memory");
  }
}
#endif

#include "ppu_ta_r102_dual.h"

#if defined(TA_R124_MODE2_STREAM) && TA_R124_MODE2_STREAM
/* Temporary raw rows exist only during one current-line BG rebuild. */
typedef struct {uint8_t pix[256],pri[256],blocked[256];} TaR124RawRow;
typedef char TaR124RawLayout[(sizeof(TaR124RawRow)==768 && offsetof(TaR124RawRow,pri)==256 && offsetof(TaR124RawRow,blocked)==512)?1:-1];
static TaR124RawRow s_taR124Rows[2] __attribute__((aligned(32)));
static TaR124RawRow* s_taR124Writing;
static const uint64_t s_taR124Mask[256] __attribute__((aligned(32)))={
0x0000000000000000ULL,0x00000000000000ffULL,0x000000000000ff00ULL,0x000000000000ffffULL,
0x0000000000ff0000ULL,0x0000000000ff00ffULL,0x0000000000ffff00ULL,0x0000000000ffffffULL,
0x00000000ff000000ULL,0x00000000ff0000ffULL,0x00000000ff00ff00ULL,0x00000000ff00ffffULL,
0x00000000ffff0000ULL,0x00000000ffff00ffULL,0x00000000ffffff00ULL,0x00000000ffffffffULL,
0x000000ff00000000ULL,0x000000ff000000ffULL,0x000000ff0000ff00ULL,0x000000ff0000ffffULL,
0x000000ff00ff0000ULL,0x000000ff00ff00ffULL,0x000000ff00ffff00ULL,0x000000ff00ffffffULL,
0x000000ffff000000ULL,0x000000ffff0000ffULL,0x000000ffff00ff00ULL,0x000000ffff00ffffULL,
0x000000ffffff0000ULL,0x000000ffffff00ffULL,0x000000ffffffff00ULL,0x000000ffffffffffULL,
0x0000ff0000000000ULL,0x0000ff00000000ffULL,0x0000ff000000ff00ULL,0x0000ff000000ffffULL,
0x0000ff0000ff0000ULL,0x0000ff0000ff00ffULL,0x0000ff0000ffff00ULL,0x0000ff0000ffffffULL,
0x0000ff00ff000000ULL,0x0000ff00ff0000ffULL,0x0000ff00ff00ff00ULL,0x0000ff00ff00ffffULL,
0x0000ff00ffff0000ULL,0x0000ff00ffff00ffULL,0x0000ff00ffffff00ULL,0x0000ff00ffffffffULL,
0x0000ffff00000000ULL,0x0000ffff000000ffULL,0x0000ffff0000ff00ULL,0x0000ffff0000ffffULL,
0x0000ffff00ff0000ULL,0x0000ffff00ff00ffULL,0x0000ffff00ffff00ULL,0x0000ffff00ffffffULL,
0x0000ffffff000000ULL,0x0000ffffff0000ffULL,0x0000ffffff00ff00ULL,0x0000ffffff00ffffULL,
0x0000ffffffff0000ULL,0x0000ffffffff00ffULL,0x0000ffffffffff00ULL,0x0000ffffffffffffULL,
0x00ff000000000000ULL,0x00ff0000000000ffULL,0x00ff00000000ff00ULL,0x00ff00000000ffffULL,
0x00ff000000ff0000ULL,0x00ff000000ff00ffULL,0x00ff000000ffff00ULL,0x00ff000000ffffffULL,
0x00ff0000ff000000ULL,0x00ff0000ff0000ffULL,0x00ff0000ff00ff00ULL,0x00ff0000ff00ffffULL,
0x00ff0000ffff0000ULL,0x00ff0000ffff00ffULL,0x00ff0000ffffff00ULL,0x00ff0000ffffffffULL,
0x00ff00ff00000000ULL,0x00ff00ff000000ffULL,0x00ff00ff0000ff00ULL,0x00ff00ff0000ffffULL,
0x00ff00ff00ff0000ULL,0x00ff00ff00ff00ffULL,0x00ff00ff00ffff00ULL,0x00ff00ff00ffffffULL,
0x00ff00ffff000000ULL,0x00ff00ffff0000ffULL,0x00ff00ffff00ff00ULL,0x00ff00ffff00ffffULL,
0x00ff00ffffff0000ULL,0x00ff00ffffff00ffULL,0x00ff00ffffffff00ULL,0x00ff00ffffffffffULL,
0x00ffff0000000000ULL,0x00ffff00000000ffULL,0x00ffff000000ff00ULL,0x00ffff000000ffffULL,
0x00ffff0000ff0000ULL,0x00ffff0000ff00ffULL,0x00ffff0000ffff00ULL,0x00ffff0000ffffffULL,
0x00ffff00ff000000ULL,0x00ffff00ff0000ffULL,0x00ffff00ff00ff00ULL,0x00ffff00ff00ffffULL,
0x00ffff00ffff0000ULL,0x00ffff00ffff00ffULL,0x00ffff00ffffff00ULL,0x00ffff00ffffffffULL,
0x00ffffff00000000ULL,0x00ffffff000000ffULL,0x00ffffff0000ff00ULL,0x00ffffff0000ffffULL,
0x00ffffff00ff0000ULL,0x00ffffff00ff00ffULL,0x00ffffff00ffff00ULL,0x00ffffff00ffffffULL,
0x00ffffffff000000ULL,0x00ffffffff0000ffULL,0x00ffffffff00ff00ULL,0x00ffffffff00ffffULL,
0x00ffffffffff0000ULL,0x00ffffffffff00ffULL,0x00ffffffffffff00ULL,0x00ffffffffffffffULL,
0xff00000000000000ULL,0xff000000000000ffULL,0xff0000000000ff00ULL,0xff0000000000ffffULL,
0xff00000000ff0000ULL,0xff00000000ff00ffULL,0xff00000000ffff00ULL,0xff00000000ffffffULL,
0xff000000ff000000ULL,0xff000000ff0000ffULL,0xff000000ff00ff00ULL,0xff000000ff00ffffULL,
0xff000000ffff0000ULL,0xff000000ffff00ffULL,0xff000000ffffff00ULL,0xff000000ffffffffULL,
0xff0000ff00000000ULL,0xff0000ff000000ffULL,0xff0000ff0000ff00ULL,0xff0000ff0000ffffULL,
0xff0000ff00ff0000ULL,0xff0000ff00ff00ffULL,0xff0000ff00ffff00ULL,0xff0000ff00ffffffULL,
0xff0000ffff000000ULL,0xff0000ffff0000ffULL,0xff0000ffff00ff00ULL,0xff0000ffff00ffffULL,
0xff0000ffffff0000ULL,0xff0000ffffff00ffULL,0xff0000ffffffff00ULL,0xff0000ffffffffffULL,
0xff00ff0000000000ULL,0xff00ff00000000ffULL,0xff00ff000000ff00ULL,0xff00ff000000ffffULL,
0xff00ff0000ff0000ULL,0xff00ff0000ff00ffULL,0xff00ff0000ffff00ULL,0xff00ff0000ffffffULL,
0xff00ff00ff000000ULL,0xff00ff00ff0000ffULL,0xff00ff00ff00ff00ULL,0xff00ff00ff00ffffULL,
0xff00ff00ffff0000ULL,0xff00ff00ffff00ffULL,0xff00ff00ffffff00ULL,0xff00ff00ffffffffULL,
0xff00ffff00000000ULL,0xff00ffff000000ffULL,0xff00ffff0000ff00ULL,0xff00ffff0000ffffULL,
0xff00ffff00ff0000ULL,0xff00ffff00ff00ffULL,0xff00ffff00ffff00ULL,0xff00ffff00ffffffULL,
0xff00ffffff000000ULL,0xff00ffffff0000ffULL,0xff00ffffff00ff00ULL,0xff00ffffff00ffffULL,
0xff00ffffffff0000ULL,0xff00ffffffff00ffULL,0xff00ffffffffff00ULL,0xff00ffffffffffffULL,
0xffff000000000000ULL,0xffff0000000000ffULL,0xffff00000000ff00ULL,0xffff00000000ffffULL,
0xffff000000ff0000ULL,0xffff000000ff00ffULL,0xffff000000ffff00ULL,0xffff000000ffffffULL,
0xffff0000ff000000ULL,0xffff0000ff0000ffULL,0xffff0000ff00ff00ULL,0xffff0000ff00ffffULL,
0xffff0000ffff0000ULL,0xffff0000ffff00ffULL,0xffff0000ffffff00ULL,0xffff0000ffffffffULL,
0xffff00ff00000000ULL,0xffff00ff000000ffULL,0xffff00ff0000ff00ULL,0xffff00ff0000ffffULL,
0xffff00ff00ff0000ULL,0xffff00ff00ff00ffULL,0xffff00ff00ffff00ULL,0xffff00ff00ffffffULL,
0xffff00ffff000000ULL,0xffff00ffff0000ffULL,0xffff00ffff00ff00ULL,0xffff00ffff00ffffULL,
0xffff00ffffff0000ULL,0xffff00ffffff00ffULL,0xffff00ffffffff00ULL,0xffff00ffffffffffULL,
0xffffff0000000000ULL,0xffffff00000000ffULL,0xffffff000000ff00ULL,0xffffff000000ffffULL,
0xffffff0000ff0000ULL,0xffffff0000ff00ffULL,0xffffff0000ffff00ULL,0xffffff0000ffffffULL,
0xffffff00ff000000ULL,0xffffff00ff0000ffULL,0xffffff00ff00ff00ULL,0xffffff00ff00ffffULL,
0xffffff00ffff0000ULL,0xffffff00ffff00ffULL,0xffffff00ffffff00ULL,0xffffff00ffffffffULL,
0xffffffff00000000ULL,0xffffffff000000ffULL,0xffffffff0000ff00ULL,0xffffffff0000ffffULL,
0xffffffff00ff0000ULL,0xffffffff00ff00ffULL,0xffffffff00ffff00ULL,0xffffffff00ffffffULL,
0xffffffffff000000ULL,0xffffffffff0000ffULL,0xffffffffff00ff00ULL,0xffffffffff00ffffULL,
0xffffffffffff0000ULL,0xffffffffffff00ffULL,0xffffffffffffff00ULL,0xffffffffffffffffULL
};
#if defined(TA_R124_STREAM_PROFILE) && TA_R124_STREAM_PROFILE
static uint64_t s_taR124Counts[4];
void ppu_taR124Counts(uint64_t out[4]){memcpy(out,s_taR124Counts,sizeof s_taR124Counts);}
#define TA_R124_COUNT(i) (++s_taR124Counts[i])
#else
#define TA_R124_COUNT(i) ((void)0)
#endif
#endif

static void ppu_xboxMode1DrawLayerR68Baseline(Ppu* ppu, int layer, int y,
                                   const uint8_t zbg[3][2], bool needSub,
                                   bool trackLayer) {
  TA_R82_SCOPE(TA82_BG);
  const Layer* ls = &ppu->layer[layer];
  const bool mainEnabled = ls->mainScreenEnabled;
  const bool subEnabled = needSub && ls->subScreenEnabled;
  if(!mainEnabled && !subEnabled) return;

  const bool mainWindowed = mainEnabled && ls->mainScreenWindowed;
  const bool subWindowed = subEnabled && ls->subScreenWindowed;
  const bool anyWindowed = mainWindowed || subWindowed;
#if defined(TA_R98_FIRST_BG_WRITE) && TA_R98_FIRST_BG_WRITE && defined(ZAMN_R23_MMX_PPU) && !defined(ZAMN_R32_PPU_HOISTS)
  /* Renderer clears both planes before BG1, then draws BG2, then sprites.
   * BG2 can also use the shortcut independently on an unused screen. */
  const bool firstMain=layer==0 || (layer==1 && !ppu->layer[0].mainScreenEnabled);
  const bool firstSub=layer==0 || (layer==1 && !ppu->layer[0].subScreenEnabled);
#endif
  const int bitDepth = (layer == 2) ? 2 : 4;
  const bool big = ppu->bgLayer[layer].bigTiles;
  const int tileBitsX = big ? 4 : 3;
  const int tileHighBitX = big ? 0x200 : 0x100;
  const int tileBitsY = big ? 4 : 3;
  const int tileHighBitY = big ? 0x200 : 0x100;
  const int originalLy = (y + ppu->bgLayer[layer].vScroll) & 0x3ff;
#if defined(ZAMN_R32_PPU_HOISTS)
  const int ly = originalLy;
  /* R32: everything below is invariant for this layer/scanline.  R31 still
     rebuilt these values for every 8-pixel tile chunk. */
  const int lyRow = ly & 7;
  const int lyRowFlip = 7 - lyRow;
  const bool bigYHalf = big && ((ly & 8) != 0);
  uint16_t tilemapRowBase = (uint16_t)(ppu->bgLayer[layer].tilemapAdr +
      (((ly >> tileBitsY) & 0x1f) << 5));
  if((ly & tileHighBitY) && ppu->bgLayer[layer].tilemapHigher)
    tilemapRowBase = (uint16_t)(tilemapRowBase +
        (ppu->bgLayer[layer].tilemapWider ? 0x800 : 0x400));

  const uint16_t rowKeyNormal =
      (uint16_t)((lyRow & 7) | ((layer & 3) << 3));
  const uint16_t rowKeyFlip =
      (uint16_t)((lyRowFlip & 7) | ((layer & 3) << 3));
  const uint32_t rowHashNormal =
      (uint32_t)rowKeyNormal * 0x9e3779b1u;
  const uint32_t rowHashFlip =
      (uint32_t)rowKeyFlip * 0x9e3779b1u;
  const uint64_t* const layerQ = &s_xboxByteBroadcast[layer & 15];
#endif
  int sx = 0;
#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
  const bool r29BGProbe = s_taR29BGSample && layer < 2;
#define TA_R29_BG_FINISH(KIND) do { \
  if(r29BGProbe) { \
    s_taR29BGTicks[layer][2] += __builtin_readcyclecounter()-r29BGRowTime; \
    ++s_taR29BGCounts[layer][(KIND)]; \
  } \
} while(0)
#else
#define TA_R29_BG_FINISH(KIND) ((void)0)
#endif
  while(sx < 256) {
#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
    const uint64_t r29BGStart = r29BGProbe ? __builtin_readcyclecounter() : 0;
#endif
    const int lxRaw = sx + ppu->bgLayer[layer].hScroll;
#if defined(TA_R96_MODE2_EARLY_OCCLUSION) && TA_R96_MODE2_EARLY_OCCLUSION && \
    defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
    /* R96: the old R18 all-windowed check ran AFTER tilemap/OPT fetch and
     * decoded-row lookup. Skip the *entire* invisible eight-pixel segment
     * before reading VRAM. The active main/sub screens must both be fully
     * masked; the exact prepared window mask and current BG layer state are
     * part of the R32 key. A later window change therefore invalidates the
     * cached result. This conservatively handles only aligned 8px segments
     * wholly within one 32-bit mask word; all other cases use R93/R94. */
    if(anyWindowed && s_taWindowMaskReady &&
       (!mainEnabled || mainWindowed) && (!subEnabled || subWindowed) &&
       ((lxRaw & 7)==0) && sx<=248 && ((sx & 31)<=24) &&
       (((s_taWindowMask[layer][(unsigned)sx>>5] >> (sx & 31)) & 255u)==255u)) {
#if defined(TA_R96_CULL_TEST_STATS) && TA_R96_CULL_TEST_STATS
      ++s_taR96CulledBgTiles;
#endif
      sx+=8;
      continue;
    }
#endif
    int bx = (lxRaw & ~7) & 0x3ff;
#if !defined(ZAMN_R32_PPU_HOISTS)
    int ly = originalLy;
#endif
    /* SNES Mode 2 offset-per-tile: BG3 provides per-eight-screen-pixel
       X and Y overrides. The source x/y must be resolved BEFORE fetching
       the tilemap row. As in ppu_handleOPT, the first column cannot offset.
       This preserves both BG-specific validity bits and the unmodified
       original pixel's low 3 bits. Only one map lookup per chunk, instead
       of per layer/priority/pixel in the generic reference path. */
    if(ppu->mode == 2) {
      const int column = ((lxRaw & ~7) -
          (ppu->bgLayer[layer].hScroll & ~7)) >> 3;
      if(column > 0) {
        const uint16_t valid = layer == 0 ? 0x2000u : 0x4000u;
        uint16_t hOff, vOff;
#if defined(TA_R12_OFFSET_CACHE) && TA_R12_OFFSET_CACHE
        ppu_taOffsetPair(ppu, column - 1, &hOff, &vOff);
#else
        hOff = ppu_getOffsetValue(ppu, column - 1, 0);
        vOff = ppu_getOffsetValue(ppu, column - 1, 1);
#endif
        if(hOff & valid)
          bx = ((hOff & 0x3f8) + column * 8) & 0x3ff;
#if !defined(ZAMN_R32_PPU_HOISTS)
        if(vOff & valid)
          ly = ((vOff & 0x3ff) + y) & 0x3ff;
#endif
      }
    }
#if defined(ZAMN_R32_PPU_HOISTS)
    uint16_t tilemapAdr =
        (uint16_t)(tilemapRowBase + ((bx >> tileBitsX) & 0x1f));
    if((bx & tileHighBitX) && ppu->bgLayer[layer].tilemapWider)
      tilemapAdr = (uint16_t)(tilemapAdr + 0x400);
#else
    uint16_t tilemapAdr = ppu->bgLayer[layer].tilemapAdr +
      (((ly >> tileBitsY) & 0x1f) << 5 | ((bx >> tileBitsX) & 0x1f));
    if((bx & tileHighBitX) && ppu->bgLayer[layer].tilemapWider) tilemapAdr += 0x400;
    if((ly & tileHighBitY) && ppu->bgLayer[layer].tilemapHigher)
      tilemapAdr += ppu->bgLayer[layer].tilemapWider ? 0x800 : 0x400;
#endif

#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
    ppu_taR32Dependency(tilemapAdr);
#endif
    const uint16_t tile = ppu->vram[tilemapAdr & 0x7fff];
#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
    const uint64_t r29BGTileTime = r29BGProbe ? __builtin_readcyclecounter() : 0;
    if(r29BGProbe) s_taR29BGTicks[layer][0] += r29BGTileTime-r29BGStart;
#endif
    const int prio = (tile >> 13) & 1;
    const int z = zbg[layer][prio];
    const bool hflip = (tile & 0x4000) != 0;
    const bool vflip = (tile & 0x8000) != 0;
#if defined(ZAMN_R32_PPU_HOISTS)
    const int row = vflip ? lyRowFlip : lyRow;
#else
    const int row = vflip ? 7 - (ly & 7) : (ly & 7);
#endif
    int tileNum = tile & 0x3ff;
    if(big && (((bool)(bx & 8)) ^ hflip)) tileNum += 1;
#if defined(ZAMN_R32_PPU_HOISTS)
    if(big && (bigYHalf ^ vflip)) tileNum += 0x10;
#else
    if(big && (((bool)(ly & 8)) ^ vflip)) tileNum += 0x10;
#endif

    const uint16_t base = (uint16_t)(ppu->bgLayer[layer].tileAdr +
      ((tileNum & 0x3ff) * 4 * bitDepth));
#if defined(ZAMN_R32_PPU_HOISTS)
    const uint16_t rowKey = vflip ? rowKeyFlip : rowKeyNormal;
    const uint32_t rowHash = vflip ? rowHashFlip : rowHashNormal;
    const uint8_t* tileRow =
        ppu_xboxResolvedRowPixels(ppu, layer, tile, base, row,
                                  rowKey, rowHash);
#else
    /* Use the original direct-mapped row-cache geometry and hash.
     * In an R68 split build the shared row-cache function accepts the
     * exact precomputed key/hash pair used by the existing one-way path. */
    const uint16_t rowKey = (uint16_t)((row & 7) | ((layer & 3) << 3));
    const uint32_t rowHash = (uint32_t)rowKey * 0x9e3779b1u;
    const uint8_t* tileRow =
        ppu_xboxResolvedRowPixels(ppu, layer, tile, base, row,
                                  rowKey, rowHash);
#endif

#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
    const uint64_t r29BGRowTime = r29BGProbe ? __builtin_readcyclecounter() : 0;
    if(r29BGProbe) s_taR29BGTicks[layer][1] += r29BGRowTime-r29BGTileTime;
#endif
    const int off = lxRaw & 7;
    int n = 8 - off;
    if(n > 256 - sx) n = 256 - sx;

#if defined(TA_R124_MODE2_STREAM) && TA_R124_MODE2_STREAM
    if(s_taR124Writing) {
      TA_R124_COUNT(3);
      if(off==0 && n==8) {
        /* Preserve R18's exact chunk decision before screen composition.
         * Mixed chunks use per-pixel window bits, including a word crossing.
         * Uniform chunks retain the original uniform-visible/hidden decision. */
        unsigned mask=0;
        if(anyWindowed){
          const unsigned shift=(unsigned)sx&31u,word=(unsigned)sx>>5;
          mask=(s_taWindowMask[layer][word]>>shift)&255u;
          if(mask!=0 && mask!=255u && shift>24u)
            mask|=(s_taWindowMask[layer][word+1]<<(32u-shift))&255u;
        }
        uint8_t* d=s_taR124Writing->pix+sx;
        __asm__ volatile("movq (%[src]), %%mm0\n\tmovq %%mm0, (%[dst])\n\t"
          "pxor %%mm1, %%mm1\n\tpcmpeqb %%mm0, %%mm1\n\tpandn %[z], %%mm1\n\t"
          "movq %%mm1, 256(%[dst])\n\t"
          "movq %[blocked], %%mm2\n\tmovq %%mm2, 512(%[dst])"
          : : [src] "r"(tileRow),[dst] "r"(d),[z] "m"(s_taR102Broadcast[z&15]),[blocked] "m"(s_taR124Mask[mask])
          : "mm0","mm1","mm2","memory");
      } else for(int j=0;j<n;++j) {
        s_taR124Writing->pix[sx+j]=tileRow[off+j];
        s_taR124Writing->pri[sx+j]=tileRow[off+j]?(uint8_t)z:0;
        s_taR124Writing->blocked[sx+j]=anyWindowed &&
          ((s_taWindowMask[layer][(unsigned)(sx+j)>>5]>>((sx+j)&31))&1u)?255:0;
      }
      sx+=n;continue;
    }
#endif

    /*
     * R18 / SMW packed-tile path.
     *
     * R17 still extracted the four SNES bitplanes independently for every
     * output pixel. The SMW Xbox renderer reads the two VRAM words once and
     * then uses fixed shifts for all eight pixels. Almost every chunk after
     * the first scrolled partial chunk is a full tile, so unroll that common
     * case and keep the scalar path only for clipping/windows.
     */
    /* R18: fully visible or fully hidden 8-pixel chunks are eligible for
       the vector compositor, even with active SNES layer windows. We only
       make this decision when the R17 line-local mask is valid, preserving
       the old per-pixel path at mixed window edges and unusual widths. */
    bool vectorMain = mainEnabled, vectorSub = subEnabled;
    bool vectorEligible = off == 0 && n == 8 && !anyWindowed;
#if defined(TA_R18_WINDOW_CHUNK) && TA_R18_WINDOW_CHUNK && defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
    if(off == 0 && n == 8 && anyWindowed && s_taWindowMaskReady) {
      const unsigned mask = (s_taWindowMask[layer][(unsigned)sx >> 5] >> (sx & 31)) & 255u;
      if(mask == 0u || mask == 255u) {
        vectorEligible = true;
        if(mask == 255u) {
          vectorMain = mainEnabled && !mainWindowed;
          vectorSub = subEnabled && !subWindowed;
        }
        TA_DIAG(++s_taR18UniformTiles);
      } else {
        TA_DIAG(++s_taR18MixedTiles);
      }
    }
#endif
    if(vectorEligible) {
#if defined(TA_R103_GUARD_AUDIT) && TA_R103_GUARD_AUDIT
      if(ppu->mode==2){
        if(vectorMain && vectorSub)++s_r103Mode2BgCandidates[2];
        else if(vectorMain)++s_r103Mode2BgCandidates[0];
        else if(vectorSub)++s_r103Mode2BgCandidates[1];
        else ++s_r103Mode2BgCandidates[3];
      }
#endif
      if(!vectorMain && !vectorSub) {
#if defined(TA_R18_WINDOW_CHUNK) && TA_R18_WINDOW_CHUNK && defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
        TA_DIAG(++s_taR18CulledTiles);
#endif
        sx += 8;
        TA_R29_BG_FINISH(3);
        continue;
      }
#if defined(ZAMN_R30_PPU_TRANSPARENT_SKIP)
      /* R30: a resolved row containing eight zero CGRAM indices is fully
         transparent.  The old path still entered the MMX priority compositor
         and discovered that lane by lane.  One 64-bit load lets the common
         empty-row case skip the compositor completely.  Resolved rows are
         eight bytes wide; memcpy keeps this strict-aliasing/alignment safe and
         Clang lowers the constant-size copy to a single load. */
      uint64_t transparentRowBits;
      memcpy(&transparentRowBits, tileRow, sizeof transparentRowBits);
      if(__builtin_expect(transparentRowBits == 0, 0)) {
        TA_DIAG(++s_xboxTransparentBgRows);
        sx += 8;
        TA_R29_BG_FINISH(2);
        continue;
      }
#endif
#if defined(ZAMN_R23_MMX_PPU)
#if defined(ZAMN_R32_PPU_HOISTS)
      const uint64_t* const zQ = &s_xboxByteBroadcast[z & 15];
      if(vectorMain)
        ppu_xboxMode1Put8(s_mode1MainPri + sx, s_mode1MainPix + sx,
                          s_mode1MainLayer + sx, tileRow, zQ, layerQ, trackLayer);
      if(vectorSub)
        ppu_xboxMode1Put8(s_mode1SubPri + sx, s_mode1SubPix + sx,
                          s_mode1SubLayer + sx, tileRow, zQ, layerQ, trackLayer);
#else
#if defined(TA_R102_MODE2_DUAL_PUT) && TA_R102_MODE2_DUAL_PUT
      if(vectorMain && vectorSub) {
#if defined(TA_R103_GUARD_AUDIT) && TA_R103_GUARD_AUDIT
        if(ppu->mode==2)++s_r103Mode2BgDualCalls;
#endif
        ppu_taR102PutBoth((unsigned)sx,tileRow,z,layer,trackLayer);
      }
      else {
#endif
      if(vectorMain) {
#if defined(TA_R98_FIRST_BG_WRITE) && TA_R98_FIRST_BG_WRITE
        if(firstMain) ppu_taR98FirstPut8(s_mode1MainPri+sx,s_mode1MainPix+sx,
                          s_mode1MainLayer+sx,tileRow,z,layer,trackLayer);
        else
#endif
        ppu_xboxMode1Put8(s_mode1MainPri+sx,s_mode1MainPix+sx,
                          s_mode1MainLayer+sx,tileRow,z,layer,trackLayer);
      }
      if(vectorSub) {
#if defined(TA_R98_FIRST_BG_WRITE) && TA_R98_FIRST_BG_WRITE
        if(firstSub) ppu_taR98FirstPut8(s_mode1SubPri+sx,s_mode1SubPix+sx,
                          s_mode1SubLayer+sx,tileRow,z,layer,trackLayer);
        else
#endif
        ppu_xboxMode1Put8(s_mode1SubPri+sx,s_mode1SubPix+sx,
                          s_mode1SubLayer+sx,tileRow,z,layer,trackLayer);
      }
#if defined(TA_R102_MODE2_DUAL_PUT) && TA_R102_MODE2_DUAL_PUT
      }
#endif
#endif
#if !defined(ZAMN_R24_RELEASE_FAST)
      TA_DIAG(++s_xboxMmxChunks);
#endif
#else
      for(int i = 0; i < 8; ++i) {
        const int p = tileRow[i];
        if(p) {
          const int x = sx + i;
          if(vectorMain)
            ppu_xboxMode1Put(s_mode1MainPri, s_mode1MainPix, s_mode1MainLayer,
                             x, z, layer, p, trackLayer);
          if(vectorSub)
            ppu_xboxMode1Put(s_mode1SubPri, s_mode1SubPix, s_mode1SubLayer,
                             x, z, layer, p, trackLayer);
        }
      }
#endif
#if !defined(ZAMN_R20_LIGHT_PROF)
      TA_DIAG(++s_xboxPackedTiles);
#endif
      sx += 8;
      TA_R29_BG_FINISH(0);
      continue;
    }

#if !defined(ZAMN_R20_LIGHT_PROF)
    TA_DIAG(++s_xboxScalarTileChunks);
#endif
    for(int i = 0; i < n; ++i) {
      const int local = off + i;
      const int pixel = tileRow[local];
      if(pixel) {
        const int x = sx + i;
        bool window = false;
        if(anyWindowed) window = ppu_taNativeWindowState(ppu, layer, x);
        if(mainEnabled && (!mainWindowed || !window))
          ppu_xboxMode1Put(s_mode1MainPri, s_mode1MainPix, s_mode1MainLayer,
                           x, z, layer, pixel, trackLayer);
        if(subEnabled && (!subWindowed || !window))
          ppu_xboxMode1Put(s_mode1SubPri, s_mode1SubPix, s_mode1SubLayer,
                           x, z, layer, pixel, trackLayer);
      }
    }
    sx += n;
    TA_R29_BG_FINISH(1);
  }
#undef TA_R29_BG_FINISH
}

static inline void ppu_xboxMode1DrawLayerR68(Ppu* ppu,int layer,int y,
      const uint8_t zbg[3][2],bool needSub,bool trackLayer) {
  if(ppu->mode == 2) ppu_xboxMode1DrawLayerR68Baseline(ppu,layer,y,zbg,needSub,trackLayer);
  else ppu_xboxMode1DrawLayer(ppu,layer,y,zbg,needSub,trackLayer);
}
#else
#define ppu_xboxMode1DrawLayerR68 ppu_xboxMode1DrawLayer
#endif

static void ppu_xboxMode1DrawSprites(Ppu* ppu, const uint8_t zspr[4], bool needSub, bool trackLayer) {
  TA_R82_SCOPE(TA82_SPRITE_COMP);
  const Layer* ls = &ppu->layer[4];
  const bool mainEnabled = ls->mainScreenEnabled;
  const bool subEnabled = needSub && ls->subScreenEnabled;
  if(!mainEnabled && !subEnabled) return;
  const bool mainWindowed = mainEnabled && ls->mainScreenWindowed;
  const bool subWindowed = subEnabled && ls->subScreenWindowed;
  const bool anyWindowed = mainWindowed || subWindowed;

#if defined(TA_R27_SPRITE_GROUP_SKIP) && TA_R27_SPRITE_GROUP_SKIP
  for(int bx = 0; bx < 256; bx += 8) {
    uint64_t group;
    /* 8-byte output buffer is aligned for efficient i386 load, and memcpy
       avoids strict-aliasing violations for the original uint8_t array. */
    memcpy(&group, &ppu->objPixelBuffer[bx], sizeof group);
    if(!group) { ++s_taR27SpriteEmptyGroups; continue; }
    ++s_taR27SpriteLiveGroups;
    for(int x = bx; x < bx + 8; ++x) {
#else
  for(int x = 0; x < 256; ++x) {
#endif
    const int pixel = ppu->objPixelBuffer[x];
    if(!pixel) continue;
    bool window = false;
    if(anyWindowed) window = ppu_taNativeWindowState(ppu, 4, x);
    const int z = zspr[ppu->objPriorityBuffer[x] & 3];
    const int layer = pixel < 0xc0 ? 6 : 4;
    if(mainEnabled && (!mainWindowed || !window))
      ppu_xboxMode1Put(s_mode1MainPri, s_mode1MainPix, s_mode1MainLayer,
                         x, z, layer, pixel, trackLayer);
    if(subEnabled && (!subWindowed || !window))
      ppu_xboxMode1Put(s_mode1SubPri, s_mode1SubPix, s_mode1SubLayer,
                         x, z, layer, pixel, trackLayer);
#if defined(TA_R27_SPRITE_GROUP_SKIP) && TA_R27_SPRITE_GROUP_SKIP
    }
#endif
  }
}

#include "ppu_ta_r124_stream.h"

#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
static bool ppu_taR32RestoreBG(Ppu* ppu, int row, bool needSub, bool trackLayer) {
#if defined(TA_R109_FUSED_MODE2_COMPOSE) && TA_R109_FUSED_MODE2_COMPOSE
  s_taR109MainPri=s_mode1MainPri;s_taR109SubPri=s_mode1SubPri;
#endif
  /* Includes direct calls and all early miss/unsupported-mode returns. */
  ppu_taR93ResetSubView();
  ppu_taR94ResetMainView();
#if defined(TA_R70_MODE1_LINE_CACHE) && TA_R70_MODE1_LINE_CACHE
  if((ppu->mode != 1 && ppu->mode != 2) || (unsigned)row>=PPU_LINES) return false;
  const bool mode1=ppu->mode==1;
  TaR32BGLine* e=mode1 ? &s_taR70Mode1Lines[row] : &s_taR32Lines[row];
  TaR70Mode1ExtraKey extra;
  if(mode1) {
    memset(&extra,0,sizeof extra);
    memcpy(&extra.bg3,&ppu->bgLayer[2],sizeof extra.bg3);
    memcpy(&extra.layer3,&ppu->layer[2],sizeof extra.layer3);
    memcpy(&extra.window3,&ppu->windowLayer[2],sizeof extra.window3);
    extra.bg3priority=(uint8_t)ppu->bg3priority;
  }
#else
  if(ppu->mode!=2 || (unsigned)row>=PPU_LINES) return false;
  TaR32BGLine* e=&s_taR32Lines[row];
#endif
  s_taR32CacheActive=true;
#if defined(TA_R123_REVERSE_MODE2_DIRTY) && TA_R123_REVERSE_MODE2_DIRTY
  /* Fold writes into OLD associations before any cache miss replaces them.
   * A fresh row built after those writes must not be dirtied retroactively. */
  if(ppu->mode==2) ppu_taR123ApplyQueued();
#endif
  TA_R87_SCOPE(r87Key,TA87_KEY);
  TaR32BGKey key;
#if defined(TA_R86_MODE2_DIRECT_KEY) && TA_R86_MODE2_DIRECT_KEY
  /* R86: a Mode-2 hit should not need a temporary full key. Test the
   * identical persisted fields directly, including byte-exact structure
   * comparison (and its padding) for the previously copied input arrays.
   * Do not alter dependency/stamp validation or output restoration below.
   * Mode 1 remains on the R70 reference construction path. */
  const bool r86Mode2=(ppu->mode==2);
  bool hit=false;
  if(r86Mode2 && e->valid) {
    const TaR32BGKey* const saved=&e->key;
    hit=(saved->needSub==needSub && saved->trackLayer==trackLayer &&
         saved->edges[0]==ppu->window1left && saved->edges[1]==ppu->window1right &&
         saved->edges[2]==ppu->window2left && saved->edges[3]==ppu->window2right &&
         memcmp(saved->bg,ppu->bgLayer,sizeof saved->bg)==0 &&
         memcmp(saved->layer,ppu->layer,sizeof saved->layer)==0 &&
         memcmp(saved->window,ppu->windowLayer,sizeof saved->window)==0);
  }
#if defined(TA_R86_KEY_AUDIT) && TA_R86_KEY_AUDIT
  if(r86Mode2) { if(hit) ++s_taR86KeyMatches; else ++s_taR86KeyMisses; }
#endif
  if(!r86Mode2) {
#endif
    memset(&key,0,sizeof key);
    memcpy(key.bg,ppu->bgLayer,sizeof key.bg);
    memcpy(key.layer,ppu->layer,sizeof key.layer);
    memcpy(key.window,ppu->windowLayer,sizeof key.window);
    key.edges[0]=ppu->window1left; key.edges[1]=ppu->window1right;
    key.edges[2]=ppu->window2left; key.edges[3]=ppu->window2right;
    key.needSub=needSub; key.trackLayer=trackLayer;
#if defined(TA_R86_MODE2_DIRECT_KEY) && TA_R86_MODE2_DIRECT_KEY
    hit=e->valid && memcmp(&key,&e->key,sizeof key)==0;
  }
#else
  bool hit=e->valid && memcmp(&key,&e->key,sizeof key)==0;
#endif
#if defined(TA_R70_MODE1_LINE_CACHE) && TA_R70_MODE1_LINE_CACHE
  if(hit && mode1)
    hit=memcmp(&extra,&s_taR70Mode1Extras[row],sizeof extra)==0;
#endif

  TA_R87_FINISH(r87Key);
  ta_r87_cache_event(hit?TA87_EVENT_KEY_HIT:TA87_EVENT_KEY_MISS);
  if(hit) {
    TA_R87_SCOPE(r87Dep,TA87_DEP);
#if defined(TA_R123_REVERSE_MODE2_DIRTY) && TA_R123_REVERSE_MODE2_DIRTY
    if(ppu->mode==2) {
      if(s_taR123DirtyRows[(unsigned)row>>5] & (1u<<((unsigned)row&31u))) {
        ta_r87_cache_event(TA87_EVENT_GENERATION_FAIL);
        hit=false;
      }
    } else
#endif
    {
#if defined(TA_R71_MODE2_VRAM_STAMP) && TA_R71_MODE2_VRAM_STAMP
    const bool stampMatch=(ppu->mode==2 && e->validatedVramStamp==s_taR71VramStamp);
#if defined(TA_R71_STAMP_PROFILE) && TA_R71_STAMP_PROFILE
    if(ppu->mode==2) {
      if(stampMatch) ++s_taR71BypassCount;
      else ++s_taR71CheckedCount;
    }
#endif
    if(stampMatch) ta_r87_cache_event(TA87_EVENT_STAMP_BYPASS);
    if(!stampMatch)
#endif
      for(unsigned i=0;i<e->count;++i) {
        if(i==0) ta_r87_cache_event(TA87_EVENT_GENERATION_SCAN);
        if(e->generation[i]!=s_taR32PageGeneration[e->page[i]]) {
          ta_r87_cache_event(TA87_EVENT_GENERATION_FAIL);hit=false;break;}
      }
    }
  }
  if(hit) {
    TA_R87_SCOPE(r87Restore,TA87_RESTORE);
#if defined(TA_R71_MODE2_VRAM_STAMP) && TA_R71_MODE2_VRAM_STAMP
    if(ppu->mode==2) e->validatedVramStamp=s_taR71VramStamp;
#endif
#if defined(TA_R43_LINE_OUTPUT_CACHE) && TA_R43_LINE_OUTPUT_CACHE
    s_taR43BgHit=true;
    if(s_taR43OutputTarget) {
      s_taR43OutputTried=true;
      if(ppu_taR43RestoreOutput(ppu,(unsigned)row,s_taR43OutputTarget)) {
        s_taR43OutputComplete=true;
#if defined(TA_R32_CACHE_PROFILE) && TA_R32_CACHE_PROFILE
        ++s_taR32Hits;
#endif
        return true;
      }
    }
#endif
    ta_r87_cache_event(TA87_EVENT_BG_RESTORE);
#if defined(TA_R109_FUSED_MODE2_COMPOSE) && TA_R109_FUSED_MODE2_COMPOSE
    if(s_taR109Fuse) {
      /* Output resolves OBJ locally; no writer needs a scratch restoration. */
      s_taR109MainPri=e->mainPri;s_taR109SubPri=e->subPri;
      s_taR94ReadMainPix=e->mainPix;s_taR94ReadMainLayer=e->mainLayer;
      s_mode1ReadSubPix=e->subPix;s_mode1ReadSubLayer=e->subLayer;
      TA_R109_COUNT(2);
    } else
#endif
#if defined(TA_R88_PACKED_BG_COPY) && TA_R88_PACKED_BG_COPY
    if(ppu->mode==2) {
      /* Original semantics: 512 main bytes + optional 256 layer bytes,
       * followed by 768 subscreen bytes when needSub, unless R93 can read
       * the validated BG entry directly with subscreen OBJ disabled.
       * Both source and destination use declared, adjacent uint8_t arrays. */
#if defined(TA_R94_READONLY_MAIN_BG) && TA_R94_READONLY_MAIN_BG
      if(ppu_taR94MainObjInactive(ppu)) {
        s_taR94ReadMainPix=e->mainPix;
        s_taR94ReadMainLayer=e->mainLayer;
      } else
#endif
        memcpy(s_mode1MainPri,e->mainPri,trackLayer?768u:512u);
      if(needSub) {
#if defined(TA_R93_READONLY_SUB_BG) && TA_R93_READONLY_SUB_BG
        if(!ppu->layer[4].subScreenEnabled) {
          s_mode1ReadSubPix=e->subPix;
          s_mode1ReadSubLayer=e->subLayer;
        } else
#endif
          memcpy(s_mode1SubPri,e->subPri,768u);
      }
    } else
#endif
    {
      memcpy(s_mode1MainPri,e->mainPri,256); memcpy(s_mode1MainPix,e->mainPix,256);
      if(trackLayer) memcpy(s_mode1MainLayer,e->mainLayer,256);
      if(needSub) {
        memcpy(s_mode1SubPri,e->subPri,256); memcpy(s_mode1SubPix,e->subPix,256);
        memcpy(s_mode1SubLayer,e->subLayer,256);
      }
    }
#if defined(TA_R32_CACHE_PROFILE) && TA_R32_CACHE_PROFILE
    ++s_taR32Hits;
#endif
#if defined(TA_R70_MODE1_LINE_CACHE) && TA_R70_MODE1_LINE_CACHE && defined(TA_R70_CACHE_PROFILE) && TA_R70_CACHE_PROFILE
    if(mode1) ++s_taR70Mode1Hits;
#endif
    return true;
  }
  TA_R87_SCOPE(r87Miss,TA87_MISS);
#if defined(TA_R43_LINE_OUTPUT_CACHE) && TA_R43_LINE_OUTPUT_CACHE
  /* A BG rebuild also invalidates final output, including rebuilds while the
   * color configuration temporarily uses another renderer branch. */
  s_taR43Output[row].valid=false;
#endif
#if defined(TA_R86_MODE2_DIRECT_KEY) && TA_R86_MODE2_DIRECT_KEY
  if(r86Mode2) {
    /* Reconstruct exactly the old zero-padded key only on a cache miss. */
    memset(&key,0,sizeof key);
    memcpy(key.bg,ppu->bgLayer,sizeof key.bg);
    memcpy(key.layer,ppu->layer,sizeof key.layer);
    memcpy(key.window,ppu->windowLayer,sizeof key.window);
    key.edges[0]=ppu->window1left; key.edges[1]=ppu->window1right;
    key.edges[2]=ppu->window2left; key.edges[3]=ppu->window2right;
    key.needSub=needSub; key.trackLayer=trackLayer;
  }
#endif
#if defined(TA_R123_REVERSE_MODE2_DIRTY) && TA_R123_REVERSE_MODE2_DIRTY
  ppu_taR123UnregisterMode2(e);
#endif
  e->valid=false; e->key=key; e->count=0;
#if defined(TA_R70_MODE1_LINE_CACHE) && TA_R70_MODE1_LINE_CACHE
  if(mode1) s_taR70Mode1Extras[row]=extra;
#if defined(TA_R70_CACHE_PROFILE) && TA_R70_CACHE_PROFILE
  if(mode1) ++s_taR70Mode1Misses;
#endif
#endif
  memset(s_taR32Seen,0,sizeof s_taR32Seen);
  s_taR32Recording=e;
#if defined(TA_R32_CACHE_PROFILE) && TA_R32_CACHE_PROFILE
  ++s_taR32Misses;
#endif
  return false;
}
static void ppu_taR32StoreBG(bool needSub, bool trackLayer) {
  TaR32BGLine* e=s_taR32Recording;
  if(!e) return;
  TA_R87_SCOPE(r87Store,TA87_STORE);
  ta_r87_cache_event(TA87_EVENT_BG_STORE);
#if defined(TA_R88_PACKED_BG_COPY) && TA_R88_PACKED_BG_COPY
  if((uintptr_t)e>=(uintptr_t)&s_taR32Lines[0] &&
     (uintptr_t)e<(uintptr_t)&s_taR32Lines[PPU_LINES]) {
    /* The recording pointer belongs to the Mode 2 cache. Preserve all
     * byte-visible fields, including the untouched optional layers. */
    memcpy(e->mainPri,s_mode1MainPri,trackLayer?768u:512u);
    if(needSub) memcpy(e->subPri,s_mode1SubPri,768u);
  } else
#endif
  {
    memcpy(e->mainPri,s_mode1MainPri,256); memcpy(e->mainPix,s_mode1MainPix,256);
    if(trackLayer) memcpy(e->mainLayer,s_mode1MainLayer,256);
    if(needSub) {
      memcpy(e->subPri,s_mode1SubPri,256); memcpy(e->subPix,s_mode1SubPix,256);
      memcpy(e->subLayer,s_mode1SubLayer,256);
    }
  }
#if defined(TA_R71_MODE2_VRAM_STAMP) && TA_R71_MODE2_VRAM_STAMP
  e->validatedVramStamp=s_taR71VramStamp;
#endif
#if defined(TA_R123_REVERSE_MODE2_DIRTY) && TA_R123_REVERSE_MODE2_DIRTY
  ppu_taR123RegisterMode2(e);
#endif
  e->valid=true; s_taR32Recording=NULL;
}
#endif

#include "ppu_ta_r110_layer_cache.h"

#if defined(TA_R34_WINDOW_OUTPUT) && TA_R34_WINDOW_OUTPUT &&     defined(TA_R28_MODE2_MASKED_SUBMATH) && TA_R28_MODE2_MASKED_SUBMATH &&     defined(TA_R26_COMPACT_SUBMATH) && TA_R26_COMPACT_SUBMATH &&     defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
/* R34: window decisions are already available as eight exact 32-bit masks.
 * A zero word needs palette output only: no layer/subscreen/color-math work.
 * In mixed words, shift the register mask instead of indexing a mask per pixel.
 * Keep the same R28 eligibility guard and exact R26 five-bit color math. */
#if defined(TA_R35_PROFILE) && TA_R35_PROFILE
static uint32_t s_taR35WindowWords[3];
void ppu_taR35WindowStats(uint32_t counts[3], bool reset) {
  memcpy(counts,s_taR35WindowWords,sizeof s_taR35WindowWords);
  if(reset) memset(s_taR35WindowWords,0,sizeof s_taR35WindowWords);
}
#endif
static inline __attribute__((always_inline)) uint32_t
ppu_taR34MathPixel(const Ppu* ppu, int x, unsigned mainPixel) {
  const unsigned layer = mainPixel ? s_taR94ReadMainLayer[x] : 5;
  if(layer >= 6 || !ppu->mathEnabled[layer]) return s_mode1ColorXrgb[mainPixel];
  const unsigned subPixel = s_mode1ReadSubPix[x];
  const unsigned subLayer = subPixel ? s_mode1ReadSubLayer[x] : 5;
  return subLayer != 5 ? ppu_taR26SubMathColor(ppu,mainPixel,subPixel)
                       : s_mode1SubFixedXrgb[mainPixel];
}
#include "ppu_ta_r99_color.h"
#if defined(TA_R125_MODE2_DIRECT_FIXED) && TA_R125_MODE2_DIRECT_FIXED
#if !TA_R50_PACKED_COLOR || !TA_R34_WINDOW_OUTPUT || TA_R109_FUSED_MODE2_COMPOSE
#error R125 requires R50/R34 and the unfused reference sprite pipeline
#endif
#endif

#if defined(TA_R50_PACKED_COLOR) && TA_R50_PACKED_COLOR
/* At brightness 15 each cached byte is exactly (v<<3)|(v>>2).
 * Recover the three original 5-bit lanes, subtract with one guard bit per
 * lane, zero negative lanes, and expand the surviving lanes together.
 * No new palette/cache and no carry or borrow crosses a channel boundary. */
static inline uint32_t ppu_taR50Subtract(uint32_t a,uint32_t b) {
  const uint32_t lanes=((a>>3)&0x001f1f1fu)|0x00202020u;
  const uint32_t diff=lanes-((b>>3)&0x001f1f1fu);
  const uint32_t valid=((diff>>5)&0x00010101u)*31u;
  const uint32_t five=diff&valid;
  return (five<<3)|((five>>2)&0x00070707u);
}
static inline uint32_t ppu_taR50MathPixel(const Ppu* ppu,unsigned x,unsigned pixel,uint32_t fixedRgb) {
  const unsigned layer=pixel?s_taR94ReadMainLayer[x]:5u;
  if(layer>=6u || !ppu->mathEnabled[layer])return s_mode1ColorXrgb[pixel];
  const unsigned sub=s_mode1ReadSubPix[x];
  const unsigned subLayer=sub?s_mode1ReadSubLayer[x]:5u;
  if(subLayer==5u) {
#if defined(TA_R125_MODE2_DIRECT_FIXED) && TA_R125_MODE2_DIRECT_FIXED
    /* Same exact full-brightness packed subtraction as a subscreen pixel.
     * Backdrop means current fixed color, never CGRAM[0]. */
    return ppu_taR50Subtract(s_mode1ColorXrgb[pixel],fixedRgb);
#else
    return s_mode1SubFixedXrgb[pixel];
#endif
  }
#if defined(TA_R99_PALETTE_LANES) && TA_R99_PALETTE_LANES
  return ppu_taR99Subtract(s_taR99Color5[pixel],s_taR99Color5[sub]);
#else
  return ppu_taR50Subtract(s_mode1ColorXrgb[pixel],s_mode1ColorXrgb[sub]);
#endif
}
static void ppu_taR50PackedOutput(const Ppu* ppu,uint32_t* out,unsigned stride) {
#if defined(TA_R125_MODE2_DIRECT_FIXED) && TA_R125_MODE2_DIRECT_FIXED
  const uint32_t fixed5=(uint32_t)ppu->fixedColorB |
      ((uint32_t)ppu->fixedColorG<<8) | ((uint32_t)ppu->fixedColorR<<16);
  const uint32_t fixedRgb=(fixed5<<3)|((fixed5>>2)&0x00070707u);
#else
  const uint32_t fixedRgb=0;
#endif
#if defined(TA_R100_NATIVE_PIXEL_LOOP) && TA_R100_NATIVE_PIXEL_LOOP
  /* R100: Mode-2 native 256px output uses stride 1. Separate its hot loop
   * once per line rather than checking width and stepping by stride on all
   * 256 pixels. Only already-resolved line-local masks and palette data
   * are read; no event/VRAM state is cached and the original stride-2 path
   * remains below, byte-for-byte. TA_R100_NATIVE_PIXEL_LOOP=0 rolls back. */
  if(stride==1u) {
    for(unsigned word=0;word<8u;++word) {
      uint32_t mask=s_taWindowMask[5][word];
      const unsigned end=(word+1u)*32u;
      if(!mask) {
        for(unsigned x=word*32u;x<end;++x)
          out[x]=s_mode1ColorXrgb[s_taR94ReadMainPix[x]];
      } else if(mask==0xffffffffu) {
        for(unsigned x=word*32u;x<end;++x)
          out[x]=ppu_taR50MathPixel(ppu,x,s_taR94ReadMainPix[x],fixedRgb);
      } else {
        for(unsigned x=word*32u;x<end;++x,mask>>=1) {
          const unsigned pixel=s_taR94ReadMainPix[x];
          out[x]=(mask&1u)?ppu_taR50MathPixel(ppu,x,pixel,fixedRgb):s_mode1ColorXrgb[pixel];
        }
      }
    }
    return;
  }
#endif
  for(unsigned word=0;word<8;++word) {
    uint32_t mask=s_taWindowMask[5][word];
    const unsigned end=(word+1u)*32u;
    if(!mask) {
      for(unsigned x=word*32u;x<end;++x) {
        const uint32_t c=s_mode1ColorXrgb[s_taR94ReadMainPix[x]];
        out[0]=c;if(stride==2u)out[1]=c;out+=stride;
      }
    } else if(mask==0xffffffffu) {
      for(unsigned x=word*32u;x<end;++x) {
        const uint32_t c=ppu_taR50MathPixel(ppu,x,s_taR94ReadMainPix[x],fixedRgb);
        out[0]=c;if(stride==2u)out[1]=c;out+=stride;
      }
    } else {
      for(unsigned x=word*32u;x<end;++x,mask>>=1) {
        const unsigned pixel=s_taR94ReadMainPix[x];
        const uint32_t c=(mask&1u)?ppu_taR50MathPixel(ppu,x,pixel,fixedRgb):s_mode1ColorXrgb[pixel];
        out[0]=c;if(stride==2u)out[1]=c;out+=stride;
      }
    }
  }
}
#endif
static inline __attribute__((always_inline)) void
ppu_taR34WindowOutput(const Ppu* ppu, uint32_t* out, unsigned stride) {
#if defined(TA_R50_PACKED_COLOR) && TA_R50_PACKED_COLOR
  if(ppu->brightness==15u){ppu_taR50PackedOutput(ppu,out,stride);return;}
#endif
  for(unsigned word=0;word<8;++word) {
    uint32_t mask=s_taWindowMask[5][word];
    const unsigned end=(word+1)*32;
    if(!mask) {
#if defined(TA_R35_PROFILE) && TA_R35_PROFILE
      ++s_taR35WindowWords[0];
#endif
      for(unsigned x=word*32;x<end;++x) {
        const uint32_t color=s_mode1ColorXrgb[s_taR94ReadMainPix[x]];
        out[0]=color; if(stride==2) out[1]=color; out+=stride;
      }
    } else if(mask==0xffffffffu) {
#if defined(TA_R35_PROFILE) && TA_R35_PROFILE
      ++s_taR35WindowWords[1];
#endif
      for(unsigned x=word*32;x<end;++x) {
        const uint32_t color=ppu_taR34MathPixel(ppu,x,s_taR94ReadMainPix[x]);
        out[0]=color; if(stride==2) out[1]=color; out+=stride;
      }
    } else {
#if defined(TA_R35_PROFILE) && TA_R35_PROFILE
      ++s_taR35WindowWords[2];
#endif
      for(unsigned x=word*32;x<end;++x,mask>>=1) {
        const unsigned pixel=s_taR94ReadMainPix[x];
        const uint32_t color=(mask&1u) ? ppu_taR34MathPixel(ppu,x,pixel)
                                      : s_mode1ColorXrgb[pixel];
        out[0]=color; if(stride==2) out[1]=color; out+=stride;
      }
    }
  }
}
#endif

/* R85: guarded Mode-1 scalar-output palette/channel lookup.
 * Only the already-prepared exact R17 256-bit color-window masks are used.
 * Clip acts on the MAIN color before math (including the subscreen case).
 * Preserve the original scalar compositor as a compile-time and runtime
 * fallback for other modes or unavailable masks. */
#if defined(TA_R85_MODE1_SCALAR_LUT) && TA_R85_MODE1_SCALAR_LUT && \
    defined(TA_R26_COMPACT_SUBMATH) && TA_R26_COMPACT_SUBMATH && \
    defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
static inline uint32_t ppu_taR85BlackSubMath(const Ppu* ppu,unsigned sub) {
  const uint16_t c=ta_panel_color(ppu->cgram,sub,ppu->mode);
  const uint8_t* ch=s_taR26MathChannel;
  return (uint32_t)ch[(c>>10)&31u] |
      ((uint32_t)ch[(c>>5)&31u]<<8) |
      ((uint32_t)ch[c&31u]<<16);
}
static void ppu_taR85ScalarLine(Ppu* ppu,uint32_t* out,bool nativeOut,
                               const uint8_t* bright,bool anyMath) {
  if(anyMath) {
    if(ppu->addSubscreen) ppu_xboxBuildSubMathTables(ppu);
    else ppu_xboxBuildMathPalette(ppu);
  }
  /* Math on a clipped main color starts at black, NOT CGRAM[0].
   * Subscreen backdrop uses fixed color without the half-color step. */
  int cr=ppu->subtractColor ? -(int)ppu->fixedColorR : (int)ppu->fixedColorR;
  int cg=ppu->subtractColor ? -(int)ppu->fixedColorG : (int)ppu->fixedColorG;
  int cb=ppu->subtractColor ? -(int)ppu->fixedColorB : (int)ppu->fixedColorB;
  if(!ppu->addSubscreen && ppu->halfColor) {cr>>=1;cg>>=1;cb>>=1;}
  const uint32_t clipMath=(uint32_t)bright[ppu_clampColorFast(cb)] |
      ((uint32_t)bright[ppu_clampColorFast(cg)]<<8) |
      ((uint32_t)bright[ppu_clampColorFast(cr)]<<16);
  const unsigned clipMode=ppu->clipMode, prevent=ppu->preventMathMode;
  const bool useSub=ppu->addSubscreen;
  const unsigned stride=nativeOut?1u:2u;
  for(unsigned word=0;word<8;++word) {
    uint32_t mask=s_taWindowMask[5][word];
    const unsigned end=(word+1u)*32u;
    for(unsigned x=word*32u;x<end;++x,mask>>=1) {
      const bool win=(mask&1u)!=0;
      const bool clipped=clipMode==3u || (clipMode==2u && win) ||
          (clipMode==1u && !win);
      const unsigned pixel=s_taR94ReadMainPix[x];
      const unsigned layer=pixel?s_taR94ReadMainLayer[x]:5u;
      const bool math=layer<6u && ppu->mathEnabled[layer] &&
          !(prevent==3u || (prevent==2u && win) || (prevent==1u && !win));
      uint32_t color;
      if(!math) color=clipped?0u:s_mode1ColorXrgb[pixel];
      else if(!useSub) color=clipped?clipMath:s_mode1MathXrgb[pixel];
      else {
        const unsigned sub=s_mode1SubPix[x];
        const unsigned subLayer=sub?s_mode1SubLayer[x]:5u;
        if(subLayer!=5u) color=clipped?ppu_taR85BlackSubMath(ppu,sub)
                                          :ppu_taR26SubMathColor(ppu,pixel,sub);
        else color=clipped?clipMath:s_mode1SubFixedXrgb[pixel];
      }
      out[0]=color;
      if(stride==2u) out[1]=color;
      out+=stride;
    }
  }
}
#endif

#include "ppu_ta_r127_indexed.h"
#include "ppu_ta_r129_direct.h"
#include "ppu_ta_r93_spans.h"
#include "ppu_ta_r109_compose.h"

static void ppu_xboxMode1RenderLine(Ppu* ppu, int y) {
#if defined(TA_R109_FUSED_MODE2_COMPOSE) && TA_R109_FUSED_MODE2_COMPOSE
  s_taR109Fuse=false;s_taR109MainPri=s_mode1MainPri;s_taR109SubPri=s_mode1SubPri;
#endif
  /* Reset even on forced blank or paths that never consult the BG cache. */
  ppu_taR93ResetSubView();
  ppu_taR94ResetMainView();
#if TA_R83_RESIDUAL_PROFILE
  /* Native line-composition cost exclusive of nested BG/sprite work. */
  TA_R82_SCOPE(TA82_NATIVE_COMPOSE);
#endif
  TA_R84_SCOPE(r84Setup,TA84_SETUP);
  const int nativeRow = y - 1;
  const int legacyRow = nativeRow + (ppu->evenFrame ? 0 : 239);
  const bool externalOut = s_xboxFrameTarget != NULL && nativeRow >= 0 && nativeRow < PPU_LINES;
#if defined(TA_R39_NATIVE256) && TA_R39_NATIVE256
  const bool packedOut = !externalOut && (unsigned)legacyRow < 478u;
#else
  const bool packedOut = false;
#endif
  const bool nativeOut = externalOut || packedOut;
  uint32_t* out = externalOut
      ? (uint32_t*)(s_xboxFrameTarget + nativeRow * s_xboxFramePitch)
      : (uint32_t*)&ppu->pixelBuffer[legacyRow * PPU_ROW_BYTES];
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  if(externalOut && s_r126Managed){
    if(s_r126CaptureValid && s_r126CaptureEven!=ppu->evenFrame){
      /* A DMA can cross vblank and return a partial NEXT field in one
       * runFrame. Preserve old aliases before target rows are overwritten. */
      ppu_r126ResolveAll(ppu);memset(s_xboxLineNative,0,sizeof s_xboxLineNative);memset(s_r126GpuRows,0,sizeof s_r126GpuRows);
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
      memset(s_r127Rows,0,sizeof s_r127Rows);
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
      memset(s_r129Rows,0,sizeof s_r129Rows);
#endif
#endif
    }
    s_r126CaptureValid=true;s_r126CaptureEven=ppu->evenFrame;
  }
#endif
  if(externalOut) s_xboxLineNative[nativeRow] = 1;
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  if((unsigned)legacyRow<478u){
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
    s_r127Alias[legacyRow]=NULL;
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
    s_r129Alias[legacyRow]=false;
    if(externalOut && (unsigned)nativeRow<240u)s_r129Rows[nativeRow]=false;
#endif
    if(externalOut && (unsigned)nativeRow<240u)s_r127Rows[nativeRow]=false;
#endif
    if(externalOut && s_r126Managed){s_r126External[legacyRow]=out;s_r126Second[legacyRow]=NULL;ppu->taNative256Row[legacyRow]=1;}
    else if(s_r126Owner==ppu){s_r126External[legacyRow]=s_r126Second[legacyRow]=NULL;}
  }
#endif
  if(packedOut) ppu->taNative256Row[legacyRow] = 1;

  if(ppu->forcedBlank) {
    TA_R84_SCOPE(r84Blank,TA84_BLANK);
    memset(out, 0, nativeOut ? 256 * 4 : 256 * 8);
    return;
  }

#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
  const bool r28Sample = (ppu->mode == 1 || ppu->mode == 2) && ((unsigned)y & 31u) == 0;
  const uint64_t r28t0 = r28Sample ? __builtin_readcyclecounter() : 0;
  uint64_t r28t1=0, r28t2=0, r28t3=0, r28t4=0;
  /* Every direct/fixed/submath/generic line is categorized. */
#define TA_R28_ENDLINE(PATH) do { \
  if(ppu->mode == 1 || ppu->mode == 2) ++s_taR28ColorPath[ppu->mode-1][(PATH)]; \
  if(r28Sample) ppu_taR28Record(ppu->mode,r28t0,r28t1,r28t2,r28t3,r28t4); \
} while(0)
#else
#define TA_R28_ENDLINE(PATH) ((void)0)
#endif
#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
  s_taR29BGSample=(ppu->mode==2) && (((unsigned)y & 31u)==0u);
#endif
#if defined(TA_R43_LINE_OUTPUT_CACHE) && TA_R43_LINE_OUTPUT_CACHE && defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
  s_taR43BgHit=false;s_taR43OutputComplete=false;s_taR43OutputTried=false;
  s_taR43OutputTarget=NULL;
#if defined(TA_R34_WINDOW_OUTPUT) && TA_R34_WINDOW_OUTPUT && defined(TA_R28_MODE2_MASKED_SUBMATH) && TA_R28_MODE2_MASKED_SUBMATH && defined(TA_R26_COMPACT_SUBMATH) && TA_R26_COMPACT_SUBMATH && defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
  if(nativeOut && ppu->mode==2 && ppu->clipMode==0 && ppu->preventMathMode==1 &&
     ppu->addSubscreen && ppu->subtractColor && !ppu->halfColor)
    s_taR43OutputTarget=out;
#endif
#endif
  ppu_xboxBuildPalette(ppu);

#if !defined(TA_R32_BG_LINE_CACHE) || !TA_R32_BG_LINE_CACHE
  memset(s_mode1MainPri, 0, sizeof(s_mode1MainPri));
  memset(s_mode1MainPix, 0, sizeof(s_mode1MainPix));
#endif
  bool anyMath = false;
  for(int i = 0; i < 6; ++i) anyMath = anyMath || ppu->mathEnabled[i];
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
  if(ppu->mode == 2) {
    const unsigned cfg = (ppu->clipMode & 3u) | ((ppu->preventMathMode & 3u) << 2) |
          ((unsigned)!!ppu->addSubscreen << 4) | ((unsigned)!!ppu->halfColor << 5) |
          ((unsigned)!!ppu->subtractColor << 6) | ((unsigned)!!anyMath << 7);
    ++s_taR28ColorConfig[cfg];
  }
#endif
  const bool trackLayer = anyMath && ppu->preventMathMode != 3;
  const bool needSub = trackLayer && ppu->addSubscreen;
#if !defined(TA_R32_BG_LINE_CACHE) || !TA_R32_BG_LINE_CACHE
  if(needSub) {
    memset(s_mode1SubPri, 0, sizeof(s_mode1SubPri));
    memset(s_mode1SubPix, 0, sizeof(s_mode1SubPix));
  }
#endif

  /* SNES mode-2 priority order (low -> high): BG2 low, OBJ0,
     BG1 low, OBJ1, BG2 high, OBJ2, BG1 high, OBJ3.
     Mode 1 retains its original exact priority ordering. */
  static const uint8_t zbgMode2[3][2] = {{3,7},{1,5},{0,0}};
  static const uint8_t zsprMode2[4] = {2,4,6,8};
  static const uint8_t zbgNormal[3][2] = {{6,9},{5,8},{1,3}};
  static const uint8_t zsprNormal[4] = {2,4,7,10};
  static const uint8_t zbgBg3Hi[3][2] = {{5,8},{4,7},{1,10}};
  static const uint8_t zsprBg3Hi[4] = {2,3,6,9};
  const uint8_t (*zbg)[2] = ppu->mode == 2 ? zbgMode2 :
      (ppu->bg3priority ? zbgBg3Hi : zbgNormal);
  const uint8_t* zspr = ppu->mode == 2 ? zsprMode2 :
      (ppu->bg3priority ? zsprBg3Hi : zsprNormal);

#if defined(TA_R12_OFFSET_CACHE) && TA_R12_OFFSET_CACHE
  /* Invalidate at the start of EACH line: HDMA/VRAM may have changed. */
  if(ppu->mode == 2) s_taMode2OffsetMask = 0;
#if defined(TA_R67_BG_FAST) && TA_R67_BG_FAST
  s_taR67OffsetsReady=false;
#endif
#endif
  for(int i = 0; i < 6; ++i) ppu->fastWindowCacheX[i] = -0x40000000;
#if defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
  ppu_taPrepareWindowMasks(ppu);
#endif
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
  if(r28Sample) r28t1=__builtin_readcyclecounter();
#endif
#if defined(TA_R124_MODE2_STREAM) && TA_R124_MODE2_STREAM
  if(ppu->mode==2)TA_R124_COUNT(0);
#endif
#if defined(TA_R109_FUSED_MODE2_COMPOSE) && TA_R109_FUSED_MODE2_COMPOSE
  if(ppu->mode==2)TA_R109_COUNT(0);
  s_taR109Fuse=nativeOut && ppu->mode==2 && trackLayer && needSub &&
      ppu->brightness==15 && ppu->clipMode==0 && ppu->preventMathMode==1 &&
      ppu->subtractColor && !ppu->halfColor && s_taWindowMaskReady;
#endif
  TA_R84_FINISH(r84Setup);
  {
  /* Exclusive cache bookkeeping; nested BG/sprite work is subtracted. */
  TA_R84_SCOPE(r84Cache,TA84_CACHE);
#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
#if defined(TA_R70_MODE1_LINE_CACHE) && TA_R70_MODE1_LINE_CACHE
  if(!ppu_taR32RestoreBG(ppu,nativeRow,needSub,trackLayer)) {
#else
  if(ppu->mode!=2 || !ppu_taR32RestoreBG(ppu,nativeRow,needSub,trackLayer)) {
#endif
#if defined(TA_R110_MODE2_FIRST_BG_CACHE) && TA_R110_MODE2_FIRST_BG_CACHE
    if(ppu->mode==2) ppu_taR110DrawFirstBG(ppu,nativeRow,y,zbg,needSub,trackLayer);
    else {
#endif
#if defined(TA_R124_MODE2_STREAM) && TA_R124_MODE2_STREAM
    if(ppu->mode==2 && trackLayer && s_taWindowMaskReady) ppu_taR124BuildBG(ppu,y,zbg,needSub,trackLayer);
    else {
#endif
    memset(s_mode1MainPri,0,256); memset(s_mode1MainPix,0,256);
    if(needSub) {memset(s_mode1SubPri,0,256);memset(s_mode1SubPix,0,256);}
#endif
  ppu_xboxMode1DrawLayerR68(ppu, 0, y, zbg, needSub, trackLayer);
#if defined(TA_R110_MODE2_FIRST_BG_CACHE) && TA_R110_MODE2_FIRST_BG_CACHE
    }
#endif
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
  if(r28Sample) r28t2=__builtin_readcyclecounter();
#endif
  ppu_xboxMode1DrawLayerR68(ppu, 1, y, zbg, needSub, trackLayer);
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
  if(r28Sample) r28t3=__builtin_readcyclecounter();
#endif
  if(ppu->mode == 1) ppu_xboxMode1DrawLayerR68(ppu, 2, y, zbg, needSub, trackLayer);
#if defined(TA_R124_MODE2_STREAM) && TA_R124_MODE2_STREAM
    }
#endif
#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
    ppu_taR32StoreBG(needSub,trackLayer);
  }
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
  if(r28Sample && s_taR32Recording==NULL) {
    /* Cached BG timing belongs to BG1; preserve monotonic stage timestamps. */
    if(!r28t2) r28t2=__builtin_readcyclecounter();
    if(!r28t3) r28t3=r28t2;
  }
#endif
#endif
#if defined(ZAMN_R23_MMX_PPU)
  __asm__ volatile("emms" ::: "memory");
#endif
#if defined(TA_R43_LINE_OUTPUT_CACHE) && TA_R43_LINE_OUTPUT_CACHE && defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
  if(s_taR43OutputComplete) {
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
    if(r28Sample) r28t2=r28t3=r28t4=__builtin_readcyclecounter();
#endif
    TA_R28_ENDLINE(3);
    return;
  }
#endif
#if defined(TA_R109_FUSED_MODE2_COMPOSE) && TA_R109_FUSED_MODE2_COMPOSE
  if(!s_taR109Fuse)
#endif
  ppu_xboxMode1DrawSprites(ppu, zspr, needSub, trackLayer);
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
  if(r28Sample) r28t4=__builtin_readcyclecounter();
#endif

  } /* r84Cache */

#if defined(TA_R109_FUSED_MODE2_COMPOSE) && TA_R109_FUSED_MODE2_COMPOSE
  if(s_taR109Fuse) {
    TA_R84_SCOPE(r109Output,TA84_WINDOW);
    {TA_R84_SCOPE(r109Palette,TA84_SETUP);ppu_xboxBuildSubMathTables(ppu);}
    ppu_taR109Output(ppu,out);TA_R109_COUNT(1);TA_R28_ENDLINE(3);return;
  }
#endif
  const uint8_t* bright = s_brightnessLut[ppu->brightness & 0xf];

  /* Common case: no color math and no clipping.  One z-buffer read + one
     cached palette lookup per pixel, exactly like the native SMW renderer. */
  if((!anyMath || ppu->preventMathMode == 3) && ppu->clipMode == 0) {
    TA_R84_SCOPE(r84Direct,TA84_DIRECT);
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
    if(ppu->mode==2 && externalOut && s_r126Managed && s_r129Enabled && s_r127Enabled &&
       ppu_taR129Capture(ppu,out,nativeRow,legacyRow)){
      TA_R28_ENDLINE(0);return;
    }
#endif
    if(nativeOut) {
      for(int x = 0; x < 256; ++x)
        out[x] = ppu_xboxMode1DirectColor(s_taR94ReadMainPix[x]);
    } else {
      for(int x = 0; x < 256; ++x) {
        const uint32_t color = ppu_xboxMode1DirectColor(s_taR94ReadMainPix[x]);
        out[x * 2 + 0] = color;
        out[x * 2 + 1] = color;
      }
    }
    TA_R28_ENDLINE(0);
    return;
  }

  /* R23 common gameplay path: fixed-colour maths with no colour-window gate
     is just one of two precomputed palettes selected by the winning layer. */
  if(anyMath && !ppu->addSubscreen && ppu->preventMathMode == 0 && ppu->clipMode == 0) {
    TA_R84_SCOPE(r84Fixed,TA84_FIXED);
    { TA_R84_SCOPE(r84Palette,TA84_SETUP); ppu_xboxBuildMathPalette(ppu); }
    TA_DIAG(++s_xboxFixedMathLines);
    if(nativeOut) {
      for(int x = 0; x < 256; ++x) {
        const int pixel = s_taR94ReadMainPix[x];
        const int layer = pixel ? s_taR94ReadMainLayer[x] : 5;
        out[x] = (layer < 6 && ppu->mathEnabled[layer])
            ? s_mode1MathXrgb[pixel] : s_mode1ColorXrgb[pixel];
      }
    } else {
      for(int x = 0; x < 256; ++x) {
        const int pixel = s_taR94ReadMainPix[x];
        const int layer = pixel ? s_taR94ReadMainLayer[x] : 5;
        const uint32_t color = (layer < 6 && ppu->mathEnabled[layer])
            ? s_mode1MathXrgb[pixel] : s_mode1ColorXrgb[pixel];
        out[x * 2 + 0] = color;
        out[x * 2 + 1] = color;
      }
    }
    TA_R28_ENDLINE(1);
    return;
  }

  /* R24 common gameplay path: add-subscreen colour math with no colour
     window/clipping. The sub-screen winner is already resolved into its compact
     z-buffer, so the expensive channel arithmetic reduces to one pair-table
     lookup (or a 256-entry fixed-colour fallback when the sub winner is the
     backdrop). */
  if(anyMath && ppu->addSubscreen && ppu->preventMathMode == 0 && ppu->clipMode == 0) {
    TA_R84_SCOPE(r84Sub,TA84_SUB);
    { TA_R84_SCOPE(r84Palette,TA84_SETUP); ppu_xboxBuildSubMathTables(ppu); }
    TA_DIAG(++s_xboxSubMathLines);
    if(nativeOut) {
      for(int x = 0; x < 256; ++x) {
        const int mainPixel = s_taR94ReadMainPix[x];
        const int mainLayer = mainPixel ? s_taR94ReadMainLayer[x] : 5;
        if(mainLayer < 6 && ppu->mathEnabled[mainLayer]) {
          const int subPixel = s_mode1ReadSubPix[x];
          const int secondLayer = subPixel ? s_mode1ReadSubLayer[x] : 5;
          out[x] = secondLayer != 5
              ? 
#if defined(TA_R26_COMPACT_SUBMATH) && TA_R26_COMPACT_SUBMATH
              ppu_taR26SubMathColor(ppu, mainPixel, subPixel)
#else
              s_mode1SubMathXrgb[(mainPixel << 8) | subPixel]
#endif
              : s_mode1SubFixedXrgb[mainPixel];
        } else {
          out[x] = s_mode1ColorXrgb[mainPixel];
        }
      }
    } else {
      for(int x = 0; x < 256; ++x) {
        const int mainPixel = s_taR94ReadMainPix[x];
        const int mainLayer = mainPixel ? s_taR94ReadMainLayer[x] : 5;
        const uint32_t color = (mainLayer < 6 && ppu->mathEnabled[mainLayer])
            ? ((s_mode1ReadSubPix[x] && s_mode1ReadSubLayer[x] != 5)
                ? 
#if defined(TA_R26_COMPACT_SUBMATH) && TA_R26_COMPACT_SUBMATH
                ppu_taR26SubMathColor(ppu, mainPixel, s_mode1ReadSubPix[x])
#else
                s_mode1SubMathXrgb[(mainPixel << 8) | s_mode1ReadSubPix[x]]
#endif
                : s_mode1SubFixedXrgb[mainPixel])
            : s_mode1ColorXrgb[mainPixel];
        out[x * 2 + 0] = color;
        out[x * 2 + 1] = color;
      }
    }
    TA_R28_ENDLINE(2);
    return;
  }

  /* R28: exact hot Mode-2 configuration found by per-scanline telemetry:
   * clip=0, preventMath=1, addSubscreen=1, subtract=1, half=0.
   * Math is permitted only INSIDE the color window. Use the precomputed
   * R26 five-bit channel table instead of scalar channel operations.
   * Not a different SNES rendering model: source layers, priority winners,
   * window mask, fixed-color backdrop, and palette/brightness are unchanged.
   * If any guard differs, fall back to the original scalar compositor. */
#if defined(TA_R28_MODE2_MASKED_SUBMATH) && TA_R28_MODE2_MASKED_SUBMATH && \
    defined(TA_R26_COMPACT_SUBMATH) && TA_R26_COMPACT_SUBMATH && \
    defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
  if(ppu->mode == 2 && anyMath && ppu->clipMode == 0 &&
     ppu->preventMathMode == 1 && ppu->addSubscreen &&
     ppu->subtractColor && !ppu->halfColor && s_taWindowMaskReady) {
    TA_R84_SCOPE(r84Window,TA84_WINDOW);
#if defined(TA_R126_GPU_SUBTRACT) && TA_R126_GPU_SUBTRACT
    if(externalOut && s_r126Managed && s_r126GpuSub && ppu->brightness==15u){
      #if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
      if(s_r127Enabled && ppu_taR127Capture(ppu,out,nativeRow,legacyRow)){
        TA_R28_ENDLINE(3);return;
      }
#endif
      uint32_t*second=(uint32_t*)(s_r126GpuSub+nativeRow*s_r126GpuPitch);
      const uint32_t fixed=((uint32_t)ppu->fixedColorR<<19)|((uint32_t)ppu->fixedColorG<<11)|((uint32_t)ppu->fixedColorB<<3);
      for(unsigned x=0;x<256;x++){
        unsigned pix=s_taR94ReadMainPix[x],layer=pix?s_taR94ReadMainLayer[x]:5;
        out[x]=s_mode1ColorXrgb[pix]&0xf8f8f8u;
        unsigned sub=s_mode1ReadSubPix[x],sl=sub?s_mode1ReadSubLayer[x]:5;
        second[x]=(layer<6&&ppu->mathEnabled[layer]&&((s_taWindowMask[5][x>>5]>>(x&31))&1u))
          ?(sl!=5?(s_mode1ColorXrgb[sub]&0xf8f8f8u):fixed):0;
      }
      s_r126Second[legacyRow]=second;s_r126GpuRows[nativeRow]=true;
      TA_R28_ENDLINE(3);return;
    }
#endif
#if defined(TA_R125_MODE2_DIRECT_FIXED) && TA_R125_MODE2_DIRECT_FIXED
    /* Full-brightness output uses packed subtraction for BOTH subscreen and
     * fixed-color sources. Neither the 1024-entry channel table nor the
     * 256-entry fixed palette is consumed. Leave their validity unchanged:
     * a later fallback rebuilds from its own current key/invalidation state. */
    if(ppu->brightness!=15u)
#endif
    { TA_R84_SCOPE(r84Palette,TA84_SETUP); ppu_xboxBuildSubMathTables(ppu); }
#if defined(TA_R34_WINDOW_OUTPUT) && TA_R34_WINDOW_OUTPUT
#if defined(TA_R43_LINE_OUTPUT_CACHE) && TA_R43_LINE_OUTPUT_CACHE && defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
    if(nativeOut && !s_taR43OutputTried && ppu_taR43RestoreOutput(ppu,(unsigned)nativeRow,out)) {
      TA_R28_ENDLINE(3);
      return;
    }
#endif
    if(nativeOut) ppu_taR34WindowOutput(ppu,out,1);
    else ppu_taR34WindowOutput(ppu,out,2);
#if defined(TA_R43_LINE_OUTPUT_CACHE) && TA_R43_LINE_OUTPUT_CACHE && defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
    if(nativeOut) ppu_taR43StoreOutput((unsigned)nativeRow,out);
#endif
#else
    for(int x = 0; x < 256; ++x) {
      const int mainPixel = s_taR94ReadMainPix[x];
      const int mainLayer = mainPixel ? s_taR94ReadMainLayer[x] : 5;
      uint32_t color;
      if(mainLayer < 6 && ppu->mathEnabled[mainLayer] &&
         ppu_taNativeWindowState(ppu, 5, x)) {
        const int subPixel = s_mode1ReadSubPix[x];
        const int secondLayer = subPixel ? s_mode1ReadSubLayer[x] : 5;
        color = secondLayer != 5
            ? ppu_taR26SubMathColor(ppu, mainPixel, subPixel)
            : s_mode1SubFixedXrgb[mainPixel];
      } else {
        color = s_mode1ColorXrgb[mainPixel];
      }
      if(nativeOut) out[x]=color;
      else {out[x*2]=color;out[x*2+1]=color;}
    }
#endif
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
    if(ppu->mode == 2) ++s_taR28MaskedLines;
#endif
    TA_R28_ENDLINE(3);
    return;
  }
#endif

  TA_R84_SCOPE(r84Scalar,TA84_SCALAR);
#if defined(TA_R85_MODE1_SCALAR_LUT) && TA_R85_MODE1_SCALAR_LUT && \
    defined(TA_R26_COMPACT_SUBMATH) && TA_R26_COMPACT_SUBMATH && \
    defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
  if(ppu->mode==1 && s_taWindowMaskReady) {
#if defined(TA_R93_WINDOW_SPANS) && TA_R93_WINDOW_SPANS
    ppu_taR93ScalarLine(ppu,out,nativeOut,bright,anyMath);
#else
    ppu_taR85ScalarLine(ppu,out,nativeOut,bright,anyMath);
#endif
    TA_R28_ENDLINE(3);
    return;
  }
#endif
  for(int x = 0; x < 256; ++x) {
    const int mainPixel = s_taR94ReadMainPix[x];
    const int mainLayer = mainPixel ? s_taR94ReadMainLayer[x] : 5;
    const bool colorWindowState = ppu_taNativeWindowState(ppu, 5, x);

    int r = 0, g = 0, b = 0;
    if(!(ppu->clipMode == 3 ||
         (ppu->clipMode == 2 && colorWindowState) ||
         (ppu->clipMode == 1 && !colorWindowState))) {
      const uint16_t c = ta_panel_color(ppu->cgram,mainPixel,ppu->mode);
      r = c & 0x1f;
      g = (c >> 5) & 0x1f;
      b = (c >> 10) & 0x1f;
    }

    const bool mathEnabled = mainLayer < 6 && ppu->mathEnabled[mainLayer] && !(
      ppu->preventMathMode == 3 ||
      (ppu->preventMathMode == 2 && colorWindowState) ||
      (ppu->preventMathMode == 1 && !colorWindowState));

    int secondLayer = 5;
    int r2 = 0, g2 = 0, b2 = 0;
    if(mathEnabled && ppu->addSubscreen) {
      const int subPixel = s_mode1ReadSubPix[x];
      secondLayer = subPixel ? s_mode1ReadSubLayer[x] : 5;
      const uint16_t c2 = ta_panel_color(ppu->cgram,subPixel,ppu->mode);
      r2 = c2 & 0x1f;
      g2 = (c2 >> 5) & 0x1f;
      b2 = (c2 >> 10) & 0x1f;
    }

    if(mathEnabled) {
      if(ppu->subtractColor) {
        if(ppu->addSubscreen && secondLayer != 5) {
          r -= r2; g -= g2; b -= b2;
        } else {
          r -= ppu->fixedColorR; g -= ppu->fixedColorG; b -= ppu->fixedColorB;
        }
      } else {
        if(ppu->addSubscreen && secondLayer != 5) {
          r += r2; g += g2; b += b2;
        } else {
          r += ppu->fixedColorR; g += ppu->fixedColorG; b += ppu->fixedColorB;
        }
      }
      if(ppu->halfColor && (secondLayer != 5 || !ppu->addSubscreen)) {
        r >>= 1; g >>= 1; b >>= 1;
      }
      r = ppu_clampColorFast(r);
      g = ppu_clampColorFast(g);
      b = ppu_clampColorFast(b);
    }

    const uint32_t color = (uint32_t)bright[b] |
                           ((uint32_t)bright[g] << 8) |
                           ((uint32_t)bright[r] << 16);
    if(nativeOut) out[x] = color;
    else {
      out[x * 2 + 0] = color;
      out[x * 2 + 1] = color;
    }
  }
  TA_R28_ENDLINE(3);
#undef TA_R28_ENDLINE
}

/* R3: Mode 2 uses two 4bpp backgrounds and sprites. The native tile
   compositor resolves BG3 offset-per-tile data per eight-pixel chunk.
   Unsupported picture modes return to the original reference renderer. */
static inline bool ppu_taCanFastMode2(Ppu* ppu) {
  if(ppu->mode != 2 || ppu->extraLeft || ppu->extraRight ||
     ppu->pseudoHires || ppu->pixelOutputFormat != ppu_pixelOutputFormatXBGR)
    return false;
#if defined(ZAMN_R32_PPU_HOISTS)
  /* The original R32 Mode-1 row hoist assumes fixed Y across all tiles. */
  return false;
#endif
  if(ppu->mosaicSize > 1 &&
     (ppu->bgLayer[0].mosaicEnabled || ppu->bgLayer[1].mosaicEnabled))
    return false;
  /* OPT offsets are decoded in ppu_xboxMode1DrawLayer per tile. */
  return true;
}

static inline bool ppu_xboxCanFastMode1(const Ppu* ppu) {
  if(ppu->mode != 1 || ppu->extraLeft != 0 || ppu->extraRight != 0 ||
     ppu->pseudoHires || ppu->pixelOutputFormat != ppu_pixelOutputFormatXBGR)
    return false;
  if(ppu->mosaicSize > 1 && (ppu->bgLayer[0].mosaicEnabled ||
                             ppu->bgLayer[1].mosaicEnabled ||
                             ppu->bgLayer[2].mosaicEnabled))
    return false;
  return true;
}

bool ppu_xboxComposeMode1Line(Ppu* ppu, int line) {
  if(!ppu) return false;
  if(ppu->mode == 2) {
#if defined(TA_R3_MODE2_FAST) && TA_R3_MODE2_FAST
    if(!ppu_taCanFastMode2(ppu)) {
      TA_DIAG(++s_taMode2RejectedLines);
      return false;
    }
    TA_DIAG(++s_taMode2FastLines);
#else
    return false;
#endif
  } else {
    if(!ppu_xboxCanFastMode1(ppu)) return false;
    TA_DIAG(++s_xboxMode1FastLines);
  }
  ppu_xboxMode1RenderLine(ppu, line);
  return true;
}

bool ppu_xboxRenderNativeLine(Ppu* ppu, int line) {
  if(!ppu) return false;
  // noPixels still evaluates OAM on the SNES; own that side effect even though
  // there is no colour output for the line. For a visible line, take ownership
  // only when the exact Xbox Mode-1 compositor can handle the current state.
  if(!ppu->noPixels && !ppu_xboxCanFastMode1(ppu) &&
     !ppu_taCanFastMode2(ppu)) return false;

  memset(ppu->objPixelBuffer, 0, (size_t)ppu_gameWidth(ppu));
  if(!ppu->forcedBlank) {
#if defined(ZAMN_R23_MMX_PPU)
    if(ppu->extraLeft == 0 && ppu->extraRight == 0)
      ppu_evaluateSpritesXboxNarrow(ppu, line - 1);
    else
      ppu_evaluateSprites(ppu, line - 1);
#else
    ppu_evaluateSprites(ppu, line - 1);
#endif
  }
  if(ppu->noPixels) return true;
  return ppu_xboxComposeMode1Line(ppu, line);
}

#endif

void ppu_runLine(Ppu* ppu, int line) {
  TA_R82_SCOPE(TA82_PPU);
  // called for lines 1-224/239
  // Where the backgrounds are scrolled to as this line is drawn -- the value
  // in force now, whether it was set in vblank or a moment ago by HDMA.
  if(line >= 0 && line < PPU_LINES) {
    for(int i = 0; i < 4; i++) {
      ppu->lineHScroll[i][line] = ppu->bgLayer[i].hScroll;
      ppu->lineVScroll[i][line] = ppu->bgLayer[i].vScroll;
    }
    ppu->lineWindow[line][0] = ppu->window1left;
    ppu->lineWindow[line][1] = ppu->window1right;
    ppu->lineWindow[line][2] = ppu->window2left;
    ppu->lineWindow[line][3] = ppu->window2right;
  }
#if defined(XBOX_PORT)
  // R41 native renderer owner gets the complete supported line before the
  // generic core spends work evaluating sprites or composing pixels.
  if(ppu->nativeLineRenderer &&
     ppu->nativeLineRenderer(ppu->nativeLineRendererCtx, ppu, line))
    return;
#endif
  // generic/reference sprite evaluation
  memset(ppu->objPixelBuffer, 0, (size_t)ppu_gameWidth(ppu));
  if(!ppu->forcedBlank) {
#if defined(XBOX_PORT) && defined(ZAMN_R23_MMX_PPU)
    if(ppu->extraLeft == 0 && ppu->extraRight == 0)
      ppu_evaluateSpritesXboxNarrow(ppu, line - 1);
    else
      ppu_evaluateSprites(ppu, line - 1);
#else
    ppu_evaluateSprites(ppu, line - 1);
#endif
  }
  // actual line
  if(ppu->noPixels) return;
#if defined(XBOX_PORT)
  if(ppu_xboxComposeMode1Line(ppu, line)) return;
#if TA_R83_RESIDUAL_PROFILE
  /* Child scope must have its own lexical block; TA_R82_SCOPE uses a fixed local name. */
  {
  TA_R82_SCOPE(TA82_GENERIC_PIXELS);
#endif
  /* A mid-frame mode/format/mosaic change may need both legacy pixels.
     Expand this row before the generic renderer, including untouched bytes. */
  if(line >= 1 && line <= 239)
    ppu_taMaterializeRow(ppu,(unsigned)(line-1+(ppu->evenFrame?0:239)));
  if(s_xboxFrameTarget && line >= 1 && line <= PPU_LINES)
    s_xboxLineNative[line - 1] = 0;
  TA_DIAG(++s_xboxGenericLines);
#endif
  if(ppu->mode == 7) ppu_calculateMode7Starts(ppu, line);
  for(int i = 0; i < 4; ++i) {
    ppu->fastBgCacheX[i] = -0x40000000;
    ppu->fastBgCacheY[i] = -0x40000000;
  }
  for(int i = 0; i < 6; ++i) ppu->fastWindowCacheX[i] = -0x40000000;
  // The Xbox game path is normally 256-wide. Unroll that hot path four
  // pixels at a time; keep the generic widescreen loop for non-zero margins.
  if(ppu->extraLeft == 0 && ppu->extraRight == 0) {
    for(int x = 0; x < 256; x += 4) {
      ppu_handlePixel(ppu, x + 0, line);
      ppu_handlePixel(ppu, x + 1, line);
      ppu_handlePixel(ppu, x + 2, line);
      ppu_handlePixel(ppu, x + 3, line);
    }
  } else {
    for(int x = -ppu->extraLeft; x < 256 + ppu->extraRight; x++)
      ppu_handlePixel(ppu, x, line);
  }
#if TA_R83_RESIDUAL_PROFILE
  }
#endif
}

void ppu_setPixelOutputFormat(Ppu* ppu, int pixelOutputFormat) {
  ppu->pixelOutputFormat = pixelOutputFormat;
}

void ppu_renderFrame(Ppu* ppu, const uint16_t (*hScroll)[PPU_LINES],
                     const uint16_t (*vScroll)[PPU_LINES]) {
  // The same lines the machine drew, in the same order, into the same rows:
  // `ppu_handlePixel` picks the half of the buffer from `evenFrame`, which
  // has not changed since the frame started, and `ppu_putPixels` reads the
  // same half back. Sprites are evaluated against the OAM as it stands, so a
  // caller that has moved them sees them moved.
  const int last = ppu->frameOverscan ? 239 : 224;
  for(int line = 1; line <= last; line++) {
    for(int i = 0; i < 4; i++) {
      if(hScroll) ppu->bgLayer[i].hScroll = hScroll[i][line];
      if(vScroll) ppu->bgLayer[i].vScroll = vScroll[i][line];
    }
    ppu_runLine(ppu, line);
  }
}

bool ppu_frameStatic(const Ppu* ppu) {
  return !ppu->midFrameWrite && ppu->mode != 7;
}

void ppu_setNativeLineRenderer(Ppu* ppu, PpuNativeLineRenderer fn, void* ctx) {
  if(!ppu) return;
  ppu->nativeLineRenderer = fn;
  ppu->nativeLineRendererCtx = ctx;
}

int ppu_layerPixel(Ppu* ppu, int layer, int x, int line, bool sub, int* priority) {
  int layerX = x, layerLine = line;
  if(!ppu_wideMapX(ppu, layer, &layerX, &layerLine)) return 0;
  const bool windowed = sub ? ppu->layer[layer].subScreenWindowed
                            : ppu->layer[layer].mainScreenWindowed;
  const int last = ppu->frameOverscan ? 239 : 224;
  const int l = layerLine < 1 ? 1 : layerLine > last ? last : layerLine;
  if(windowed && ppu_getWindowStateOnLine(ppu, layer, layerX, l)) return 0;
  const int lx = layerX + ppu->lineHScroll[layer][l];
  const int ly = layerLine + ppu->lineVScroll[layer][l];
  int pixel = ppu_getPixelForBgLayer(ppu, lx & 0x3ff, ly & 0x3ff, layer, false);
  *priority = 0;
  if(pixel == 0) {
    pixel = ppu_getPixelForBgLayer(ppu, lx & 0x3ff, ly & 0x3ff, layer, true);
    *priority = 1;
  }
  return pixel;
}

int ppu_layerShiftX(const Ppu* ppu, int layer, int line) {
  if(layer < 0 || layer > 3 || ppu->layerWide[layer] != ppu_wideSweep) return 0;
  if(ppu->extraLeft == 0 && ppu->extraRight == 0) return 0;
  // How much of the map has been swept in, of 256.
  const int hs = ppu->lineHScroll[layer][line] & 0x3ff;
  const int in = hs > 256 ? 0 : 256 - hs;
  return (in * (ppu->extraLeft + ppu->extraRight) + 128) / 256 - ppu->extraLeft;
}

bool ppu_columnEmptyAt(const Ppu* ppu, int layer, int sx) {
  return ppu_columnEmpty(ppu, layer, sx);
}

bool ppu_columnFilledAt(const Ppu* ppu, int layer, int sx) {
  if(layer < 0 || layer > 3) return false;
  int actMode = ppu->mode == 1 && ppu->bg3priority ? 8 : ppu->mode;
  const int bitDepth = bitDepthsPerMode[actMode][layer];
  if(bitDepth != 2 && bitDepth != 4 && bitDepth != 8) return false;
  const bool big = ppu->bgLayer[layer].bigTiles;
  const int step = big ? 16 : 8;
  const int x = sx + ppu->bgLayer[layer].hScroll;
  const int top = ppu->bgLayer[layer].vScroll;
  for(int y = top; y < top + 224 + step; y += step) {
    const int n = ppu_tilemapWord(ppu, layer, x, y) & 0x3ff;
    if(ppu_charEmpty(ppu, layer, n, bitDepth)) return false;
  }
  return true;
}

bool ppu_mathAllowedAt(Ppu* ppu, int x, int line) {
  const int last = ppu->frameOverscan ? 239 : 224;
  const int l = line < 1 ? 1 : line > last ? last : line;
  const bool cw = ppu_getWindowStateOnLine(ppu, 5, x, l);
  return !(ppu->preventMathMode == 3 || (ppu->preventMathMode == 2 && cw) ||
           (ppu->preventMathMode == 1 && !cw));
}

bool ppu_clippedAt(Ppu* ppu, int x) {
  const bool cw = ppu_getWindowState(ppu, 5, x);
  return ppu->clipMode == 3 || (ppu->clipMode == 2 && cw) ||
         (ppu->clipMode == 1 && !cw);
}

int ppu_spriteSize(const Ppu* ppu, int slot) {
  const int index = slot * 2;
  return spriteSizes[ppu->objSize][(ppu->highOam[index >> 3] >> ((index & 7) + 1)) & 1];
}

int ppu_spriteXOf(const Ppu* ppu, int slot) {
  return ppu_spriteX(ppu, (uint8_t)(slot * 2));
}

static void ppu_handlePixel(Ppu* ppu, int x, int y) {
  int r = 0, r2 = 0;
  int g = 0, g2 = 0;
  int b = 0, b2 = 0;
  if(!ppu->forcedBlank) {
    int mainLayer = ppu_getPixel(ppu, x, y, false, &r, &g, &b);
    bool colorWindowState = ppu_getWindowStateFast(ppu, 5, x);
    if(
      ppu->clipMode == 3 ||
      (ppu->clipMode == 2 && colorWindowState) ||
      (ppu->clipMode == 1 && !colorWindowState)
    ) {
      r = 0;
      g = 0;
      b = 0;
    }
    int secondLayer = 5; // backdrop
    bool mathEnabled = mainLayer < 6 && ppu->mathEnabled[mainLayer] && !(
      ppu->preventMathMode == 3 ||
      (ppu->preventMathMode == 2 && colorWindowState) ||
      (ppu->preventMathMode == 1 && !colorWindowState)
    );
    if((mathEnabled && ppu->addSubscreen) || ppu->pseudoHires || ppu->mode == 5 || ppu->mode == 6) {
      secondLayer = ppu_getPixel(ppu, x, y, true, &r2, &g2, &b2);
    }
    // TODO: subscreen pixels can be clipped to black as well
    // TODO: math for subscreen pixels (add/sub sub to main)
    if(mathEnabled) {
      if(ppu->subtractColor) {
        r -= (ppu->addSubscreen && secondLayer != 5) ? r2 : ppu->fixedColorR;
        g -= (ppu->addSubscreen && secondLayer != 5) ? g2 : ppu->fixedColorG;
        b -= (ppu->addSubscreen && secondLayer != 5) ? b2 : ppu->fixedColorB;
      } else {
        r += (ppu->addSubscreen && secondLayer != 5) ? r2 : ppu->fixedColorR;
        g += (ppu->addSubscreen && secondLayer != 5) ? g2 : ppu->fixedColorG;
        b += (ppu->addSubscreen && secondLayer != 5) ? b2 : ppu->fixedColorB;
      }
      if(ppu->halfColor && (secondLayer != 5 || !ppu->addSubscreen)) {
        r >>= 1;
        g >>= 1;
        b >>= 1;
      }
      r = ppu_clampColorFast(r);
      g = ppu_clampColorFast(g);
      b = ppu_clampColorFast(b);
    }
    if(!(ppu->pseudoHires || ppu->mode == 5 || ppu->mode == 6)) {
      r2 = r; g2 = g; b2 = b;
    }
  }
  int row = (y - 1) + (ppu->evenFrame ? 0 : 239);
  // The buffer is indexed from the left edge of the *picture*, which widescreen
  // moves left of column 0. At zero margins this is `x` and the row is the
  // 2048 bytes it always was.
  const int col = x + ppu->extraLeft;
  const uint8_t* bright = s_brightnessLut[ppu->brightness & 0xf];
#if defined(XBOX_PORT)
  // Xbox requests ppu_pixelOutputFormatXBGR (little-endian XRGB). Two aligned
  // dword stores replace six byte stores for every SNES pixel. Keep the generic
  // byte path for the alternate frontend format.
  if(ppu->pixelOutputFormat == ppu_pixelOutputFormatXBGR) {
    uint32_t* out = (uint32_t*)&ppu->pixelBuffer[row * PPU_ROW_BYTES + col * 8];
    out[0] = (uint32_t)bright[b2] | ((uint32_t)bright[g2] << 8) | ((uint32_t)bright[r2] << 16);
    out[1] = (uint32_t)bright[b]  | ((uint32_t)bright[g]  << 8) | ((uint32_t)bright[r]  << 16);
  } else
#endif
  {
    ppu->pixelBuffer[row * PPU_ROW_BYTES + col * 8 + 0 + ppu->pixelOutputFormat] = bright[b2];
    ppu->pixelBuffer[row * PPU_ROW_BYTES + col * 8 + 1 + ppu->pixelOutputFormat] = bright[g2];
    ppu->pixelBuffer[row * PPU_ROW_BYTES + col * 8 + 2 + ppu->pixelOutputFormat] = bright[r2];
    ppu->pixelBuffer[row * PPU_ROW_BYTES + col * 8 + 4 + ppu->pixelOutputFormat] = bright[b];
    ppu->pixelBuffer[row * PPU_ROW_BYTES + col * 8 + 5 + ppu->pixelOutputFormat] = bright[g];
    ppu->pixelBuffer[row * PPU_ROW_BYTES + col * 8 + 6 + ppu->pixelOutputFormat] = bright[r];
  }
}

static int ppu_getPixel(Ppu* ppu, int x, int y, bool sub, int* r, int* g, int* b) {
  // figure out which color is on this location on main- or subscreen, sets it in r, g, b
  // returns which layer it is: 0-3 for bg layer, 4 or 6 for sprites (depending on palette), 5 for backdrop
  int actMode = ppu->mode == 1 && ppu->bg3priority ? 8 : ppu->mode;
  actMode = ppu->mode == 7 && ppu->m7extBg ? 9 : actMode;
  int layer = 5;
  int pixel = 0;
  for(int i = 0; i < layerCountPerMode[actMode]; i++) {
    int curLayer = layersPerMode[actMode][i];
    int curPriority = prioritysPerMode[actMode][i];
    // Which column of this layer belongs at this column of the picture. The
    // two differ only for a layer that does not stretch, and such a layer has
    // columns it declines outright -- the gap in the middle of an anchored one.
    int layerX = x, layerY = y;
    // ppu_wideMapX() is intentionally large. Avoid the call entirely when
    // Xbox is rendering the normal zero-margin 256-wide picture.
    if((ppu->extraLeft != 0 || ppu->extraRight != 0) &&
       !ppu_wideMapX(ppu, curLayer, &layerX, &layerY)) continue;
    bool layerActive = false;
    if(!sub) {
      layerActive = ppu->layer[curLayer].mainScreenEnabled && (
        !ppu->layer[curLayer].mainScreenWindowed || !ppu_getWindowStateFast(ppu, curLayer, layerX)
      );
    } else {
      layerActive = ppu->layer[curLayer].subScreenEnabled && (
        !ppu->layer[curLayer].subScreenWindowed || !ppu_getWindowStateFast(ppu, curLayer, layerX)
      );
    }
    if(layerActive) {
      if(curLayer < 4) {
        // bg layer
        int lx = layerX;
        int ly = layerY;
        if(ppu->bgLayer[curLayer].mosaicEnabled && ppu->mosaicSize > 1) {
          lx -= lx % ppu->mosaicSize;
          ly -= (ly - ppu->mosaicStartLine) % ppu->mosaicSize;
        }
        if(ppu->mode == 7) {
          pixel = ppu_getPixelForMode7(ppu, lx, curLayer, curPriority);
        } else {
          lx += ppu->bgLayer[curLayer].hScroll;
          if(ppu->mode == 5 || ppu->mode == 6) {
            lx *= 2;
            lx += (sub || ppu->bgLayer[curLayer].mosaicEnabled) ? 0 : 1;
            if(ppu->interlace) {
              ly *= 2;
              ly += (ppu->evenFrame || ppu->bgLayer[curLayer].mosaicEnabled) ? 0 : 1;
            }
          }
          ly += ppu->bgLayer[curLayer].vScroll;
          if(ppu->mode == 2 || ppu->mode == 4 || ppu->mode == 6) {
            ppu_handleOPT(ppu, curLayer, &lx, &ly);
          }
          pixel = ppu_getPixelForBgLayerCached(
            ppu, lx & 0x3ff, ly & 0x3ff,
            curLayer, curPriority
          );
        }
      } else {
        // get a pixel from the sprite buffer
        pixel = 0;
        const int oc = layerX + ppu->extraLeft;
        if(ppu->objPriorityBuffer[oc] == curPriority) pixel = ppu->objPixelBuffer[oc];
      }
    }
    if(pixel > 0) {
      layer = curLayer;
      break;
    }
  }
  if(ppu->directColor && layer < 4 && bitDepthsPerMode[actMode][layer] == 8) {
    *r = ((pixel & 0x7) << 2) | ((pixel & 0x100) >> 7);
    *g = ((pixel & 0x38) >> 1) | ((pixel & 0x200) >> 8);
    *b = ((pixel & 0xc0) >> 3) | ((pixel & 0x400) >> 8);
  } else {
    uint16_t color = ta_panel_color(ppu->cgram,pixel & 0xff,ppu->mode);
    *r = color & 0x1f;
    *g = (color >> 5) & 0x1f;
    *b = (color >> 10) & 0x1f;
  }
  if(layer == 4 && pixel < 0xc0) layer = 6; // sprites with palette color < 0xc0
  return layer;
}

static void ppu_handleOPT(Ppu* ppu, int layer, int* lx, int* ly) {
  int x = *lx;
  int y = *ly;
  int column = 0;
  if(ppu->mode == 6) {
    column = ((x - (x & 0xf)) - ((ppu->bgLayer[layer].hScroll * 2) & 0xfff0)) >> 4;
  } else {
    column = ((x - (x & 0x7)) - (ppu->bgLayer[layer].hScroll & 0xfff8)) >> 3;
  }
  if(column > 0) {
    // fetch offset values from layer 3 tilemap
    int valid = layer == 0 ? 0x2000 : 0x4000;
    uint16_t hOffset = ppu_getOffsetValue(ppu, column - 1, 0);
    uint16_t vOffset = 0;
    if(ppu->mode == 4) {
      if(hOffset & 0x8000) {
        vOffset = hOffset;
        hOffset = 0;
      }
    } else {
      vOffset = ppu_getOffsetValue(ppu, column - 1, 1);
    }
    if(ppu->mode == 6) {
      // TODO: not sure if correct
      if(hOffset & valid) *lx = (((hOffset & 0x3f8) + (column * 8)) * 2) | (x & 0xf);
    } else {
      if(hOffset & valid) *lx = ((hOffset & 0x3f8) + (column * 8)) | (x & 0x7);
    }
    // TODO: not sure if correct for interlace
    if(vOffset & valid) *ly = (vOffset & 0x3ff) + (y - ppu->bgLayer[layer].vScroll);
  }
}

static uint16_t ppu_getOffsetValue(Ppu* ppu, int col, int row) {
  int x = col * 8 + ppu->bgLayer[2].hScroll;
  int y = row * 8 + ppu->bgLayer[2].vScroll;
  int tileBits = ppu->bgLayer[2].bigTiles ? 4 : 3;
  int tileHighBit = ppu->bgLayer[2].bigTiles ? 0x200 : 0x100;
  uint16_t tilemapAdr = ppu->bgLayer[2].tilemapAdr + (((y >> tileBits) & 0x1f) << 5 | ((x >> tileBits) & 0x1f));
  if((x & tileHighBit) && ppu->bgLayer[2].tilemapWider) tilemapAdr += 0x400;
  if((y & tileHighBit) && ppu->bgLayer[2].tilemapHigher) tilemapAdr += ppu->bgLayer[2].tilemapWider ? 0x800 : 0x400;
#if defined(XBOX_PORT) && defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
  ppu_taR32Dependency(tilemapAdr);
#endif
  return ppu->vram[tilemapAdr & 0x7fff];
}

static int ppu_getPixelForBgLayer(Ppu* ppu, int x, int y, int layer, bool priority) {
  // figure out address of tilemap word and read it
  bool wideTiles = ppu->bgLayer[layer].bigTiles || ppu->mode == 5 || ppu->mode == 6;
  int tileBitsX = wideTiles ? 4 : 3;
  int tileHighBitX = wideTiles ? 0x200 : 0x100;
  int tileBitsY = ppu->bgLayer[layer].bigTiles ? 4 : 3;
  int tileHighBitY = ppu->bgLayer[layer].bigTiles ? 0x200 : 0x100;
  uint16_t tilemapAdr = ppu->bgLayer[layer].tilemapAdr + (((y >> tileBitsY) & 0x1f) << 5 | ((x >> tileBitsX) & 0x1f));
  if((x & tileHighBitX) && ppu->bgLayer[layer].tilemapWider) tilemapAdr += 0x400;
  if((y & tileHighBitY) && ppu->bgLayer[layer].tilemapHigher) tilemapAdr += ppu->bgLayer[layer].tilemapWider ? 0x800 : 0x400;
  uint16_t tile = ppu->vram[tilemapAdr & 0x7fff];
  // check priority, get palette
  if(((bool) (tile & 0x2000)) != priority) return 0; // wrong priority
  int paletteNum = (tile & 0x1c00) >> 10;
  // figure out position within tile
  int row = (tile & 0x8000) ? 7 - (y & 0x7) : (y & 0x7);
  int col = (tile & 0x4000) ? (x & 0x7) : 7 - (x & 0x7);
  int tileNum = tile & 0x3ff;
  if(wideTiles) {
    // if unflipped right half of tile, or flipped left half of tile
    if(((bool) (x & 8)) ^ ((bool) (tile & 0x4000))) tileNum += 1;
  }
  if(ppu->bgLayer[layer].bigTiles) {
    // if unflipped bottom half of tile, or flipped upper half of tile
    if(((bool) (y & 8)) ^ ((bool) (tile & 0x8000))) tileNum += 0x10;
  }
  // read tiledata, ajust palette for mode 0
  int bitDepth = bitDepthsPerMode[ppu->mode][layer];
  if(ppu->mode == 0) paletteNum += 8 * layer;
  // plane 1 (always)
  int paletteSize = 4;
  uint16_t plane1 = ppu->vram[(ppu->bgLayer[layer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth) + row) & 0x7fff];
  int pixel = (plane1 >> col) & 1;
  pixel |= ((plane1 >> (8 + col)) & 1) << 1;
  // plane 2 (for 4bpp, 8bpp)
  if(bitDepth > 2) {
    paletteSize = 16;
    uint16_t plane2 = ppu->vram[(ppu->bgLayer[layer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth) + 8 + row) & 0x7fff];
    pixel |= ((plane2 >> col) & 1) << 2;
    pixel |= ((plane2 >> (8 + col)) & 1) << 3;
  }
  // plane 3 & 4 (for 8bpp)
  if(bitDepth > 4) {
    paletteSize = 256;
    uint16_t plane3 = ppu->vram[(ppu->bgLayer[layer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth) + 16 + row) & 0x7fff];
    pixel |= ((plane3 >> col) & 1) << 4;
    pixel |= ((plane3 >> (8 + col)) & 1) << 5;
    uint16_t plane4 = ppu->vram[(ppu->bgLayer[layer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth) + 24 + row) & 0x7fff];
    pixel |= ((plane4 >> col) & 1) << 6;
    pixel |= ((plane4 >> (8 + col)) & 1) << 7;
  }
  // return cgram index, or 0 if transparent, palette number in bits 10-8 for 8-color layers
  return pixel == 0 ? 0 : paletteSize * paletteNum + pixel;
}

static inline int ppu_getPixelForBgLayerCached(Ppu* ppu, int x, int y, int layer, bool priority) {
  /*
   * The layer table asks for the same BG pixel once per priority and often
   * again for the subscreen. Decode the tile planes once and cache both the
   * pixel and its priority. Cache lifetime is one scanline.
   */
  if(ppu->fastBgCacheX[layer] == x && ppu->fastBgCacheY[layer] == y) {
    return ppu->fastBgCachePrio[layer] == (uint8_t)priority
      ? ppu->fastBgCachePixel[layer] : 0;
  }

  bool wideTiles = ppu->bgLayer[layer].bigTiles || ppu->mode == 5 || ppu->mode == 6;
  int tileBitsX = wideTiles ? 4 : 3;
  int tileHighBitX = wideTiles ? 0x200 : 0x100;
  int tileBitsY = ppu->bgLayer[layer].bigTiles ? 4 : 3;
  int tileHighBitY = ppu->bgLayer[layer].bigTiles ? 0x200 : 0x100;
  uint16_t tilemapAdr = ppu->bgLayer[layer].tilemapAdr +
      (((y >> tileBitsY) & 0x1f) << 5 | ((x >> tileBitsX) & 0x1f));
  if((x & tileHighBitX) && ppu->bgLayer[layer].tilemapWider) tilemapAdr += 0x400;
  if((y & tileHighBitY) && ppu->bgLayer[layer].tilemapHigher)
    tilemapAdr += ppu->bgLayer[layer].tilemapWider ? 0x800 : 0x400;

  uint16_t tile = ppu->vram[tilemapAdr & 0x7fff];
  const uint8_t tilePrio = (uint8_t)((tile >> 13) & 1);
  int paletteNum = (tile & 0x1c00) >> 10;
  int row = (tile & 0x8000) ? 7 - (y & 0x7) : (y & 0x7);
  int col = (tile & 0x4000) ? (x & 0x7) : 7 - (x & 0x7);
  int tileNum = tile & 0x3ff;

  if(wideTiles && (((bool)(x & 8)) ^ ((bool)(tile & 0x4000)))) tileNum += 1;
  if(ppu->bgLayer[layer].bigTiles &&
      (((bool)(y & 8)) ^ ((bool)(tile & 0x8000)))) tileNum += 0x10;

  const int bitDepth = bitDepthsPerMode[ppu->mode][layer];
  if(ppu->mode == 0) paletteNum += 8 * layer;

  const uint16_t base = (uint16_t)(ppu->bgLayer[layer].tileAdr +
      ((tileNum & 0x3ff) * 4 * bitDepth));
  const int bit2 = 8 + col;
  int pixel = 0;

  uint16_t plane = ppu->vram[(base + row) & 0x7fff];
  pixel = (plane >> col) & 1;
  pixel |= ((plane >> bit2) & 1) << 1;

  if(bitDepth > 2) {
    plane = ppu->vram[(base + 8 + row) & 0x7fff];
    pixel |= ((plane >> col) & 1) << 2;
    pixel |= ((plane >> bit2) & 1) << 3;
  }
  if(bitDepth > 4) {
    plane = ppu->vram[(base + 16 + row) & 0x7fff];
    pixel |= ((plane >> col) & 1) << 4;
    pixel |= ((plane >> bit2) & 1) << 5;
    plane = ppu->vram[(base + 24 + row) & 0x7fff];
    pixel |= ((plane >> col) & 1) << 6;
    pixel |= ((plane >> bit2) & 1) << 7;
  }

  const int result = pixel == 0 ? 0 : (paletteNum << bitDepth) + pixel;
  ppu->fastBgCacheX[layer] = x;
  ppu->fastBgCacheY[layer] = y;
  ppu->fastBgCachePrio[layer] = tilePrio;
  ppu->fastBgCachePixel[layer] = result;

  return tilePrio == (uint8_t)priority ? result : 0;
}

static void ppu_calculateMode7Starts(Ppu* ppu, int y) {
  // expand 13-bit values to signed values
  int hScroll = ((int16_t) (ppu->m7matrix[6] << 3)) >> 3;
  int vScroll = ((int16_t) (ppu->m7matrix[7] << 3)) >> 3;
  int xCenter = ((int16_t) (ppu->m7matrix[4] << 3)) >> 3;
  int yCenter = ((int16_t) (ppu->m7matrix[5] << 3)) >> 3;
  // do calculation
  int clippedH = hScroll - xCenter;
  int clippedV = vScroll - yCenter;
  clippedH = (clippedH & 0x2000) ? (clippedH | ~1023) : (clippedH & 1023);
  clippedV = (clippedV & 0x2000) ? (clippedV | ~1023) : (clippedV & 1023);
  if(ppu->bgLayer[0].mosaicEnabled && ppu->mosaicSize > 1) {
    y -= (y - ppu->mosaicStartLine) % ppu->mosaicSize;
  }
  uint8_t ry = ppu->m7yFlip ? 255 - y : y;
  ppu->m7startX = (
    ((ppu->m7matrix[0] * clippedH) & ~63) +
    ((ppu->m7matrix[1] * ry) & ~63) +
    ((ppu->m7matrix[1] * clippedV) & ~63) +
    (xCenter << 8)
  );
  ppu->m7startY = (
    ((ppu->m7matrix[2] * clippedH) & ~63) +
    ((ppu->m7matrix[3] * ry) & ~63) +
    ((ppu->m7matrix[3] * clippedV) & ~63) +
    (yCenter << 8)
  );
}

static int ppu_getPixelForMode7(Ppu* ppu, int x, int layer, bool priority) {
  uint8_t rx = ppu->m7xFlip ? 255 - x : x;
  int xPos = (ppu->m7startX + ppu->m7matrix[0] * rx) >> 8;
  int yPos = (ppu->m7startY + ppu->m7matrix[2] * rx) >> 8;
  bool outsideMap = xPos < 0 || xPos >= 1024 || yPos < 0 || yPos >= 1024;
  xPos &= 0x3ff;
  yPos &= 0x3ff;
  if(!ppu->m7largeField) outsideMap = false;
  uint8_t tile = outsideMap ? 0 : ppu->vram[(yPos >> 3) * 128 + (xPos >> 3)] & 0xff;
  uint8_t pixel = outsideMap && !ppu->m7charFill ? 0 : ppu->vram[tile * 64 + (yPos & 7) * 8 + (xPos & 7)] >> 8;
  if(layer == 1) {
    if(((bool) (pixel & 0x80)) != priority) return 0;
    return pixel & 0x7f;
  }
  return pixel;
}

// One window's range test at a screen column.
//
// Widescreen: a window covering the console's entire line is how a game says
// "everywhere", and everywhere on a wider line is wider. Nothing else moves.
//
// The temptation is to treat each edge separately -- to say that a window
// starting at 0 starts at the left of the screen, whatever its other end is
// doing -- and it is wrong in a way that is easy to ship and hard to see. This
// game turns colour maths off by pointing both windows at the single column
// `0..0` and asking for maths everywhere *outside* them. Read 0 as "the left
// edge" and that degenerate window becomes 43 pixels wide, so the whole left
// margin of every level got the fixed colour subtracted from it and came out a
// shade darker than the picture it was supposed to be continuing.
static bool ppu_windowTest(const Ppu* ppu, int x, int left, int right) {
  if(ppu->extraLeft != 0 || ppu->extraRight != 0) {
    if(left == 0 && right == 255) {
      return x >= -ppu->extraLeft && x <= 255 + ppu->extraRight;
    }
    // ...and a window one column wide sitting on either edge of the console is
    // a window that has been parked. This game switches colour maths off by
    // pointing both of them at column `0..0` and asking for maths inside them,
    // which on the console leaves maths applying to column 0 and nothing else:
    // one column of fixed colour subtracted, at the extreme left of a picture
    // that no television showed the extreme left of. Widen the picture and that
    // column is 43 pixels in from the edge, in plain view, as a dark line down
    // the left of every level. Read as parked it goes away, and nothing else
    // moves: no window the game aims at anything keeps both ends on one column
    // of the screen edge.
    if(left == right && (left == 0 || left == 255)) return false;
    // A window the game aims at something is aimed at the console's 256, and
    // in a level the something is the status panel, which `ppu_wideAnchor`
    // has split and pinned to the two edges of the wider picture. The window
    // goes with it: an edge in the left half is that many columns in from the
    // picture's left edge, one in the right half that many from its right --
    // exactly where the anchored layer's own columns went. Without this the
    // survivor radar's dimmed box sat 43 columns to the right of its frame.
    // Off a level nothing is anchored and nothing is windowed but the parked
    // pair above, so the console's own coordinates stand.
    bool anchored = false;
    for(int l = 0; l < 4; l++) anchored |= ppu->layerWide[l] == ppu_wideAnchor;
    if(anchored) {
      const int l2 = left < 128 ? left - ppu->extraLeft : left + ppu->extraRight;
      const int r2 = right < 128 ? right - ppu->extraLeft : right + ppu->extraRight;
      return x >= l2 && x <= r2;
    }
  }
  return x >= left && x <= right;
}

// Where `layer` reads from, given a column of the picture that may be outside
// the console's 256, and whether it has anything to say there at all. See the
// `ppu_wide*` policies in the header.
static bool ppu_wideMapX(Ppu* ppu, int layer, int* x, int* y) {
  if(ppu->extraLeft == 0 && ppu->extraRight == 0) return true;
  int policy = ppu->layerWide[layer];
  if(policy == ppu_wideAuto) {
    if(layer > 3) {
      // Objects have no tilemap to ask about. In a level they are world things
      // and the margins are simply more of the same world, which is what
      // `ppu_wideStretch` says; on a fixed screen they are composed for the
      // console's 256 and whoever set the policy will have said `ppu_wideClip`.
      policy = ppu_wideStretch;
    } else if(ppu->layerRaster[layer]) {
      // A layer whose horizontal scroll is rewritten many times while the frame
      // is being drawn is not scrolling: it is being drawn a line at a time,
      // and each line is a window on the map at its own offset. Every question
      // below reads one scroll for the whole frame and would answer for the
      // wrong line, so they are not asked. Continuing to read along each line's
      // own map is both the honest answer and the only one that means anything
      // -- see the title logo, which sweeps in on a per-line scroll and is
      // twice as wide as the console.
      policy = ppu_wideStretch;
    } else if(ppu->layerEdgeEmpty[layer]) {
      // Nothing at the edge of the console means nothing beyond it. The margins
      // show whatever is behind this layer, which is what the console's own
      // edge column is already showing.
      policy = ppu_wideClip;
    } else if(ppu->mode != 7 && ppu->bgLayer[layer].tilemapWider) {
      // A 64-column map, of which the game maintains the 32 the console shows:
      // the other 32 hold whatever that VRAM was last used for, and reading
      // them is how a stone wall comes out shredded. Repeat the 32 it does
      // maintain. (A level's world is 64 columns *and* maintained, and is given
      // `ppu_wideStretch` from outside rather than reaching this.)
      policy = ppu_wideTile;
    } else if(ppu->mode == 7 || ppu->bgLayer[layer].bigTiles ||
              ppu->layerScrolled[layer]) {
      // Otherwise draw what the hardware would have drawn if the line were
      // longer, which is honest in three cases and only three. A map of 16x16
      // tiles is 512 pixels across, so the margins are real map the console had
      // no room for. Mode 7 is a transform, defined at any x. And a 256-pixel
      // map the game *scrolls* is one the console already wraps in plain sight,
      // so its seam is one an artist has had to make look right -- the stone
      // wall drifting diagonally behind the LucasArts logo, the wallpaper
      // behind the character select.
      policy = ppu_wideStretch;
    } else {
      // What is left is a 256-pixel map that has not moved since the screen
      // went up. Its seam has never been seen by anyone, and repeating it
      // prints the tail of a line of text down the far side of the picture --
      // which is exactly what the card naming a level did. Clip.
      policy = ppu_wideClip;
    }
  }
  switch(policy) {
    case ppu_wideClip:
      return *x >= 0 && *x < 256;
    case ppu_wideClampEdge:
      if(*x < 0) *x = 0;
      if(*x > 255) *x = 255;
      return true;
    case ppu_wideCentre: {
      // The picture runs from -extraLeft to 255 + extraRight; its middle is
      // half their difference right of the console's, and the layer goes
      // there with it. Rounded toward the left as the picture's own width is.
      *x -= (ppu->extraRight - ppu->extraLeft) / 2;
      if(*x >= 0 && *x <= 255) return true;
      // Beyond the layer's edge. The mask is a solid field with the letters
      // cut out of its upper part and a row of drips hanging from its foot,
      // and its margins are drawn in two ways. Beside the field and the
      // letters the margin is the field: the edge column's pixel, carried out
      // as `ppu_wideClampEdge` carries it, or where that pixel is a gap, the
      // edge column a few lines into the nearest run of opaque pixels at or
      // above -- past a drip's outline, which is one pixel thick and dark.
      // Beside the drips the margin is more drips: the layer's own 256
      // columns repeated, as a 32-tile map repeats on the hardware, so that
      // the curtain goes on to the edge of the picture instead of turning
      // into a slab. A line is beside the drips when it is below the last
      // line on which the layer is opaque all the way across (the foot of
      // the field) and is a gap within 16 columns of both edges -- the
      // letters, which are also below a solid line now and then as the mask
      // scrolls, keep more field than that at both sides. A line with
      // nothing opaque on it at all is below the mask, and shows what is
      // behind. One search per line and side, cached by the line and its
      // scroll; the foot of the field once per scroll.
      const int side = *x < 0 ? 0 : 1;
      const int edge = side ? 255 : 0;
      const int last = ppu->frameOverscan ? 239 : 224;
      const int l = *y < 1 ? 1 : *y > last ? last : *y;
      const uint16_t hs = ppu->lineHScroll[layer][l], vs = ppu->lineVScroll[layer][l];
      if(ppu->centreLastFullH[layer] != hs || ppu->centreLastFullV[layer] != vs ||
         ppu->centreLastFull[layer] == -2) {
        int full = -1;
        for(int r = last; r >= 1 && full < 0; r--) {
          const int ry = (r + vs) & 0x3ff;
          bool solid = true;
          for(int c = 0; c < 256 && solid; c++) {
            const int rx = (c + hs) & 0x3ff;
            solid = ppu_getPixelForBgLayer(ppu, rx, ry, layer, false) ||
                    ppu_getPixelForBgLayer(ppu, rx, ry, layer, true);
          }
          if(solid) full = r;
        }
        ppu->centreLastFull[layer] = (int16_t)full;
        ppu->centreLastFullH[layer] = hs;
        ppu->centreLastFullV[layer] = vs;
      }
      if(ppu->centreFillLine[layer][side] != l || ppu->centreFillH[layer][side] != hs ||
         ppu->centreFillV[layer][side] != vs) {
        const int ly = (l + vs) & 0x3ff;
        // How far in from each edge the line is opaque, and whether it has
        // anything opaque at all.
        int runL = 0, runR = 0, found = -1, from = l;
        while(runL < 256) {
          const int lx = (runL + hs) & 0x3ff;
          if(!(ppu_getPixelForBgLayer(ppu, lx, ly, layer, false) ||
               ppu_getPixelForBgLayer(ppu, lx, ly, layer, true))) break;
          runL++;
        }
        while(runR < 256 - runL) {
          const int lx = (255 - runR + hs) & 0x3ff;
          if(!(ppu_getPixelForBgLayer(ppu, lx, ly, layer, false) ||
               ppu_getPixelForBgLayer(ppu, lx, ly, layer, true))) break;
          runR++;
        }
        if(side ? runR > 0 : runL > 0) found = edge;
        else
          for(int c = side ? 255 : 0, n = 0; n < 256; n++, c += side ? -1 : 1) {
            const int lx = (c + hs) & 0x3ff;
            if(ppu_getPixelForBgLayer(ppu, lx, ly, layer, false) ||
               ppu_getPixelForBgLayer(ppu, lx, ly, layer, true)) { found = c; break; }
          }
        const bool wrap = found >= 0 && l > ppu->centreLastFull[layer] && runL < 16 && runR < 16;
        if(found >= 0 && !wrap) {
          int run = 0;
          for(int r = l; r >= 1 && run < 4; r--) {
            const int rx = (edge + ppu->lineHScroll[layer][r]) & 0x3ff;
            const int ry = (r + ppu->lineVScroll[layer][r]) & 0x3ff;
            if(ppu_getPixelForBgLayer(ppu, rx, ry, layer, false) ||
               ppu_getPixelForBgLayer(ppu, rx, ry, layer, true)) { found = edge; from = r; run++; }
            else if(run) break;
          }
        }
        ppu->centreFillLine[layer][side] = (int16_t)l;
        ppu->centreFillH[layer][side] = hs;
        ppu->centreFillV[layer][side] = vs;
        ppu->centreFillCol[layer][side] = (int16_t)found;
        ppu->centreFillFrom[layer][side] = (int16_t)from;
        ppu->centreFillWrap[layer][side] = wrap;
      }
      if(ppu->centreFillCol[layer][side] < 0) return false;
      if(ppu->centreFillWrap[layer][side]) { *x &= 255; return true; }
      *x = ppu->centreFillCol[layer][side];
      *y = ppu->centreFillFrom[layer][side];
      return true;
    }
    case ppu_wideCentreClip:
      *x -= (ppu->extraRight - ppu->extraLeft) / 2;
      return *x >= 0 && *x < 256;
    case ppu_wideTile:
      // Two's complement does the wrap for negatives as well, which is the
      // whole reason the console's width is a power of two.
      *x &= 255;
      return true;
    case ppu_wideSweep: {
      const int last = ppu->frameOverscan ? 239 : 224;
      const int l = *y < 1 ? 1 : *y > last ? last : *y;
      *x -= ppu_layerShiftX(ppu, layer, l);
      // Left of the map's first column is more of the first column.
      const int hs = ppu->lineHScroll[layer][l] & 0x3ff;
      if(hs <= 256 && *x + hs < 0) *x = -hs;
      return true;
    }
    case ppu_wideStretch:
      // The only policy the world clamp applies to. A layer that is being
      // continued into the margins can only be continued as far as there is
      // something to continue; past that the picture is outside the map and
      // the backdrop is the honest thing to show.
      return *x >= ppu->wideClampLo && *x <= ppu->wideClampHi;
    case ppu_wideAnchor:
      // The left half keeps its distance from the left edge and the right half
      // its distance from the right, so the two move apart by exactly the width
      // that was added and nothing inside either half is stretched or moved.
      if(*x < 128 - ppu->extraLeft) { *x += ppu->extraLeft; return true; }
      if(*x > 127 + ppu->extraRight) { *x -= ppu->extraRight; return true; }
      return false;
    default:
      return true;
  }
}

static bool ppu_getWindowState(Ppu* ppu, int layer, int x) {
  if(!ppu->windowLayer[layer].window1enabled && !ppu->windowLayer[layer].window2enabled) {
    return false;
  }
  if(ppu->windowLayer[layer].window1enabled && !ppu->windowLayer[layer].window2enabled) {
    bool test = ppu_windowTest(ppu, x, ppu->window1left, ppu->window1right);
    return ppu->windowLayer[layer].window1inversed ? !test : test;
  }
  if(!ppu->windowLayer[layer].window1enabled && ppu->windowLayer[layer].window2enabled) {
    bool test = ppu_windowTest(ppu, x, ppu->window2left, ppu->window2right);
    return ppu->windowLayer[layer].window2inversed ? !test : test;
  }
  bool test1 = ppu_windowTest(ppu, x, ppu->window1left, ppu->window1right);
  bool test2 = ppu_windowTest(ppu, x, ppu->window2left, ppu->window2right);
  if(ppu->windowLayer[layer].window1inversed) test1 = !test1;
  if(ppu->windowLayer[layer].window2inversed) test2 = !test2;
  switch(ppu->windowLayer[layer].maskLogic) {
    case 0: return test1 || test2;
    case 1: return test1 && test2;
    case 2: return test1 != test2;
    case 3: return test1 == test2;
  }
  return false;
}

// The same, with the edges the two windows had on `line` rather than the
// ones they have now -- which differ only in a frame that moved them.
static bool ppu_getWindowStateOnLine(Ppu* ppu, int layer, int x, int line) {
  const uint8_t w1l = ppu->window1left, w1r = ppu->window1right;
  const uint8_t w2l = ppu->window2left, w2r = ppu->window2right;
  ppu->window1left = ppu->lineWindow[line][0];
  ppu->window1right = ppu->lineWindow[line][1];
  ppu->window2left = ppu->lineWindow[line][2];
  ppu->window2right = ppu->lineWindow[line][3];
  const bool state = ppu_getWindowState(ppu, layer, x);
  ppu->window1left = w1l; ppu->window1right = w1r;
  ppu->window2left = w2l; ppu->window2right = w2r;
  return state;
}

// A sprite's X, out of the 8 bits in OAM and the 9th in high OAM.
//
// Nine bits is a range of 512 and the console spends it as -256..255, so an X
// of 256 or more is a sprite hanging off the *left* edge. Widescreen has to
// move that wrap point: with a right margin of 71 the columns 256..326 are now
// on screen, and a sprite there must appear there rather than 512 pixels to the
// left of it. Everything past the widened right edge still wraps, which is what
// keeps a sprite walking off the left edge doing so.
static int ppu_spriteX(const Ppu* ppu, uint8_t index) {
  int x = ppu->oam[index] & 0xff;
  x |= ((ppu->highOam[index >> 3] >> (index & 7)) & 1) << 8;
  const int place = ppu->spritePlace[index >> 1];
  if(place == ppu_spriteCentred) {
    // A sprite laid out over a centred layer: the console's own wrap, since
    // the layer's columns are the console's 256 and the margins repeat
    // them (`ppu_evaluateSprites` draws the copies), then the layer's
    // shift, which is `ppu_wideCentre`'s.
    if(x > 255) x -= 512;
    return x + (ppu->extraRight - ppu->extraLeft) / 2;
  }
  if(x > 255 + ppu->extraRight) x -= 512;
  // An anchored sprite: with the picture widened, where `ppu_wideAnchor`
  // puts a layer's column of the same number.
  if(place == ppu_spriteAnchored && (ppu->extraLeft != 0 || ppu->extraRight != 0))
    x += x < 128 ? -ppu->extraLeft : ppu->extraRight;
  return x + ppu->spriteShift[index >> 1];
}

#if defined(XBOX_PORT) && defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
static void ppu_xboxBuildSpriteLineCache(Ppu* ppu) {
  memset(s_xboxSpriteLineCount, 0, sizeof(s_xboxSpriteLineCount));
  memset(s_xboxSpriteLineRangeOver, 0, sizeof(s_xboxSpriteLineRangeOver));

  const uint8_t first = ppu->objPriority ? (ppu->oamAdr & 0xfe) : 0;
  bool anyFront = false;
  for(int i = 0; i < 128 && !anyFront; ++i) anyFront = ppu->objFront[i];

  /* Preserve the exact R23/SNES sprite order. Instead of asking all 128
     sprites whether they touch each line, walk a sprite once and append it to
     the scanlines it covers. The per-line 32-sprite limit is applied while
     appending, so the cached list is the same list the old evaluator built. */
  for(int pass = anyFront ? 0 : 1; pass < 2; ++pass) {
    uint8_t index = first;
    for(int i = 0; i < 128; ++i, index += 2) {
      if(ppu->objFront[index >> 1] != (pass == 0)) continue;
      const uint8_t y = ppu->oam[index] >> 8;
      const int spriteSize = spriteSizes[ppu->objSize]
          [(ppu->highOam[index >> 3] >> ((index & 7) + 1)) & 1];
      const int spriteHeight = ppu->objInterlace ? spriteSize / 2 : spriteSize;
      const int x = ppu_spriteX(ppu, index);
      if(x + spriteSize <= 0) continue;

      for(int r = 0; r < spriteHeight; ++r) {
        const unsigned line = (uint8_t)(y + r);
        if(line >= PPU_LINES) continue;
        uint8_t count = s_xboxSpriteLineCount[line];
        if(count < 32) {
          s_xboxSpriteLineIndex[line][count] = index;
          s_xboxSpriteLineCount[line] = (uint8_t)(count + 1);
        } else {
          s_xboxSpriteLineRangeOver[line] = 1;
        }
      }
    }
  }

  s_xboxSpriteBuiltEpoch = s_xboxSpriteEpoch;
  TA_DIAG(++s_xboxSpriteCacheBuilds);
}

static inline uint64_t ppu_xboxBroadcastByte(uint8_t v) {
  const uint32_t w = (uint32_t)v * 0x01010101u;
  return (uint64_t)w | ((uint64_t)w << 32);
}

#if defined(ZAMN_R23_MMX_PPU)
/* Compose one fully-visible 8-pixel object sliver. This is the object-side
   counterpart of R23's background MMX compositor: transparent lanes preserve
   the previous object, opaque lanes replace pixel and priority in one batch. */
static inline void ppu_xboxSpritePut8(uint8_t* pix, uint8_t* pri,
                                      const uint8_t* src,
                                      const uint64_t* paletteQ,
                                      const uint64_t* priorityQ) {
  __asm__ volatile(
      "movq (%[src]), %%mm0\n\t"
      "pxor %%mm7, %%mm7\n\t"
      "movq %%mm0, %%mm3\n\t"
      "pcmpeqb %%mm7, %%mm3\n\t"      /* zero lanes */
      "pmovmskb %%mm3, %%eax\n\t"
      "cmp $255, %%eax\n\t"
      "je 9f\n\t"
      "pcmpeqb %%mm6, %%mm6\n\t"       /* all ones */
      "pxor %%mm6, %%mm3\n\t"         /* nonzero mask */
      "movq (%[pal]), %%mm4\n\t"
      "paddb %%mm4, %%mm0\n\t"         /* paletteBase + pixel */
      "pand %%mm3, %%mm0\n\t"          /* zero stays transparent */
      "movq (%[pix]), %%mm1\n\t"
      "movq %%mm0, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[pix])\n\t"
      "movq (%[priq]), %%mm4\n\t"
      "movq (%[pri]), %%mm1\n\t"
      "movq %%mm4, %%mm5\n\t"
      "pxor %%mm1, %%mm5\n\t"
      "pand %%mm3, %%mm5\n\t"
      "pxor %%mm5, %%mm1\n\t"
      "movq %%mm1, (%[pri])\n\t"
      "9:\n\t"
      :
      : [pix] "r" (pix), [pri] "r" (pri), [src] "r" (src),
        [pal] "r" (paletteQ), [priq] "r" (priorityQ)
      : "eax", "mm0", "mm1", "mm3", "mm4", "mm5", "mm6", "mm7", "memory");
}
#endif
#endif

#if defined(XBOX_PORT) && defined(ZAMN_R23_MMX_PPU)
/* R23 256-wide object evaluator.
 *
 * The generic ZAMN evaluator carries the widescreen placement machinery on
 * every scanline: three possible copies, clip ranges and per-copy shifts.
 * The Xbox shipping path is 256 wide, so all of that collapses to the SNES
 * hardware case.  Keep the ZAMN `objFront` ordering and remap behavior, but
 * use the already-decoded R19 object tile cache and a full-sliver fast path.
 */
static void ppu_evaluateSpritesXboxNarrow(Ppu* ppu, int line) {
  TA_R82_SCOPE(TA82_SPRITE_EVAL);
  TA_DIAG(++s_xboxNarrowSpriteLines);
  int spritesFound = 0, tilesFound = 0;
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
  if(__builtin_expect(s_xboxSpriteBuiltEpoch != s_xboxSpriteEpoch, 0))
    ppu_xboxBuildSpriteLineCache(ppu);
  const uint8_t* foundSprites = (line >= 0 && line < PPU_LINES)
      ? s_xboxSpriteLineIndex[line] : NULL;
  if(foundSprites) {
    spritesFound = s_xboxSpriteLineCount[line];
    if(s_xboxSpriteLineRangeOver[line]) ppu->rangeOver = true;
    TA_DIAG(++s_xboxSpriteCachedLines);
  }
#else
  const uint8_t first = ppu->objPriority ? (ppu->oamAdr & 0xfe) : 0;
  uint8_t foundSpritesStorage[32];
  uint8_t* foundSprites = foundSpritesStorage;
  bool over = false;

  bool anyFront = false;
  for(int i = 0; i < 128 && !anyFront; ++i) anyFront = ppu->objFront[i];

  for(int pass = anyFront ? 0 : 1; pass < 2 && !over; ++pass) {
    uint8_t index = first;
    for(int i = 0; i < 128 && !over; ++i, index += 2) {
      if(ppu->objFront[index >> 1] != (pass == 0)) continue;
      const uint8_t y = ppu->oam[index] >> 8;
      const uint8_t row = (uint8_t)(line - y);
      const int spriteSize = spriteSizes[ppu->objSize]
          [(ppu->highOam[index >> 3] >> ((index & 7) + 1)) & 1];
      const int spriteHeight = ppu->objInterlace ? spriteSize / 2 : spriteSize;
      if(row >= spriteHeight) continue;
      const int x = ppu_spriteX(ppu, index);
      if(x + spriteSize <= 0) continue;
      if(++spritesFound > 32) {
        ppu->rangeOver = true;
        spritesFound = 32;
        break;
      }
      foundSprites[spritesFound - 1] = index;
    }
  }
#endif

  for(int i = spritesFound; i > 0; --i) {
    const uint8_t index = foundSprites[i - 1];
    const uint8_t y = ppu->oam[index] >> 8;
    uint8_t row = (uint8_t)(line - y);
    const int spriteSize = spriteSizes[ppu->objSize]
        [(ppu->highOam[index >> 3] >> ((index & 7) + 1)) & 1];
    const int x = ppu_spriteX(ppu, index);
    if(x <= -spriteSize) continue;
    if(ppu->objInterlace) row = (uint8_t)(row * 2 + (ppu->evenFrame ? 0 : 1));

    const uint16_t attr = ppu->oam[index + 1];
    const int tile = attr & 0xff;
    const int palette = (attr & 0xe00) >> 9;
    const bool hFlipped = (attr & 0x4000) != 0;
    if(attr & 0x8000) row = (uint8_t)(spriteSize - 1 - row);
    const uint16_t objAdr = (attr & 0x100) ? ppu->objTileAdr2 : ppu->objTileAdr1;
    const uint8_t priority = (uint8_t)((attr & 0x3000) >> 12);
    const uint8_t paletteBase = (uint8_t)(0x80 + 16 * palette);
    const bool remap = ppu->objRemapOn[index >> 1];
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
    const uint64_t paletteQ = ppu_xboxBroadcastByte(paletteBase);
    const uint64_t priorityQ = ppu_xboxBroadcastByte(priority);
#endif

    for(int col = 0; col < spriteSize; col += 8) {
      const int screen0 = x + col;
      if(screen0 <= -8 || screen0 >= 256) continue;
      if(++tilesFound > 34) {
        ppu->timeOver = true;
        break;
      }
      const int usedCol = hFlipped ? spriteSize - 1 - col : col;
      const uint8_t usedTile = (uint8_t)((((tile >> 4) + (row / 8)) << 4) |
          (((tile & 0xf) + (usedCol / 8)) & 0xf));
      const uint16_t objBase = (uint16_t)(objAdr + usedTile * 16);
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
      const uint8_t* objRow = (hFlipped
          ? ppu_xboxTile4PixelsFlipped(ppu, objBase)
          : ppu_xboxTile4Pixels(ppu, objBase)) + (row & 7) * 8;
#else
      const uint8_t* objRow = ppu_xboxTile4Pixels(ppu, objBase) + (row & 7) * 8;
#endif

      /* Fully visible, normal-palette sliver: R35 composes all eight object
         pixels at once. Remapped blood/radar sprites stay on the exact scalar
         path because their palette is intentionally per-pixel. */
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE) && defined(ZAMN_R23_MMX_PPU)
      if(screen0 >= 0 && screen0 <= 248 && !remap) {
        ppu_xboxSpritePut8(ppu->objPixelBuffer + screen0,
                           ppu->objPriorityBuffer + screen0, objRow,
                           &paletteQ, &priorityQ);
        TA_DIAG(++s_xboxSpriteMmxSlivers);
        continue;
      }
#endif
      if(screen0 >= 0 && screen0 <= 248) {
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
        TA_DIAG(++s_xboxSpriteScalarSlivers);
#endif
        for(int px = 0; px < 8; ++px) {
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
          const int p = objRow[px];
#else
          const int p = hFlipped ? objRow[7 - px] : objRow[px];
#endif
          if(!p) continue;
          const int sx = screen0 + px;
          ppu->objPixelBuffer[sx] = remap && ppu->objRemap[p]
              ? ppu->objRemap[p] : (uint8_t)(paletteBase + p);
          ppu->objPriorityBuffer[sx] = priority;
        }
      } else {
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
        TA_DIAG(++s_xboxSpriteScalarSlivers);
#endif
        for(int px = 0; px < 8; ++px) {
          const int sx = screen0 + px;
          if(sx < 0 || sx >= 256) continue;
#if defined(ZAMN_R35_NATIVE_OBJECT_PIPELINE)
          const int p = objRow[px];
#else
          const int p = hFlipped ? objRow[7 - px] : objRow[px];
#endif
          if(!p) continue;
          ppu->objPixelBuffer[sx] = remap && ppu->objRemap[p]
              ? ppu->objRemap[p] : (uint8_t)(paletteBase + p);
          ppu->objPriorityBuffer[sx] = priority;
        }
      }
    }
    if(tilesFound > 34) break;
  }
}
#endif

static void ppu_evaluateSprites(Ppu* ppu, int line) {
  TA_R82_SCOPE(TA82_SPRITE_EVAL);
  // TODO: rectangular sprites, wierdness with sprites at -256
  const uint8_t first = ppu->objPriority ? (ppu->oamAdr & 0xfe) : 0;
  uint8_t index = first;
  int spritesFound = 0;
  int tilesFound = 0;
  const int width = ppu_gameWidth(ppu);
  // The ceilings of 32 sprites and 34 tile slivers a line are a statement about
  // how much fetching time a scanline had, and a wider line is proportionally
  // more of it. Holding them at the console's numbers would make widescreen
  // *drop* sprites the console managed to draw, which is the opposite of what
  // widening is for. At zero margins these are 32 and 34 exactly.
  const int spriteLimit = 32 * width / 256;
  const int tileLimit = 34 * width / 256;
  uint8_t foundSprites[32 * PPU_MAX_WIDTH / 256] = {};
  // ...and for each, how far it is shifted and which picture columns it is
  // clipped to (`foundLo` to `foundHi`, exclusive, in the console's
  // coordinates): a sprite placed with a centred layer is found up to three
  // times, 256 columns apart, each clipped to the layer's columns or to one
  // margin -- see `ppu_spriteCentred`.
  int16_t foundShift[32 * PPU_MAX_WIDTH / 256] = {};
  int16_t foundLo[32 * PPU_MAX_WIDTH / 256] = {};
  int16_t foundHi[32 * PPU_MAX_WIDTH / 256] = {};
  const int mid = (ppu->extraRight - ppu->extraLeft) / 2;
  bool over = false;
  // iterate over oam to find sprites in range -- zamn: twice, the entries
  // marked `objFront` on the first pass and the rest on the second, so that
  // the marked ones are found first and so drawn in front.
  bool anyFront = false;
  for(int i = 0; i < 128 && !anyFront; i++) anyFront = ppu->objFront[i];
  for(int pass = anyFront ? 0 : 1; pass < 2 && !over; pass++) {
  index = first;
  for(int i = 0; i < 128 && !over; i++, index += 2) {
    if(ppu->objFront[index >> 1] != (pass == 0)) continue;
    uint8_t y = ppu->oam[index] >> 8;
    // check if the sprite is on this line and get the sprite size
    uint8_t row = line - y;
    int spriteSize = spriteSizes[ppu->objSize][(ppu->highOam[index >> 3] >> ((index & 7) + 1)) & 1];
    int spriteHeight = ppu->objInterlace ? spriteSize / 2 : spriteSize;
    if(row < spriteHeight) {
      // in y-range, get the x location, using the high bit as well
      int x = ppu_spriteX(ppu, index);
      const bool centred = ppu->spritePlace[index >> 1] == ppu_spriteCentred &&
                           (ppu->extraLeft != 0 || ppu->extraRight != 0);
      for(int k = centred ? -1 : 0; k <= (centred ? 1 : 0); k++) {
        const int sx = x + k * 256;
        const int lo = !centred ? -ppu->extraLeft : k < 0 ? -ppu->extraLeft : k > 0 ? mid + 256 : mid;
        const int hi = !centred ? 256 + ppu->extraRight : k < 0 ? mid : k > 0 ? 256 + ppu->extraRight : mid + 256;
        // if in x-range, record
        if(sx + spriteSize <= lo || (centred && sx >= hi)) continue;
        // break if we found 32 sprites already
        spritesFound++;
        if(spritesFound > spriteLimit) {
          ppu->rangeOver = true;
          spritesFound = spriteLimit;
          over = true;
          break;
        }
        foundSprites[spritesFound - 1] = index;
        foundShift[spritesFound - 1] = (int16_t)(k * 256);
        foundLo[spritesFound - 1] = (int16_t)lo;
        foundHi[spritesFound - 1] = (int16_t)hi;
      }
    }
  }
  }
  // iterate over found sprites backwards to fetch max 34 tile slivers
  for(int i = spritesFound; i > 0; i--) {
    index = foundSprites[i - 1];
    const int lo = foundLo[i - 1], hi = foundHi[i - 1];
    uint8_t y = ppu->oam[index] >> 8;
    uint8_t row = line - y;
    int spriteSize = spriteSizes[ppu->objSize][(ppu->highOam[index >> 3] >> ((index & 7) + 1)) & 1];
    int x = ppu_spriteX(ppu, index) + foundShift[i - 1];
    if(x > -spriteSize - ppu->extraLeft) {
      // update row according to obj-interlace
      if(ppu->objInterlace) row = row * 2 + (ppu->evenFrame ? 0 : 1);
      // get some data for the sprite and y-flip row if needed
      int tile = ppu->oam[index + 1] & 0xff;
      int palette = (ppu->oam[index + 1] & 0xe00) >> 9;
      bool hFlipped = ppu->oam[index + 1] & 0x4000;
      if(ppu->oam[index + 1] & 0x8000) row = spriteSize - 1 - row;
      // fetch all tiles in x-range
      for(int col = 0; col < spriteSize; col += 8) {
        if(col + x > -8 - ppu->extraLeft && col + x < 256 + ppu->extraRight) {
          // break if we found > 34 8*1 slivers already
          tilesFound++;
          if(tilesFound > tileLimit) {
            ppu->timeOver = true;
            break;
          }
          // figure out which tile this uses, looping within 16x16 pages, and get it's data
          int usedCol = hFlipped ? spriteSize - 1 - col : col;
          uint8_t usedTile = (((tile >> 4) + (row / 8)) << 4) | (((tile & 0xf) + (usedCol / 8)) & 0xf);
          uint16_t objAdr = (ppu->oam[index + 1] & 0x100) ? ppu->objTileAdr2 : ppu->objTileAdr1;
          const uint16_t objBase = (uint16_t)(objAdr + usedTile * 16);
          const uint8_t* objRow = ppu_xboxTile4Pixels(ppu, objBase) + (row & 0x7) * 8;
          // go over each pixel
          for(int px = 0; px < 8; px++) {
            int pixel = hFlipped ? objRow[7 - px] : objRow[px];
            // draw it in the buffer if there is a pixel here, and inside
            // the clip
            int screenCol = col + x + px + ppu->extraLeft;
            if(pixel > 0 && screenCol >= 0 && screenCol < width && col + x + px >= lo &&
               col + x + px < hi) {
              // zamn: `objRemap`, a sprite in colours of the frontend's
              ppu->objPixelBuffer[screenCol] = ppu->objRemapOn[index >> 1] && ppu->objRemap[pixel]
                  ? ppu->objRemap[pixel] : 0x80 + 16 * palette + pixel;
              ppu->objPriorityBuffer[screenCol] = (ppu->oam[index + 1] & 0x3000) >> 12;
            }
          }
        }
      }
      if(tilesFound > tileLimit) break; // break out of sprite-loop if max tiles found
    }
  }
}

static uint16_t ppu_getVramRemap(Ppu* ppu) {
  uint16_t adr = ppu->vramPointer;
  switch(ppu->vramRemapMode) {
    case 0: return adr;
    case 1: return (adr & 0xff00) | ((adr & 0xe0) >> 5) | ((adr & 0x1f) << 3);
    case 2: return (adr & 0xfe00) | ((adr & 0x1c0) >> 6) | ((adr & 0x3f) << 3);
    case 3: return (adr & 0xfc00) | ((adr & 0x380) >> 7) | ((adr & 0x7f) << 3);
  }
  return adr;
}

uint8_t ppu_read(Ppu* ppu, uint8_t adr) {
  switch(adr) {
    case 0x04: case 0x14: case 0x24:
    case 0x05: case 0x15: case 0x25:
    case 0x06: case 0x16: case 0x26:
    case 0x08: case 0x18: case 0x28:
    case 0x09: case 0x19: case 0x29:
    case 0x0a: case 0x1a: case 0x2a: {
      return ppu->ppu1openBus;
    }
    case 0x34:
    case 0x35:
    case 0x36: {
      int result = ppu->m7matrix[0] * (ppu->m7matrix[1] >> 8);
      ppu->ppu1openBus = (result >> (8 * (adr - 0x34))) & 0xff;
      return ppu->ppu1openBus;
    }
    case 0x37: {
      // TODO: only when ppulatch is set
      ppu->hCount = ppu->snes->hPos / 4;
      ppu->vCount = ppu->snes->vPos;
      ppu->countersLatched = true;
      return ppu->snes->openBus;
    }
    case 0x38: {
      uint8_t ret = 0;
      if(ppu->oamInHigh) {
        ret = ppu->highOam[((ppu->oamAdr & 0xf) << 1) | ppu->oamSecondWrite];
        if(ppu->oamSecondWrite) {
          ppu->oamAdr++;
          if(ppu->oamAdr == 0) ppu->oamInHigh = false;
        }
      } else {
        if(!ppu->oamSecondWrite) {
          ret = ppu->oam[ppu->oamAdr] & 0xff;
        } else {
          ret = ppu->oam[ppu->oamAdr++] >> 8;
          if(ppu->oamAdr == 0) ppu->oamInHigh = true;
        }
      }
      ppu->oamSecondWrite = !ppu->oamSecondWrite;
#if defined(XBOX_PORT)
      ppu_xboxInvalidateSpriteCache();
#endif
      ppu->ppu1openBus = ret;
      return ret;
    }
    case 0x39: {
      uint16_t val = ppu->vramReadBuffer;
      if(!ppu->vramIncrementOnHigh) {
        ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
        ppu->vramPointer += ppu->vramIncrement;
      }
      ppu->ppu1openBus = val & 0xff;
      return val & 0xff;
    }
    case 0x3a: {
      uint16_t val = ppu->vramReadBuffer;
      if(ppu->vramIncrementOnHigh) {
        ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
        ppu->vramPointer += ppu->vramIncrement;
      }
      ppu->ppu1openBus = val >> 8;
      return val >> 8;
    }
    case 0x3b: {
      uint8_t ret = 0;
      if(!ppu->cgramSecondWrite) {
        ret = ppu->cgram[ppu->cgramPointer] & 0xff;
      } else {
        ret = ((ppu->cgram[ppu->cgramPointer++] >> 8) & 0x7f) | (ppu->ppu2openBus & 0x80);
      }
      ppu->cgramSecondWrite = !ppu->cgramSecondWrite;
      ppu->ppu2openBus = ret;
      return ret;
    }
    case 0x3c: {
      uint8_t val = 0;
      if(ppu->hCountSecond) {
        val = ((ppu->hCount >> 8) & 1) | (ppu->ppu2openBus & 0xfe);
      } else {
        val = ppu->hCount & 0xff;
      }
      ppu->hCountSecond = !ppu->hCountSecond;
      ppu->ppu2openBus = val;
      return val;
    }
    case 0x3d: {
      uint8_t val = 0;
      if(ppu->vCountSecond) {
        val = ((ppu->vCount >> 8) & 1) | (ppu->ppu2openBus & 0xfe);
      } else {
        val = ppu->vCount & 0xff;
      }
      ppu->vCountSecond = !ppu->vCountSecond;
      ppu->ppu2openBus = val;
      return val;
    }
    case 0x3e: {
      uint8_t val = 0x1; // ppu1 version (4 bit)
      val |= ppu->ppu1openBus & 0x10;
      val |= ppu->rangeOver << 6;
      val |= ppu->timeOver << 7;
      ppu->ppu1openBus = val;
      return val;
    }
    case 0x3f: {
      uint8_t val = 0x3; // ppu2 version (4 bit)
      val |= ppu->snes->palTiming << 4; // ntsc/pal
      val |= ppu->ppu2openBus & 0x20;
      val |= ppu->countersLatched << 6;
      val |= ppu->evenFrame << 7;
      ppu->countersLatched = false; // TODO: only when ppulatch is set
      ppu->hCountSecond = false;
      ppu->vCountSecond = false;
      ppu->ppu2openBus = val;
      return val;
    }
    default: {
      return ppu->snes->openBus;
    }
  }
}

void ppu_write(Ppu* ppu, uint8_t adr, uint8_t val) {
  // A write while a line is being drawn -- the same test `snes_runCycle`
  // uses to decide whether a line is -- to anything but a scroll register
  // ($210D-$2114, which are recorded per line). See `midFrameWrite`.
  if(!ppu->snes->inVblank && ppu->snes->vPos > 0 && adr >= 0x26 && adr <= 0x29)
    ppu->windowRaster = true;
  else if(!ppu->snes->inVblank && ppu->snes->vPos > 0 && (adr < 0x0d || adr > 0x14)) {
    if(!ppu->midFrameWrite) {
      ppu->midFrameAdr = adr;
      ppu->midFrameLine = ppu->snes->vPos;
      ppu->midFrameWrites = 0;
    }
    ppu->midFrameWrite = true;
    ppu->midFrameWrites++;
  }
  switch(adr) {
    case 0x00: {
      // TODO: oam address reset when written on first line of vblank, (and when forced blank is disabled?)
      ppu->brightness = val & 0xf;
      ppu->forcedBlank = val & 0x80;
      break;
    }
    case 0x01: {
      ppu->objSize = val >> 5;
      ppu->objTileAdr1 = (val & 7) << 13;
      ppu->objTileAdr2 = ppu->objTileAdr1 + (((val & 0x18) + 8) << 9);
#if defined(XBOX_PORT)
      ppu_xboxInvalidateSpriteCache();
#endif
      break;
    }
    case 0x02: {
      ppu->oamAdr = val;
      ppu->oamAdrWritten = ppu->oamAdr;
      ppu->oamInHigh = ppu->oamInHighWritten;
      ppu->oamSecondWrite = false;
#if defined(XBOX_PORT)
      ppu_xboxInvalidateSpriteCache();
#endif
      break;
    }
    case 0x03: {
      ppu->objPriority = val & 0x80;
      ppu->oamInHigh = val & 1;
      ppu->oamInHighWritten = ppu->oamInHigh;
      ppu->oamAdr = ppu->oamAdrWritten;
      ppu->oamSecondWrite = false;
#if defined(XBOX_PORT)
      ppu_xboxInvalidateSpriteCache();
#endif
      break;
    }
    case 0x04: {
      if(ppu->oamInHigh) {
        ppu->highOam[((ppu->oamAdr & 0xf) << 1) | ppu->oamSecondWrite] = val;
        if(ppu->oamSecondWrite) {
          ppu->oamAdr++;
          if(ppu->oamAdr == 0) ppu->oamInHigh = false;
        }
      } else {
        if(!ppu->oamSecondWrite) {
          ppu->oamBuffer = val;
        } else {
          ppu->oam[ppu->oamAdr++] = (val << 8) | ppu->oamBuffer;
          if(ppu->oamAdr == 0) ppu->oamInHigh = true;
        }
      }
      ppu->oamSecondWrite = !ppu->oamSecondWrite;
#if defined(XBOX_PORT)
      ppu_xboxInvalidateSpriteCache();
#endif
      break;
    }
    case 0x05: {
      ppu->mode = val & 0x7;
      ppu->bg3priority = val & 0x8;
      ppu->bgLayer[0].bigTiles = val & 0x10;
      ppu->bgLayer[1].bigTiles = val & 0x20;
      ppu->bgLayer[2].bigTiles = val & 0x40;
      ppu->bgLayer[3].bigTiles = val & 0x80;
      break;
    }
    case 0x06: {
      // TODO: mosaic line reset specifics
      ppu->bgLayer[0].mosaicEnabled = val & 0x1;
      ppu->bgLayer[1].mosaicEnabled = val & 0x2;
      ppu->bgLayer[2].mosaicEnabled = val & 0x4;
      ppu->bgLayer[3].mosaicEnabled = val & 0x8;
      ppu->mosaicSize = (val >> 4) + 1;
      ppu->mosaicStartLine = ppu->snes->vPos;
      break;
    }
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0a: {
      ppu->bgLayer[adr - 7].tilemapWider = val & 0x1;
      ppu->bgLayer[adr - 7].tilemapHigher = val & 0x2;
      ppu->bgLayer[adr - 7].tilemapAdr = (val & 0xfc) << 8;
      break;
    }
    case 0x0b: {
      ppu->bgLayer[0].tileAdr = (val & 0xf) << 12;
      ppu->bgLayer[1].tileAdr = (val & 0xf0) << 8;
      break;
    }
    case 0x0c: {
      ppu->bgLayer[2].tileAdr = (val & 0xf) << 12;
      ppu->bgLayer[3].tileAdr = (val & 0xf0) << 8;
      break;
    }
    case 0x0d: {
      ppu->m7matrix[6] = ((val << 8) | ppu->m7prev) & 0x1fff;
      ppu->m7prev = val;
      // fallthrough to normal layer BG-HOFS
    }
    case 0x0f:
    case 0x11:
    case 0x13: {
      // Counted as well as stored, because how *often* within one frame a
      // background's horizontal scroll changes is the difference between a
      // layer that is scrolling and a layer being drawn a line at a time. See
      // `layerRaster`.
      const int hsLayer = (adr - 0xd) / 2;
      const uint16_t hsNew = ((val << 8) | (ppu->scrollPrev & 0xf8) | (ppu->scrollPrev2 & 0x7)) & 0x3ff;
      if(hsNew != ppu->bgLayer[hsLayer].hScroll && ppu->hScrollWrites[hsLayer] < 255)
        ppu->hScrollWrites[hsLayer]++;
      ppu->bgLayer[hsLayer].hScroll = hsNew;
      ppu->scrollPrev = val;
      ppu->scrollPrev2 = val;
      break;
    }
    case 0x0e: {
      ppu->m7matrix[7] = ((val << 8) | ppu->m7prev) & 0x1fff;
      ppu->m7prev = val;
      // fallthrough to normal layer BG-VOFS
    }
    case 0x10:
    case 0x12:
    case 0x14: {
      ppu->bgLayer[(adr - 0xe) / 2].vScroll = ((val << 8) | ppu->scrollPrev) & 0x3ff;
      ppu->scrollPrev = val;
      break;
    }
    case 0x15: {
      if((val & 3) == 0) {
        ppu->vramIncrement = 1;
      } else if((val & 3) == 1) {
        ppu->vramIncrement = 32;
      } else {
        ppu->vramIncrement = 128;
      }
      ppu->vramRemapMode = (val & 0xc) >> 2;
      ppu->vramIncrementOnHigh = val & 0x80;
      break;
    }
    case 0x16: {
      ppu->vramPointer = (ppu->vramPointer & 0xff00) | val;
      ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
      break;
    }
    case 0x17: {
      ppu->vramPointer = (ppu->vramPointer & 0x00ff) | (val << 8);
      ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
      break;
    }
    case 0x18: {
      // TODO: vram access during rendering (also cgram and oam)
      uint16_t vramAdr = ppu_getVramRemap(ppu);
      const uint16_t oldWord=ppu->vram[vramAdr & 0x7fff];
      ppu->vram[vramAdr & 0x7fff] = (ppu->vram[vramAdr & 0x7fff] & 0xff00) | val;
#if defined(XBOX_PORT)
#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
      if(oldWord!=ppu->vram[vramAdr & 0x7fff])
#endif
        ppu_xboxInvalidateTileWord(vramAdr);
#else
      (void)oldWord;
#endif
      if(!ppu->vramIncrementOnHigh) ppu->vramPointer += ppu->vramIncrement;
      break;
    }
    case 0x19: {
      uint16_t vramAdr = ppu_getVramRemap(ppu);
      const uint16_t oldWord=ppu->vram[vramAdr & 0x7fff];
      ppu->vram[vramAdr & 0x7fff] = (ppu->vram[vramAdr & 0x7fff] & 0x00ff) | (val << 8);
#if defined(XBOX_PORT)
#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
      if(oldWord!=ppu->vram[vramAdr & 0x7fff])
#endif
        ppu_xboxInvalidateTileWord(vramAdr);
#else
      (void)oldWord;
#endif
      if(ppu->vramIncrementOnHigh) ppu->vramPointer += ppu->vramIncrement;
      break;
    }
    case 0x1a: {
      ppu->m7largeField = val & 0x80;
      ppu->m7charFill = val & 0x40;
      ppu->m7yFlip = val & 0x2;
      ppu->m7xFlip = val & 0x1;
      break;
    }
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e: {
      ppu->m7matrix[adr - 0x1b] = (val << 8) | ppu->m7prev;
      ppu->m7prev = val;
      break;
    }
    case 0x1f:
    case 0x20: {
      ppu->m7matrix[adr - 0x1b] = ((val << 8) | ppu->m7prev) & 0x1fff;
      ppu->m7prev = val;
      break;
    }
    case 0x21: {
      ppu->cgramPointer = val;
      ppu->cgramSecondWrite = false;
      break;
    }
    case 0x22: {
      if(!ppu->cgramSecondWrite) {
        ppu->cgramBuffer = val;
      } else {
        ppu->cgram[ppu->cgramPointer++] = (val << 8) | ppu->cgramBuffer;
#if defined(XBOX_PORT)
        ppu_xboxInvalidatePaletteCache();
#endif
      }
      ppu->cgramSecondWrite = !ppu->cgramSecondWrite;
      break;
    }
    case 0x23:
    case 0x24:
    case 0x25: {
      ppu->windowLayer[(adr - 0x23) * 2].window1inversed = val & 0x1;
      ppu->windowLayer[(adr - 0x23) * 2].window1enabled = val & 0x2;
      ppu->windowLayer[(adr - 0x23) * 2].window2inversed = val & 0x4;
      ppu->windowLayer[(adr - 0x23) * 2].window2enabled = val & 0x8;
      ppu->windowLayer[(adr - 0x23) * 2 + 1].window1inversed = val & 0x10;
      ppu->windowLayer[(adr - 0x23) * 2 + 1].window1enabled = val & 0x20;
      ppu->windowLayer[(adr - 0x23) * 2 + 1].window2inversed = val & 0x40;
      ppu->windowLayer[(adr - 0x23) * 2 + 1].window2enabled = val & 0x80;
      break;
    }
    case 0x26: {
      ppu->window1left = val;
      break;
    }
    case 0x27: {
      ppu->window1right = val;
      break;
    }
    case 0x28: {
      ppu->window2left = val;
      break;
    }
    case 0x29: {
      ppu->window2right = val;
      break;
    }
    case 0x2a: {
      ppu->windowLayer[0].maskLogic = val & 0x3;
      ppu->windowLayer[1].maskLogic = (val >> 2) & 0x3;
      ppu->windowLayer[2].maskLogic = (val >> 4) & 0x3;
      ppu->windowLayer[3].maskLogic = (val >> 6) & 0x3;
      break;
    }
    case 0x2b: {
      ppu->windowLayer[4].maskLogic = val & 0x3;
      ppu->windowLayer[5].maskLogic = (val >> 2) & 0x3;
      break;
    }
    case 0x2c: {
      ppu->layer[0].mainScreenEnabled = val & 0x1;
      ppu->layer[1].mainScreenEnabled = val & 0x2;
      ppu->layer[2].mainScreenEnabled = val & 0x4;
      ppu->layer[3].mainScreenEnabled = val & 0x8;
      ppu->layer[4].mainScreenEnabled = val & 0x10;
      break;
    }
    case 0x2d: {
      ppu->layer[0].subScreenEnabled = val & 0x1;
      ppu->layer[1].subScreenEnabled = val & 0x2;
      ppu->layer[2].subScreenEnabled = val & 0x4;
      ppu->layer[3].subScreenEnabled = val & 0x8;
      ppu->layer[4].subScreenEnabled = val & 0x10;
      break;
    }
    case 0x2e: {
      ppu->layer[0].mainScreenWindowed = val & 0x1;
      ppu->layer[1].mainScreenWindowed = val & 0x2;
      ppu->layer[2].mainScreenWindowed = val & 0x4;
      ppu->layer[3].mainScreenWindowed = val & 0x8;
      ppu->layer[4].mainScreenWindowed = val & 0x10;
      break;
    }
    case 0x2f: {
      ppu->layer[0].subScreenWindowed = val & 0x1;
      ppu->layer[1].subScreenWindowed = val & 0x2;
      ppu->layer[2].subScreenWindowed = val & 0x4;
      ppu->layer[3].subScreenWindowed = val & 0x8;
      ppu->layer[4].subScreenWindowed = val & 0x10;
      break;
    }
    case 0x30: {
      ppu->directColor = val & 0x1;
      ppu->addSubscreen = val & 0x2;
      ppu->preventMathMode = (val & 0x30) >> 4;
      ppu->clipMode = (val & 0xc0) >> 6;
      break;
    }
    case 0x31: {
      ppu->subtractColor = val & 0x80;
      ppu->halfColor = val & 0x40;
      for(int i = 0; i < 6; i++) {
        ppu->mathEnabled[i] = val & (1 << i);
      }
      break;
    }
    case 0x32: {
      if(val & 0x80) ppu->fixedColorB = val & 0x1f;
      if(val & 0x40) ppu->fixedColorG = val & 0x1f;
      if(val & 0x20) ppu->fixedColorR = val & 0x1f;
      break;
    }
    case 0x33: {
      ppu->interlace = val & 0x1;
      ppu->objInterlace = val & 0x2;
      ppu->overscan = val & 0x4;
      ppu->pseudoHires = val & 0x8;
      ppu->m7extBg = val & 0x40;
#if defined(XBOX_PORT)
      ppu_xboxInvalidateSpriteCache();
#endif
      break;
    }
    default: {
      break;
    }
  }
}

int ppu_gameWidth(const Ppu* ppu) {
  return 256 + ppu->extraLeft + ppu->extraRight;
}

int ppu_outputWidth(const Ppu* ppu) {
  return ppu_gameWidth(ppu) * 2;
}

void ppu_setWidescreen(Ppu* ppu, int left, int right) {
  if(left < 0) left = 0;
  if(right < 0) right = 0;
  if(left > PPU_EXTRA_MAX) left = PPU_EXTRA_MAX;
  if(right > PPU_EXTRA_MAX) right = PPU_EXTRA_MAX;
  ppu->extraLeft = left;
  ppu->extraRight = right;
}

void ppu_setLayerWide(Ppu* ppu, int layer, int policy) {
  if(layer < 0 || layer > 4) return;
  ppu->layerWide[layer] = (uint8_t) policy;
}

bool ppu_bgTilemapWider(const Ppu* ppu, int layer) {
  if(layer < 0 || layer > 3) return false;
  return ppu->bgLayer[layer].tilemapWider;
}

bool ppu_bgOnMainScreen(const Ppu* ppu, int layer) {
  if(layer < 0 || layer > 3) return false;
  return ppu->layer[layer].mainScreenEnabled;
}

void ppu_setSprite(Ppu* ppu, int slot, int x, int y, uint16_t tileAttr,
                   bool large) {
  if(slot < 0 || slot >= 128) return;
  const int i = slot * 2;
  ppu->oam[i] = (uint16_t)(((y & 0xff) << 8) | (x & 0xff));
  ppu->oam[i + 1] = tileAttr;
  const int bit = i & 7;
  const uint8_t mask = (uint8_t)(3 << bit);
  const uint8_t val = (uint8_t)((((x >> 8) & 1) << bit) | ((large ? 1 : 0) << (bit + 1)));
  ppu->highOam[i >> 3] = (uint8_t)((ppu->highOam[i >> 3] & ~mask) | val);
#if defined(XBOX_PORT)
  ppu_xboxInvalidateSpriteCache();
#endif
}

int ppu_freeSprite(const Ppu* ppu, int from) {
  for(int slot = from; slot < 128; slot++) {
    const int y = ppu->oam[slot * 2] >> 8;
    // $E0..$F0 is the gap between the bottom of the screen and the rows a
    // sprite hanging off the top would use, so nothing the game draws lands
    // there and anything that does is parked.
    if(y >= 0xe0 && y <= 0xf0) return slot;
  }
  return 128;
}

void ppu_setWideClamp(Ppu* ppu, int lo, int hi) {
  ppu->wideClampLo = lo;
  ppu->wideClampHi = hi;
}

void ppu_writeVramWord(Ppu* ppu, uint16_t wordAdr, uint16_t val) {
  const uint16_t oldWord=ppu->vram[wordAdr & 0x7fff];
  ppu->vram[wordAdr & 0x7fff] = val;
#if defined(XBOX_PORT)
#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
  if(oldWord!=val)
#endif
    ppu_xboxInvalidateTileWord(wordAdr);
#else
  (void)oldWord;
#endif
}

#if defined(XBOX_PORT)
/* The normal ppu_putPixels() copies the 224 SNES source rows twice each into
   output rows [16,464). That is faithful, but wasteful for an Xbox texture.
   Make no color conversion, no frame timing or PPU state changes. The guest
   runtime can instead point-sample the 224 unique source lines. Return false
   for any mode whose output is not safely described by these exact bytes. */
bool ppu_taCanCopyActive224(const Ppu* ppu) {
  return ppu && !ppu->extraLeft && !ppu->extraRight &&
         !ppu->frameOverscan && !ppu->frameInterlace && !ppu->interlace;
}

/* The supplied destination may be a linear D3D8 texture with an Xbox-chosen
   row pitch. We copy only active image data, with the same eligibility guard
   as R5 and no changes to the SNES framebuffer or machine state. */
bool ppu_taCopyActive224Strided(const Ppu* ppu, uint8_t* dst,
                               unsigned pitch, unsigned capacity) {
  if(!ppu_taCanCopyActive224(ppu) || !dst || pitch < 2048u ||
     pitch > 16384u || capacity < (223u * pitch + 2048u)) return false;
#if defined(TA_VS_ACTIVE_CAPTURE) && TA_VS_ACTIVE_CAPTURE && defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  ppu_r126ResolveActive224((Ppu*)ppu);
#endif
  const int fieldBase = ppu->evenFrame ? 0 : 239;
  int streamed=0;
#if defined(TA_R39_NATIVE256) && TA_R39_NATIVE256
  uint32_t expanded[512] __attribute__((aligned(16)));
#endif
  for(unsigned y=0; y<224; ++y) {
    const unsigned row=fieldBase+y;
    const uint8_t* src=&ppu->pixelBuffer[row * PPU_ROW_BYTES];
#if defined(TA_R39_NATIVE256) && TA_R39_NATIVE256
    if(ppu->taNative256Row[row]) {
      const uint32_t* colors=(const uint32_t*)src;
      for(unsigned x=0;x<256;++x) expanded[2*x]=expanded[2*x+1]=colors[x];
      src=(const uint8_t*)expanded;
    }
#endif
    streamed |= ta_video_copy_row(dst + (size_t)y * pitch,src,2048u);
  }
  ta_video_copy_fence(streamed);
  return true;
}

bool ppu_taCopyActive224(const Ppu* ppu, uint8_t* dst, unsigned bytes) {
  return ppu_taCopyActive224Strided(ppu, dst, 2048u, bytes);
}

bool ppu_taCanCopyNative224(const Ppu* ppu) {
#if defined(TA_R39_NATIVE256) && TA_R39_NATIVE256
  if(!ppu_taCanCopyActive224(ppu)) return false;
  const unsigned fieldBase=ppu->evenFrame?0u:239u;
  /* Every visible row must be the guarded native renderer's exact low-res
     output. One generic/hires row selects the original 512-wide texture. */
  for(unsigned y=0;y<224;++y) if(!ppu->taNative256Row[fieldBase+y]) return false;
  return true;
#else
  (void)ppu; return false;
#endif
}
bool ppu_taCopyNative224Strided(const Ppu* ppu,uint8_t* dst,unsigned pitch,unsigned capacity) {
  if(!ppu_taCanCopyNative224(ppu) || !dst || pitch<1024u || pitch>16384u ||
     capacity<223u*pitch+1024u) return false;
  const unsigned fieldBase=ppu->evenFrame?0u:239u;
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
#if defined(TA_VS_ACTIVE_CAPTURE) && TA_VS_ACTIVE_CAPTURE
  ppu_r126ResolveActive224((Ppu*)ppu);
#else
  ppu_r126ResolveAll((Ppu*)ppu);
#endif
#endif
  int streamed=0;
  for(unsigned y=0;y<224;++y)
    streamed|=ta_video_copy_row(dst+(size_t)y*pitch,
               &ppu->pixelBuffer[(fieldBase+y)*PPU_ROW_BYTES],1024u);
  ta_video_copy_fence(streamed);
  return true;
}

bool ppu_xboxBeginNativeTarget(Ppu* ppu, uint8_t* pixels, int pitch) {
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  if(!s_r126Managed)ppu_taMaterializeNativeRows(ppu);
  s_r126CaptureValid=false;
  if(s_r126Owner!=ppu){if(s_r126Owner)ppu_r126ResolveAll(s_r126Owner);s_r126Owner=ppu;memset(s_r126External,0,sizeof s_r126External);memset(s_r126Second,0,sizeof s_r126Second);}
  memset(s_r126GpuRows,0,sizeof s_r126GpuRows);s_r126GpuReady=false;
#else
  ppu_taMaterializeNativeRows(ppu);
#endif
  s_xboxFrameTarget = NULL;
  s_xboxFramePitch = 0;
  memset(s_xboxLineNative, 0, sizeof(s_xboxLineNative));
  if(!pixels || pitch < 256 * 4) return false;
  if(ppu->extraLeft != 0 || ppu->extraRight != 0) return false;
  /* Keep high-resolution/pseudo-hires/non-Mode-1 frames on LakeSnes's proven
     legacy 512-wide path. ZAMN gameplay is Mode 1, which is the hot path this
     optimization is for. */
  if(
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
     (ppu->mode!=1 && ppu->mode!=2) || ppu->frameOverscan || ppu->noPixels ||
#else
     ppu->mode != 1 ||
#endif
     ppu->pseudoHires || ppu->pixelOutputFormat != ppu_pixelOutputFormatXBGR) return false;
  /* ZAMN never enables interlace, but keep the old full renderer if it ever
     does so this optimization cannot change the picture. Overscan is fine:
     it is simply 239 native rows instead of 224. */
  if(ppu->interlace || ppu->frameInterlace) return false;
  s_xboxFrameTarget = pixels;
  s_xboxFramePitch = pitch;
  return true;
}

bool ppu_xboxFinishNativeTarget(Ppu* ppu, int* width, int* height) {
  if(!s_xboxFrameTarget) return false;
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  if(s_r126Managed){
  bool supported=(!s_r126CaptureValid||s_r126CaptureEven==ppu->evenFrame)&&!ppu->frameOverscan&&!ppu->frameInterlace&&!ppu->interlace&&!ppu->pseudoHires&&!ppu->extraLeft&&!ppu->extraRight;
  bool gpu=s_r126GpuSub!=NULL;
  for(unsigned y=0;y<224;y++){supported=supported&&s_xboxLineNative[y];gpu=gpu&&s_r126GpuRows[y];}
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
  unsigned indexed=0;for(unsigned y=0;y<224;y++)indexed+=s_r127Rows[y];
  if(indexed && indexed!=224)gpu=false;
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
  unsigned direct=0;for(unsigned y=0;y<224;y++)direct+=s_r129Rows[y];
  if(direct&&direct!=224)gpu=false;
  s_r129Ready=gpu&&direct==224;s_r129Enabled=false;
#endif
  s_r127Ready=gpu&&indexed==224;s_r127Enabled=false;
#endif
  if(!supported){ppu_r126ResolveAll(ppu);s_xboxFrameTarget=NULL;s_xboxFramePitch=0;s_r126GpuSub=NULL;s_r126Managed=false;return false;}
  if(!gpu){/* A mixed frame is fully materialized to exact RGB before drawing. */
    const unsigned field=ppu->evenFrame?0:239;
    for(unsigned y=0;y<224;y++)if(s_r126GpuRows[y]){
      uint32_t*dst=(uint32_t*)(s_xboxFrameTarget+y*s_xboxFramePitch);
      const uint32_t*sub=(const uint32_t*)(s_r126GpuSub+y*s_r126GpuPitch);
      #if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
      if(s_r127Alias[field+y]){
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
        if(s_r129Alias[field+y]){
          for(unsigned x=0;x<256;x++)dst[x]=s_r127Alias[field+y]->color[dst[x]>>24];
          s_r129Alias[field+y]=false;
        }else
#endif
        for(unsigned x=0;x<256;x++)dst[x]=ta_r127_decode(dst[x],s_r127Alias[field+y]);
        s_r127Alias[field+y]=NULL;
      }else
#endif
      for(unsigned x=0;x<256;x++)dst[x]=ppu_r126Color(dst[x],sub[x]);
      s_r126Second[field+y]=NULL;
    }
  }
  s_r126GpuReady=gpu;s_r126GpuSub=NULL;s_xboxFrameTarget=NULL;s_xboxFramePitch=0;
  if(width)*width=256;if(height)*height=224;s_r126Managed=false;return true;
  }
#endif
  const int h = ppu->frameOverscan ? 239 : 224;
  const int fieldBase = ppu->evenFrame ? 0 : 239;
  for(int y = 0; y < h; ++y) {
    if(s_xboxLineNative[y]) continue;
    uint32_t* dst = (uint32_t*)(s_xboxFrameTarget + y * s_xboxFramePitch);
    const uint32_t* src = (const uint32_t*)&ppu->pixelBuffer[(fieldBase + y) * PPU_ROW_BYTES];
    /* Generic LakeSnes output is two identical horizontal pixels in normal
       low-res modes. Pick the first of each pair; no filtering/approximation. */
    for(int x = 0; x < 256; ++x) dst[x] = src[x * 2];
  }
  s_xboxFrameTarget = NULL;
  s_xboxFramePitch = 0;
  if(width) *width = 256;
  if(height) *height = h;
  return true;
}

void ppu_xboxCancelNativeTarget(void) {
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
  if(s_r126Managed && s_r126Owner)ppu_r126ResolveAll(s_r126Owner);
  s_r126Managed=false;s_r126GpuSub=NULL;
#endif
  s_xboxFrameTarget = NULL;
  s_xboxFramePitch = 0;
}
#endif

void ppu_putPixels(Ppu* ppu, uint8_t* pixels) {
#if defined(XBOX_PORT)
  ppu_taMaterializeNativeRows(ppu);
#endif
  // The destination is packed at whatever width the picture currently is, and
  // the source row starts at its own byte 0 -- `ppu_handlePixel` already shifted
  // the leftmost column there. At zero margins both pitches are 2048.
  const int dpitch = ppu_outputWidth(ppu) * 4;
  for(int y = 0; y < (ppu->frameOverscan ? 239 : 224); y++) {
    int dest = y * 2 + (ppu->frameOverscan ? 2 : 16);
    int y1 = y, y2 = y + 239;
    if(!ppu->frameInterlace) {
      y1 = y + (ppu->evenFrame ? 0 : 239);
      y2 = y1;
    }
    memcpy(pixels + (dest * dpitch), &ppu->pixelBuffer[y1 * PPU_ROW_BYTES], dpitch);
    memcpy(pixels + ((dest + 1) * dpitch), &ppu->pixelBuffer[y2 * PPU_ROW_BYTES], dpitch);
  }
  // clear top 2 lines, and following 14 and last 16 lines if not overscanning
  memset(pixels, 0, dpitch * 2);
  if(!ppu->frameOverscan) {
    memset(pixels + (2 * dpitch), 0, dpitch * 14);
    memset(pixels + (464 * dpitch), 0, dpitch * 16);
  }
}

#if defined(XBOX_PORT) && defined(TA_R44_DMA_VRAM_FAST) && TA_R44_DMA_VRAM_FAST
/* Exact $2118/$2119 byte semantics without the general register dispatcher.
 * VMAIN/VMADD remain live after each DMA wait; each byte is visible before
 * the next wait, including rendering, HDMA and frame callbacks. */
void ppu_dmaWriteVramByte(Ppu* ppu, bool high, uint8_t val) {
  const uint8_t adr = high ? 0x19 : 0x18;
  if(!ppu->snes->inVblank && ppu->snes->vPos > 0) {
    if(!ppu->midFrameWrite) {
      ppu->midFrameAdr = adr;
      ppu->midFrameLine = ppu->snes->vPos;
      ppu->midFrameWrites = 0;
    }
    ppu->midFrameWrite = true;
    ppu->midFrameWrites++;
  }
  const uint16_t vramAdr = ppu_getVramRemap(ppu);
  const unsigned index = vramAdr & 0x7fffu;
  const uint16_t oldWord = ppu->vram[index];
  const uint16_t word = high
    ? (uint16_t)((oldWord & 0x00ffu) | ((uint16_t)val << 8))
    : (uint16_t)((oldWord & 0xff00u) | val);
  ppu->vram[index] = word;
#if defined(TA_R32_BG_LINE_CACHE) && TA_R32_BG_LINE_CACHE
  if(oldWord != word)
#endif
    ppu_xboxInvalidateTileWord(vramAdr);
  if(high == ppu->vramIncrementOnHigh)
    ppu->vramPointer += ppu->vramIncrement;
}
#endif
