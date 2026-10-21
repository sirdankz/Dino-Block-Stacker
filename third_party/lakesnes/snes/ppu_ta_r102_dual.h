/* R102: Mode-2 BG main/sub writes share source load, transparency mask and
 * broadcasts. Priority comparisons and masked writes remain screen-specific.
 * Only full eight-pixel chunks visible on both screens use this helper.
 * No retained row state, timing change, OPT change, or altered cache keys. */
#if defined(TA_R102_MODE2_DUAL_PUT) && TA_R102_MODE2_DUAL_PUT
#if !TA_R88_PACKED_BG_COPY || !defined(ZAMN_R23_MMX_PPU) || defined(ZAMN_R32_PPU_HOISTS)
#error R102 requires the existing R88 plane layout and baseline MMX renderer
#endif
static const uint64_t s_taR102Broadcast[16] __attribute__((aligned(32)))={
0x0000000000000000ULL,0x0101010101010101ULL,0x0202020202020202ULL,0x0303030303030303ULL,0x0404040404040404ULL,0x0505050505050505ULL,0x0606060606060606ULL,0x0707070707070707ULL,0x0808080808080808ULL,0x0909090909090909ULL,0x0a0a0a0a0a0a0a0aULL,0x0b0b0b0b0b0b0b0bULL,0x0c0c0c0c0c0c0c0cULL,0x0d0d0d0d0d0d0d0dULL,0x0e0e0e0e0e0e0e0eULL,0x0f0f0f0f0f0f0f0fULL
};
static inline void ppu_taR102PutBoth(unsigned x,const uint8_t* src,int z,int layer,bool trackLayer){
 uint8_t* dst=s_mode1MainPri+x;
 if(trackLayer){
  __asm__ volatile(
    "movq (%[src]), %%mm0\n\t"
    "pxor %%mm1, %%mm1\n\t"
    "pcmpeqb %%mm0, %%mm1\n\t"
    "pcmpeqb %%mm6, %%mm6\n\t"
    "pxor %%mm1, %%mm6\n\t"
    "movq %[z], %%mm7\n\t"
    "movq %[layer], %%mm4\n\t"
    "movq 0(%[dst]), %%mm1\n\t"
    "movq %%mm7, %%mm2\n\t"
    "pcmpgtb %%mm1, %%mm2\n\t"
    "pand %%mm6, %%mm2\n\t"
    "movq %%mm7, %%mm3\n\t"
    "pxor %%mm1, %%mm3\n\t"
    "pand %%mm2, %%mm3\n\t"
    "pxor %%mm3, %%mm1\n\t"
    "movq %%mm1, 0(%[dst])\n\t"
    "movq 256(%[dst]), %%mm1\n\t"
    "movq %%mm0, %%mm3\n\t"
    "pxor %%mm1, %%mm3\n\t"
    "pand %%mm2, %%mm3\n\t"
    "pxor %%mm3, %%mm1\n\t"
    "movq %%mm1, 256(%[dst])\n\t"
    "movq 512(%[dst]), %%mm1\n\t"
    "movq %%mm4, %%mm3\n\t"
    "pxor %%mm1, %%mm3\n\t"
    "pand %%mm2, %%mm3\n\t"
    "pxor %%mm3, %%mm1\n\t"
    "movq %%mm1, 512(%[dst])\n\t"
    "movq 768(%[dst]), %%mm1\n\t"
    "movq %%mm7, %%mm2\n\t"
    "pcmpgtb %%mm1, %%mm2\n\t"
    "pand %%mm6, %%mm2\n\t"
    "movq %%mm7, %%mm3\n\t"
    "pxor %%mm1, %%mm3\n\t"
    "pand %%mm2, %%mm3\n\t"
    "pxor %%mm3, %%mm1\n\t"
    "movq %%mm1, 768(%[dst])\n\t"
    "movq 1024(%[dst]), %%mm1\n\t"
    "movq %%mm0, %%mm3\n\t"
    "pxor %%mm1, %%mm3\n\t"
    "pand %%mm2, %%mm3\n\t"
    "pxor %%mm3, %%mm1\n\t"
    "movq %%mm1, 1024(%[dst])\n\t"
    "movq 1280(%[dst]), %%mm1\n\t"
    "movq %%mm4, %%mm3\n\t"
    "pxor %%mm1, %%mm3\n\t"
    "pand %%mm2, %%mm3\n\t"
    "pxor %%mm3, %%mm1\n\t"
    "movq %%mm1, 1280(%[dst])\n\t"
    : : [dst] "r"(dst),[src] "r"(src),[z] "m"(s_taR102Broadcast[z&15]),[layer] "m"(s_taR102Broadcast[layer&15])
    : "mm0","mm1","mm2","mm3","mm4","mm6","mm7","memory");
 }else{
  __asm__ volatile(
    "movq (%[src]), %%mm0\n\t"
    "pxor %%mm1, %%mm1\n\t"
    "pcmpeqb %%mm0, %%mm1\n\t"
    "pcmpeqb %%mm6, %%mm6\n\t"
    "pxor %%mm1, %%mm6\n\t"
    "movq %[z], %%mm7\n\t"
    "movq 0(%[dst]), %%mm1\n\t"
    "movq %%mm7, %%mm2\n\t"
    "pcmpgtb %%mm1, %%mm2\n\t"
    "pand %%mm6, %%mm2\n\t"
    "movq %%mm7, %%mm3\n\t"
    "pxor %%mm1, %%mm3\n\t"
    "pand %%mm2, %%mm3\n\t"
    "pxor %%mm3, %%mm1\n\t"
    "movq %%mm1, 0(%[dst])\n\t"
    "movq 256(%[dst]), %%mm1\n\t"
    "movq %%mm0, %%mm3\n\t"
    "pxor %%mm1, %%mm3\n\t"
    "pand %%mm2, %%mm3\n\t"
    "pxor %%mm3, %%mm1\n\t"
    "movq %%mm1, 256(%[dst])\n\t"
    "movq 768(%[dst]), %%mm1\n\t"
    "movq %%mm7, %%mm2\n\t"
    "pcmpgtb %%mm1, %%mm2\n\t"
    "pand %%mm6, %%mm2\n\t"
    "movq %%mm7, %%mm3\n\t"
    "pxor %%mm1, %%mm3\n\t"
    "pand %%mm2, %%mm3\n\t"
    "pxor %%mm3, %%mm1\n\t"
    "movq %%mm1, 768(%[dst])\n\t"
    "movq 1024(%[dst]), %%mm1\n\t"
    "movq %%mm0, %%mm3\n\t"
    "pxor %%mm1, %%mm3\n\t"
    "pand %%mm2, %%mm3\n\t"
    "pxor %%mm3, %%mm1\n\t"
    "movq %%mm1, 1024(%[dst])\n\t"
    : : [dst] "r"(dst),[src] "r"(src),[z] "m"(s_taR102Broadcast[z&15])
    : "mm0","mm1","mm2","mm3","mm4","mm6","mm7","memory");
 }
}
#endif
