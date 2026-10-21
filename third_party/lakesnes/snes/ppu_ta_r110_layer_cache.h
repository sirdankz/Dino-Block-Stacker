/* R110: retain the first Mode 2 background independently of BG2 scroll.
 * Called only on a full-background cache miss, before any BG/sprite writes.
 * Physical-row indexing preserves scanline-dependent vertical OPT behavior.
 * Every reused page is also recorded in the enclosing combined cache. */
#if defined(TA_R110_MODE2_FIRST_BG_CACHE) && TA_R110_MODE2_FIRST_BG_CACHE
#if !TA_R32_BG_LINE_CACHE || !TA_R71_MODE2_VRAM_STAMP || !TA_R88_PACKED_BG_COPY
#error R110 requires R32, R71 and the R88 contiguous scratch planes
#endif
#if defined(TA_R110_CACHE_PROFILE) && TA_R110_CACHE_PROFILE
static uint64_t s_taR110Counts[4];
void ppu_taR110Counts(uint64_t a[4]){memcpy(a,s_taR110Counts,sizeof s_taR110Counts);}
#define TA_R110_COUNT(i) (++s_taR110Counts[i])
#else
#define TA_R110_COUNT(i) ((void)0)
#endif
static void ppu_taR110DrawFirstBG(Ppu* ppu,int row,int y,
    const uint8_t zbg[3][2],bool needSub,bool trackLayer) {
  /* Unsupported/direct callers keep the original generation path. */
  if((unsigned)row>=PPU_LINES || !s_taR32Recording) {
    memset(s_mode1MainPri,0,256);memset(s_mode1MainPix,0,256);
    if(needSub){memset(s_mode1SubPri,0,256);memset(s_mode1SubPix,0,256);}
    ppu_xboxMode1DrawLayerR68(ppu,0,y,zbg,needSub,trackLayer);return;
  }
  TaR32BGLine* const outer=s_taR32Recording;
  TaR32BGLine* const e=&s_taR110FirstBG[row];
  TaR32BGKey key;
  memset(&key,0,sizeof key);
  key.bg[0]=ppu->bgLayer[0];key.bg[2]=ppu->bgLayer[2];
  key.layer[0]=ppu->layer[0];key.window[0]=ppu->windowLayer[0];
  key.edges[0]=ppu->window1left;key.edges[1]=ppu->window1right;
  key.edges[2]=ppu->window2left;key.edges[3]=ppu->window2right;
  key.needSub=needSub;key.trackLayer=trackLayer;
  bool hit=e->valid && memcmp(&e->key,&key,sizeof key)==0;
  TA_R110_COUNT(0);
  if(hit && e->validatedVramStamp!=s_taR71VramStamp)
    for(unsigned i=0;i<e->count;++i)
      if(e->generation[i]!=s_taR32PageGeneration[e->page[i]]){hit=false;TA_R110_COUNT(2);break;}
  if(hit){
    memcpy(s_mode1MainPri,e->mainPri,trackLayer?768u:512u);
    if(needSub)memcpy(s_mode1SubPri,e->subPri,768u);
    e->validatedVramStamp=s_taR71VramStamp;
    TA_R110_COUNT(1);
  }else{
    /* Isolate dependency recording, then restore the outer recording state. */
    uint32_t seen[8];memcpy(seen,s_taR32Seen,sizeof seen);
    memset(s_taR32Seen,0,sizeof s_taR32Seen);
    e->valid=false;e->key=key;e->count=0;s_taR32Recording=e;
    memset(s_mode1MainPri,0,256);memset(s_mode1MainPix,0,256);
    if(needSub){memset(s_mode1SubPri,0,256);memset(s_mode1SubPix,0,256);}
    ppu_xboxMode1DrawLayerR68(ppu,0,y,zbg,needSub,trackLayer);
    ppu_taR32StoreBG(needSub,trackLayer);
    s_taR32Recording=outer;memcpy(s_taR32Seen,seen,sizeof seen);
    TA_R110_COUNT(3);
  }
  /* BG2 can share map/OPT/character pages with BG1. Deduplicate via R32.
   * On stamp-bypass hits these dependencies are still mandatory. */
  for(unsigned i=0;i<e->count;++i)ppu_taR32Dependency((uint16_t)e->page[i]<<7);
}
#endif
