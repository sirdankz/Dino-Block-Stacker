/* R124: stream two raw Mode2 backgrounds, resolve whole screens once.
 * The original layer traversal still resolves every map/OPT/character word
 * and records the original dependencies. No cached rendering policy changes.
 * The output retains the existing sprite pass and exact color/window math. */
#if defined(TA_R124_MODE2_STREAM) && TA_R124_MODE2_STREAM
#if !TA_R102_MODE2_DUAL_PUT || !TA_R88_PACKED_BG_COPY || !defined(ZAMN_R23_MMX_PPU)
#error R124 requires the R102/R88 MMX Mode2 baseline
#endif
#if TA_R109_FUSED_MODE2_COMPOSE || TA_R110_MODE2_FIRST_BG_CACHE || TA_R123_REVERSE_MODE2_DIRTY || TA_R96_MODE2_EARLY_OCCLUSION
#error R124 starts from the clean R117 renderer
#endif

/* Zero pixels are transparent, including tiles with a nonzero palette base.
 * Raw priority is zero for transparent pixels; other values are positive
 * signed bytes (BG1=3/7,BG2=1/5).
 * Strict greater-than reproduces the original priority test. */
static inline void ppu_taR124Resolve8(unsigned x,uint8_t* dst,uint64_t allowA,uint64_t allowB){
 const uint8_t* src=s_taR124Rows[0].pix+x;
 __asm__ volatile(
  "movq 0(%[s]), %%mm0\n\t"
  "movq 768(%[s]), %%mm1\n\t"
  "movq 256(%[s]), %%mm2\n\t"
  "movq 1024(%[s]), %%mm3\n\t"
  "pand %[a], %%mm0\n\t"
  "pand %[a], %%mm2\n\t"
  "pand %[b], %%mm1\n\t"
  "pand %[b], %%mm3\n\t"
  "movq %%mm2, %%mm6\n\t"
  "pcmpgtb %%mm3, %%mm6\n\t"
  "pxor %%mm1, %%mm0\n\t"
  "pand %%mm6, %%mm0\n\t"
  "pxor %%mm1, %%mm0\n\t"
  "movq %%mm0, 256(%[d])\n\t"
  "pxor %%mm3, %%mm2\n\t"
  "pand %%mm6, %%mm2\n\t"
  "pxor %%mm3, %%mm2\n\t"
  "movq %%mm2, 0(%[d])\n\t"
  "pandn %[one], %%mm6\n\t"
  "movq %%mm6, 512(%[d])"
  : : [s] "r"(src),[d] "r"(dst),[a] "m"(allowA),[b] "m"(allowB),[one] "m"(s_taR102Broadcast[1])
  : "mm0","mm1","mm2","mm3","mm6","memory");
}
static inline uint64_t ppu_taR124Allow(const Ppu* ppu,unsigned layer,unsigned x,bool sub){
 const Layer* ls=&ppu->layer[layer];
 if(!(sub?ls->subScreenEnabled:ls->mainScreenEnabled))return 0;
 if(!(sub?ls->subScreenWindowed:ls->mainScreenWindowed))return UINT64_MAX;
 uint64_t blocked;memcpy(&blocked,s_taR124Rows[layer].blocked+x,8);return ~blocked;
}
/* A screen with one enabled background needs no priority competition.
 * Raw priority is already zero for transparent pixels. Color math selects
 * backdrop by pixel==0, so its unused layer byte may be the constant BG id. */
static void ppu_taR124Screen(const Ppu* ppu,bool sub,uint8_t* dst){
 const unsigned enabled=(sub?ppu->layer[0].subScreenEnabled:ppu->layer[0].mainScreenEnabled) |
    ((sub?ppu->layer[1].subScreenEnabled:ppu->layer[1].mainScreenEnabled)<<1);
 if(!enabled){memset(dst,0,768);return;}
 if(enabled!=3){
  TA_R124_COUNT(2);
  const unsigned layer=enabled==1?0:1;
  const TaR124RawRow* row=&s_taR124Rows[layer];
  memset(dst+512,(int)layer,256);
  if(!(sub?ppu->layer[layer].subScreenWindowed:ppu->layer[layer].mainScreenWindowed)){
   memcpy(dst,row->pri,256);memcpy(dst+256,row->pix,256);return;
  }
  for(unsigned x=0;x<256;x+=8){
   const uint8_t* src=row->pix+x;uint8_t* d=dst+x;
   __asm__ volatile("movq 512(%[s]), %%mm2\n\tpcmpeqb %%mm3, %%mm3\n\tpxor %%mm3, %%mm2\n\t"
    "movq (%[s]), %%mm0\n\tmovq 256(%[s]), %%mm1\n\t"
    "pand %%mm2, %%mm0\n\tpand %%mm2, %%mm1\n\t"
    "movq %%mm1, (%[d])\n\tmovq %%mm0, 256(%[d])"
    : : [s] "r"(src),[d] "r"(d) : "mm0","mm1","mm2","mm3","memory");
  }
  return;
 }
 for(unsigned x=0;x<256;x+=8)
  ppu_taR124Resolve8(x,dst+x,ppu_taR124Allow(ppu,0,x,sub),ppu_taR124Allow(ppu,1,x,sub));
}
static void ppu_taR124BuildBG(Ppu* ppu,int y,const uint8_t zbg[3][2],bool needSub,bool trackLayer){
 TA_R82_SCOPE(TA82_BG);
 TA_R124_COUNT(1);
 for(int layer=0;layer<2;++layer){
  s_taR124Writing=&s_taR124Rows[layer];
  ppu_xboxMode1DrawLayerR68Baseline(ppu,layer,y,zbg,needSub,trackLayer);
 }
 s_taR124Writing=NULL;
 ppu_taR124Screen(ppu,false,s_mode1MainPri);
 if(needSub){
  const bool same=ppu->layer[0].mainScreenEnabled==ppu->layer[0].subScreenEnabled &&
    ppu->layer[1].mainScreenEnabled==ppu->layer[1].subScreenEnabled &&
    ppu->layer[0].mainScreenWindowed==ppu->layer[0].subScreenWindowed &&
    ppu->layer[1].mainScreenWindowed==ppu->layer[1].subScreenWindowed;
  if(same){memcpy(s_mode1SubPri,s_mode1MainPri,768);}
  else ppu_taR124Screen(ppu,true,s_mode1SubPri);
 }
}
#endif
