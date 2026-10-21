/* R109: Mode 2 OBJ selection and exact full-brightness window math in one pass.
 * BG planes stay read-only. OBJ priority ties retain the background, just as
 * ppu_xboxMode1Put does. The caller guards mode/width/math/brightness/windows. */
#if defined(TA_R109_FUSED_MODE2_COMPOSE) && TA_R109_FUSED_MODE2_COMPOSE
/* R111: the Mode-2 fused renderer's sprite-free eight-pixel runs.
 * A zero OBJ byte cannot win on either screen. Keep math/window and every
 * cached BG read identical; any nonzero OBJ run uses R109 unchanged. */
#if defined(TA_R111_SPRITE_FREE_SPANS) && TA_R111_SPRITE_FREE_SPANS
static inline void ppu_taR111NoObj8(const Ppu* ppu,uint32_t* out,
                                     unsigned x,uint32_t mathMask) {
  for(unsigned i=0;i<8u;++i,mathMask>>=1) {
    const unsigned pixel=s_taR94ReadMainPix[x+i];
    uint32_t color=s_mode1ColorXrgb[pixel];
    if(mathMask&1u) {
      const unsigned layer=pixel?s_taR94ReadMainLayer[x+i]:5u;
      if(layer<6u && ppu->mathEnabled[layer]) {
        const unsigned sub=s_mode1ReadSubPix[x+i];
        const unsigned subLayer=sub?s_mode1ReadSubLayer[x+i]:5u;
        color=subLayer!=5u?ppu_taR50Subtract(color,s_mode1ColorXrgb[sub])
                           :s_mode1SubFixedXrgb[pixel];
      }
    }
    out[x+i]=color;
  }
}
#endif
static void ppu_taR109Output(const Ppu* ppu,uint32_t* out) {
  const Layer* objState=&ppu->layer[4];
  const bool mainObj=objState->mainScreenEnabled,subObj=objState->subScreenEnabled;
  if(!mainObj && !subObj){ppu_taR50PackedOutput(ppu,out,1u);return;}
  for(unsigned word=0;word<8u;++word) {
    uint32_t mathMask=s_taWindowMask[5][word];
    const uint32_t objMask=s_taWindowMask[4][word];
    uint32_t mainMask=mainObj?(objState->mainScreenWindowed?~objMask:~0u):0u;
    uint32_t subMask=subObj?(objState->subScreenWindowed?~objMask:~0u):0u;
    const unsigned end=(word+1u)*32u;
#if defined(TA_R111_SPRITE_FREE_SPANS) && TA_R111_SPRITE_FREE_SPANS
    for(unsigned start=word*32u;start<end;start+=8u) {
      uint64_t obj8;
      /* Unaligned buffer access stays well-defined with memcpy. Only skip
       * R109 if ALL eight original OBJ pixels are zero. */
      memcpy(&obj8,ppu->objPixelBuffer+start,sizeof obj8);
      if(__builtin_expect(obj8==0,1)) {
        ppu_taR111NoObj8(ppu,out,start,mathMask);
        mathMask>>=8u;mainMask>>=8u;subMask>>=8u;
        continue;
      }
      for(unsigned x=start;x<start+8u;++x,mathMask>>=1,mainMask>>=1,subMask>>=1) {
#else
    for(unsigned x=word*32u;x<end;++x,mathMask>>=1,mainMask>>=1,subMask>>=1) {
#endif
      unsigned mainPixel=s_taR94ReadMainPix[x];
      const unsigned obj=ppu->objPixelBuffer[x];
      unsigned z=0;bool mainWon=false;
      if(obj) {
        z=((ppu->objPriorityBuffer[x]&3u)+1u)*2u;
        if((mainMask&1u) && z>s_taR109MainPri[x]){mainPixel=obj;mainWon=true;TA_R109_COUNT(3);}
      }
      uint32_t color=s_mode1ColorXrgb[mainPixel];
      if(mathMask&1u) {
        const unsigned layer=mainWon?(obj<0xc0u?6u:4u):(mainPixel?s_taR94ReadMainLayer[x]:5u);
        if(layer<6u && ppu->mathEnabled[layer]) {
          unsigned subPixel=s_mode1ReadSubPix[x];
          unsigned subLayer=subPixel?s_mode1ReadSubLayer[x]:5u;
          if(obj && (subMask&1u) && z>s_taR109SubPri[x]) {
            subPixel=obj;subLayer=obj<0xc0u?6u:4u;TA_R109_COUNT(4);
          }
          color=subLayer!=5u?ppu_taR50Subtract(color,s_mode1ColorXrgb[subPixel])
                             :s_mode1SubFixedXrgb[mainPixel];
        }
      }
      out[x]=color;
    }
#if defined(TA_R111_SPRITE_FREE_SPANS) && TA_R111_SPRITE_FREE_SPANS
    }
#endif
  }
}
#endif
