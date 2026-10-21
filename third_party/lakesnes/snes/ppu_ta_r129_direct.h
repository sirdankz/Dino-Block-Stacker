/* R129: final full-8-bit palette lookup on NV2A, one presentation pass.
 * Eligibility is the original no-math/no-clipping direct-color branch.
 * Only resolved main indices are captured; all PPU evaluation stays exact. */
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
#if !TA_R127_GPU_INDEXED || !TA_R128_GPU_ATLAS_CACHE
#error R129 requires the existing indexed lifetime and atlas-cache baseline
#endif
static bool ppu_taR129Capture(Ppu*p,uint32_t*out,unsigned row,unsigned legacy){
 if(!ppu_taR127SnapshotPalette(p))return false;
 unsigned index=s_r127CurrentIndex;
 const uint64_t rows=(uint64_t)((8*index+1)<<16)*0x100000001ULL;
 for(unsigned x=0;x<256;x+=8){
  const uint8_t*src=s_taR94ReadMainPix+x;uint32_t*dst=out+x;
  __asm__ volatile(
   "movq (%[src]), %%mm0\n\t"
   "pxor %%mm7, %%mm7\n\t"
   "movq %%mm0, %%mm1\n\t"
   "punpcklbw %%mm7, %%mm0\n\t"
   "punpckhbw %%mm7, %%mm1\n\t"
   "movq %%mm0, %%mm2\n\t"
   "punpcklwd %%mm7, %%mm0\n\t"
   "punpckhwd %%mm7, %%mm2\n\t"
   "movq %%mm1, %%mm3\n\t"
   "punpcklwd %%mm7, %%mm1\n\t"
   "punpckhwd %%mm7, %%mm3\n\t"
   "pslld $24, %%mm0\n\tpslld $24, %%mm2\n\t"
   "pslld $24, %%mm1\n\tpslld $24, %%mm3\n\t"
   "por %[rows], %%mm0\n\tpor %[rows], %%mm2\n\t"
   "por %[rows], %%mm1\n\tpor %[rows], %%mm3\n\t"
   "movq %%mm0, 0(%[dst])\n\tmovq %%mm2, 8(%[dst])\n\t"
   "movq %%mm1, 16(%[dst])\n\tmovq %%mm3, 24(%[dst])"
   : : [src] "r"(src),[dst] "r"(dst),[rows] "m"(rows)
   : "mm0","mm1","mm2","mm3","mm7","memory");
 }
 __asm__ volatile("emms":::"memory");
 s_r127Alias[legacy]=&s_r127Palettes[s_r127Slot][index];
 s_r129Alias[legacy]=true;s_r129Rows[row]=true;s_r127Rows[row]=true;s_r126GpuRows[row]=true;
 return true;
}
#endif
