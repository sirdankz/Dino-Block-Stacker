/* R127: pack eight resolved main/sub indices with MMX, without CPU palette
 * gathers or a second per-pixel texture store. BG/OPT/OBJ/windows are unchanged.
 * Palette epochs and fixed color are captured before any subsequent guest write.
 * Overflow retains the R126 path; mixed formats are decoded by Finish. */
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
#if !TA_R126_GPU_SUBTRACT || !TA_R126_DIRECT_TARGET
#error R127 indexed output requires the complete R126 GPU/direct pipeline
#endif

static const uint64_t s_r127Windows[256]={
0x0ULL,0xffULL,0xff00ULL,0xffffULL,
0xff0000ULL,0xff00ffULL,0xffff00ULL,0xffffffULL,
0xff000000ULL,0xff0000ffULL,0xff00ff00ULL,0xff00ffffULL,
0xffff0000ULL,0xffff00ffULL,0xffffff00ULL,0xffffffffULL,
0xff00000000ULL,0xff000000ffULL,0xff0000ff00ULL,0xff0000ffffULL,
0xff00ff0000ULL,0xff00ff00ffULL,0xff00ffff00ULL,0xff00ffffffULL,
0xffff000000ULL,0xffff0000ffULL,0xffff00ff00ULL,0xffff00ffffULL,
0xffffff0000ULL,0xffffff00ffULL,0xffffffff00ULL,0xffffffffffULL,
0xff0000000000ULL,0xff00000000ffULL,0xff000000ff00ULL,0xff000000ffffULL,
0xff0000ff0000ULL,0xff0000ff00ffULL,0xff0000ffff00ULL,0xff0000ffffffULL,
0xff00ff000000ULL,0xff00ff0000ffULL,0xff00ff00ff00ULL,0xff00ff00ffffULL,
0xff00ffff0000ULL,0xff00ffff00ffULL,0xff00ffffff00ULL,0xff00ffffffffULL,
0xffff00000000ULL,0xffff000000ffULL,0xffff0000ff00ULL,0xffff0000ffffULL,
0xffff00ff0000ULL,0xffff00ff00ffULL,0xffff00ffff00ULL,0xffff00ffffffULL,
0xffffff000000ULL,0xffffff0000ffULL,0xffffff00ff00ULL,0xffffff00ffffULL,
0xffffffff0000ULL,0xffffffff00ffULL,0xffffffffff00ULL,0xffffffffffffULL,
0xff000000000000ULL,0xff0000000000ffULL,0xff00000000ff00ULL,0xff00000000ffffULL,
0xff000000ff0000ULL,0xff000000ff00ffULL,0xff000000ffff00ULL,0xff000000ffffffULL,
0xff0000ff000000ULL,0xff0000ff0000ffULL,0xff0000ff00ff00ULL,0xff0000ff00ffffULL,
0xff0000ffff0000ULL,0xff0000ffff00ffULL,0xff0000ffffff00ULL,0xff0000ffffffffULL,
0xff00ff00000000ULL,0xff00ff000000ffULL,0xff00ff0000ff00ULL,0xff00ff0000ffffULL,
0xff00ff00ff0000ULL,0xff00ff00ff00ffULL,0xff00ff00ffff00ULL,0xff00ff00ffffffULL,
0xff00ffff000000ULL,0xff00ffff0000ffULL,0xff00ffff00ff00ULL,0xff00ffff00ffffULL,
0xff00ffffff0000ULL,0xff00ffffff00ffULL,0xff00ffffffff00ULL,0xff00ffffffffffULL,
0xffff0000000000ULL,0xffff00000000ffULL,0xffff000000ff00ULL,0xffff000000ffffULL,
0xffff0000ff0000ULL,0xffff0000ff00ffULL,0xffff0000ffff00ULL,0xffff0000ffffffULL,
0xffff00ff000000ULL,0xffff00ff0000ffULL,0xffff00ff00ff00ULL,0xffff00ff00ffffULL,
0xffff00ffff0000ULL,0xffff00ffff00ffULL,0xffff00ffffff00ULL,0xffff00ffffffffULL,
0xffffff00000000ULL,0xffffff000000ffULL,0xffffff0000ff00ULL,0xffffff0000ffffULL,
0xffffff00ff0000ULL,0xffffff00ff00ffULL,0xffffff00ffff00ULL,0xffffff00ffffffULL,
0xffffffff000000ULL,0xffffffff0000ffULL,0xffffffff00ff00ULL,0xffffffff00ffffULL,
0xffffffffff0000ULL,0xffffffffff00ffULL,0xffffffffffff00ULL,0xffffffffffffffULL,
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
/* Revert already captured rows to R126's two exact color operands when the
 * bounded atlas fills. This retains full-frame GPU subtraction/expansion;
 * there is no CPU color-math detour for palette-heavy effects. */
static void ppu_taR127Abandon(Ppu*p){
 const unsigned field=p->evenFrame?0:239;
 for(unsigned y=0;y<224;y++)if(s_r127Rows[y]){
  const TaR127Palette*pal=s_r127Alias[field+y];
  uint32_t*a=(uint32_t*)(s_xboxFrameTarget+y*s_xboxFramePitch);
  uint32_t*b=(uint32_t*)(s_r126GpuSub+y*s_r126GpuPitch);
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
  if(s_r129Alias[field+y]){
   if(pal)for(unsigned x=0;x<256;x++)a[x]=pal->color[a[x]>>24];
   s_r129Alias[field+y]=false;s_r129Rows[y]=false;s_r126GpuRows[y]=false;
   s_r127Alias[field+y]=NULL;s_r126Second[field+y]=NULL;s_r127Rows[y]=false;
   continue;
  }
#endif
  if(pal)for(unsigned x=0;x<256;x++){
   uint32_t packed=a[x];unsigned sub=(packed>>8)&255u,row=packed&255u;
   const TaR127Palette*frame=pal-(((packed>>16)&255u)/8);
   b[x]=!row?0:(row==253u?frame[31].color[sub]:pal->color[sub])&0xf8f8f8u;
   a[x]=pal->color[packed>>24]&0xf8f8f8u;
  }
  s_r127Alias[field+y]=NULL;s_r126Second[field+y]=b;s_r127Rows[y]=false;
 }
 s_r127Enabled=false;
}
static bool ppu_taR127SnapshotPalette(Ppu*p){
 if(!s_r127Count || s_r127LastSerial!=s_r127PaletteSerial){
  unsigned found=0;
  for(;found<s_r127Count;found++)
   if(!memcmp(s_r127Palettes[s_r127Slot][found].color,s_mode1ColorXrgb,1024))break;
  if(found==s_r127Count){
   if(s_r127Count==TA_R127_PALETTES-1){ppu_taR127Abandon(p);return false;}
   memcpy(s_r127Palettes[s_r127Slot][s_r127Count++].color,s_mode1ColorXrgb,1024);
  }
  s_r127CurrentIndex=found;s_r127LastSerial=s_r127PaletteSerial;
 }
 return true;
}
static bool ppu_taR127Capture(Ppu*p,uint32_t*out,unsigned row,unsigned legacy){
 const uint32_t fixed=((uint32_t)p->fixedColorR<<19)|((uint32_t)p->fixedColorG<<11)|((uint32_t)p->fixedColorB<<3);
 if(!ppu_taR127SnapshotPalette(p))return false;
 if(!s_r127FixedCount || s_r127LastFixed!=fixed){
  if(s_r127FixedCount==256){ppu_taR127Abandon(p);return false;}
  s_r127FixedIndex=s_r127FixedCount++;
  s_r127Palettes[s_r127Slot][31].color[s_r127FixedIndex]=fixed;
  s_r127LastFixed=fixed;
 }
 const unsigned index=s_r127CurrentIndex;
 const uint64_t five=0x0505050505050505ULL;
 const uint64_t mainRow=(uint64_t)(8*index+1)*0x0101010101010101ULL;
 const uint64_t fixedRow=0xfdfdfdfdfdfdfdfdULL;
 const uint64_t fixedIndex=(uint64_t)s_r127FixedIndex*0x0101010101010101ULL;
 uint64_t enabled[6];unsigned count=0;
 for(unsigned l=0;l<6;l++)if(p->mathEnabled[l])enabled[count++]=(uint64_t)l*0x0101010101010101ULL;
 for(unsigned x=0;x<256;x+=8){
  uint64_t m,l,s,sl,w=s_r127Windows[(s_taWindowMask[5][x>>5]>>(x&31))&255u];
  memcpy(&m,s_taR94ReadMainPix+x,8);memcpy(&l,s_taR94ReadMainLayer+x,8);
  memcpy(&s,s_mode1ReadSubPix+x,8);memcpy(&sl,s_mode1ReadSubLayer+x,8);
  /* Explicit MMX: current RXDK Clang maps legacy intrinsics through SSE2. */
  __asm__ volatile(
   "movq %[m], %%mm0\n\t"
   "movq %[l], %%mm1\n\t"
   "pxor %%mm3, %%mm3\n\t"
   "pcmpeqb %%mm0, %%mm3\n\t"
   "movq %[five], %%mm2\n\t"
   "pxor %%mm1, %%mm2\n\t"
   "pand %%mm3, %%mm2\n\t"
   "pxor %%mm2, %%mm1\n\t"
   "pxor %%mm3, %%mm3\n\t"
   "movl %[count], %%ecx\n\t"
#if defined(__x86_64__)
   "mov %[enabled], %%rdx\n\t"
#else
   "mov %[enabled], %%edx\n\t"
#endif
   "test %%ecx, %%ecx\n\tjz 2f\n\t"
#if defined(__x86_64__)
   "1: movq (%%rdx), %%mm2\n\t"
#else
   "1: movq (%%edx), %%mm2\n\t"
#endif
   "pcmpeqb %%mm1, %%mm2\n\t"
   "por %%mm2, %%mm3\n\t"
#if defined(__x86_64__)
   "add $8, %%rdx\n\tdec %%ecx\n\tjnz 1b\n\t"
#else
   "add $8, %%edx\n\tdec %%ecx\n\tjnz 1b\n\t"
#endif
   "2: pand %[w], %%mm3\n\t"
   "movq %[sl], %%mm2\n\t"
   "pcmpeqb %[five], %%mm2\n\t"
   "movq %[s], %%mm1\n\t"
   "pxor %%mm4, %%mm4\n\t"
   "pcmpeqb %%mm1, %%mm4\n\t"
   "por %%mm4, %%mm2\n\t"
   "movq %[fi], %%mm4\n\t"
   "pxor %%mm1, %%mm4\n\t"
   "pand %%mm2, %%mm4\n\t"
   "pxor %%mm1, %%mm4\n\t"
   "movq %[fr], %%mm5\n\t"
   "pxor %[mr], %%mm5\n\t"
   "pand %%mm2, %%mm5\n\t"
   "pxor %[mr], %%mm5\n\t"
   "pand %%mm5, %%mm3\n\t"
   "movq %%mm4, %%mm2\n\t"
   "movq %%mm3, %%mm1\n\t"
   "punpcklbw %%mm2, %%mm3\n\t"
   "punpckhbw %%mm2, %%mm1\n\t"
   "movq %[mr], %%mm4\n\t"
   "movq %%mm4, %%mm5\n\t"
   "punpcklbw %%mm0, %%mm4\n\t"
   "punpckhbw %%mm0, %%mm5\n\t"
   "movq %%mm3, %%mm6\n\t"
   "punpcklwd %%mm4, %%mm3\n\t"
   "punpckhwd %%mm4, %%mm6\n\t"
   "movq %%mm3, 0(%[out])\n\t"
   "movq %%mm6, 8(%[out])\n\t"
   "movq %%mm1, %%mm6\n\t"
   "punpcklwd %%mm5, %%mm1\n\t"
   "punpckhwd %%mm5, %%mm6\n\t"
   "movq %%mm1, 16(%[out])\n\t"
   "movq %%mm6, 24(%[out])"
   : : [m] "m"(m),[l] "m"(l),[s] "m"(s),[sl] "m"(sl),[w] "m"(w),
       [five] "m"(five),[mr] "m"(mainRow),[fr] "m"(fixedRow),[fi] "m"(fixedIndex),
       [count] "m"(count),[enabled] "r"(enabled),[out] "r"(out+x)
   : "ecx","edx","mm0","mm1","mm2","mm3","mm4","mm5","mm6","memory");
 }
 __asm__ volatile("emms":::"memory");s_r127Alias[legacy]=&s_r127Palettes[s_r127Slot][index];
 s_r127Rows[row]=true;s_r126GpuRows[row]=true;return true;
}
#endif
