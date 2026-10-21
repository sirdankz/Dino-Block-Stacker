/* R93: resolve color-window decisions per run, not per output pixel.
 * Included after the unchanged R85 reference. All inputs are current-line
 * buffers and tables; no state is cached across scanlines or guest events. */
#if defined(TA_R93_WINDOW_SPANS) && TA_R93_WINDOW_SPANS
#if !TA_R85_MODE1_SCALAR_LUT || !TA_R26_COMPACT_SUBMATH || !TA_R17_WINDOW_BITSETS
#error R93 requires the existing R85/R26/R17 exact color paths
#endif

enum { TA93_BLACK, TA93_DIRECT, TA93_FIXED, TA93_CLIP_FIXED,
       TA93_SUB, TA93_CLIP_SUB };

static inline __attribute__((always_inline)) void
ppu_taR93Span(Ppu* ppu, uint32_t* out, unsigned x, unsigned end,
              unsigned stride, unsigned action, uint32_t clipMath) {
#define TA93_WRITE(c) do { const uint32_t color_=(c); out[0]=color_; \
  if(stride==2u) out[1]=color_; out+=stride; } while(0)
  switch(action) {
    case TA93_BLACK:
      for(;x<end;++x) { TA93_WRITE(0u); }
      break;
    case TA93_DIRECT:
      for(;x<end;++x) { TA93_WRITE(s_mode1ColorXrgb[s_taR94ReadMainPix[x]]); }
      break;
    case TA93_FIXED:
      for(;x<end;++x) {
        const unsigned pixel=s_taR94ReadMainPix[x];
        const unsigned layer=pixel?s_taR94ReadMainLayer[x]:5u;
        TA93_WRITE(layer<6u && ppu->mathEnabled[layer]
                   ?s_mode1MathXrgb[pixel]:s_mode1ColorXrgb[pixel]);
      }
      break;
    case TA93_CLIP_FIXED:
      for(;x<end;++x) {
        const unsigned pixel=s_taR94ReadMainPix[x];
        const unsigned layer=pixel?s_taR94ReadMainLayer[x]:5u;
        TA93_WRITE(layer<6u && ppu->mathEnabled[layer]?clipMath:0u);
      }
      break;
    case TA93_SUB:
      for(;x<end;++x) {
        const unsigned pixel=s_taR94ReadMainPix[x];
        const unsigned layer=pixel?s_taR94ReadMainLayer[x]:5u;
        uint32_t color=s_mode1ColorXrgb[pixel];
        if(layer<6u && ppu->mathEnabled[layer]) {
          const unsigned sub=s_mode1SubPix[x];
          const unsigned subLayer=sub?s_mode1SubLayer[x]:5u;
          color=subLayer!=5u?ppu_taR26SubMathColor(ppu,pixel,sub)
                            :s_mode1SubFixedXrgb[pixel];
        }
        TA93_WRITE(color);
      }
      break;
    default: /* TA93_CLIP_SUB: addition to black, never CGRAM[0]. */
      for(;x<end;++x) {
        const unsigned pixel=s_taR94ReadMainPix[x];
        const unsigned layer=pixel?s_taR94ReadMainLayer[x]:5u;
        uint32_t color=0;
        if(layer<6u && ppu->mathEnabled[layer]) {
          const unsigned sub=s_mode1SubPix[x];
          const unsigned subLayer=sub?s_mode1SubLayer[x]:5u;
          color=subLayer!=5u?ppu_taR85BlackSubMath(ppu,sub):clipMath;
        }
        TA93_WRITE(color);
      }
      break;
  }
#undef TA93_WRITE
}

static inline __attribute__((always_inline)) void
ppu_taR93Runs(Ppu* ppu,uint32_t* out,unsigned stride,
              const unsigned action[2],uint32_t clipMath) {
  /* Equal actions make the entire window irrelevant for this line. */
  if(action[0]==action[1]) {
    ppu_taR93Span(ppu,out,0,256,stride,action[0],clipMath);
    return;
  }
  for(unsigned word=0;word<8;++word) {
    uint32_t mask=s_taWindowMask[5][word];
    unsigned x=word*32u,remaining=32u;
    do {
      const unsigned win=mask&1u;
      const uint32_t transitions=mask^(win?UINT32_MAX:0u);
      /* Never ctz(0), never shift by 32. Each run stays within this word. */
      unsigned count=transitions?(unsigned)__builtin_ctz(transitions):remaining;
      if(count>remaining)count=remaining;
      ppu_taR93Span(ppu,out,x,x+count,stride,action[win],clipMath);
      out+=count*stride;x+=count;remaining-=count;
      if(remaining)mask>>=count;
    } while(remaining);
  }
}

static void ppu_taR93ScalarLine(Ppu* ppu,uint32_t* out,bool nativeOut,
                               const uint8_t* bright,bool anyMath) {
  unsigned action[2];
  bool needsMath=false;
  for(unsigned win=0;win<2;++win) {
    const bool clipped=ppu->clipMode==3u ||
      (ppu->clipMode==2u && win) || (ppu->clipMode==1u && !win);
    const bool math=anyMath && !(ppu->preventMathMode==3u ||
      (ppu->preventMathMode==2u && win) || (ppu->preventMathMode==1u && !win));
    /* Black minus either color clamps to black, even with half-color. */
    if(clipped && (!math || ppu->subtractColor))action[win]=TA93_BLACK;
    else if(!math)action[win]=TA93_DIRECT;
    else {
      needsMath=true;
      action[win]=ppu->addSubscreen?(clipped?TA93_CLIP_SUB:TA93_SUB)
                                  :(clipped?TA93_CLIP_FIXED:TA93_FIXED);
    }
  }
  uint32_t clipMath=0;
  if(needsMath) {
    if(ppu->addSubscreen)ppu_xboxBuildSubMathTables(ppu);
    else ppu_xboxBuildMathPalette(ppu);
    int r=ppu->subtractColor?-(int)ppu->fixedColorR:(int)ppu->fixedColorR;
    int g=ppu->subtractColor?-(int)ppu->fixedColorG:(int)ppu->fixedColorG;
    int b=ppu->subtractColor?-(int)ppu->fixedColorB:(int)ppu->fixedColorB;
    if(!ppu->addSubscreen && ppu->halfColor){r>>=1;g>>=1;b>>=1;}
    clipMath=(uint32_t)bright[ppu_clampColorFast(b)] |
      ((uint32_t)bright[ppu_clampColorFast(g)]<<8) |
      ((uint32_t)bright[ppu_clampColorFast(r)]<<16);
  }
  /* Constant strides allow two tight loops without per-pixel width tests. */
  if(nativeOut)ppu_taR93Runs(ppu,out,1u,action,clipMath);
  else ppu_taR93Runs(ppu,out,2u,action,clipMath);
}
#endif
