/* R107: exact cross-line reuse of the Mode2 BG3 offset words.
 * No work on full-BG-cache hits. Per-line R12 mask checks are unchanged.
 * Each reused table replays its page dependencies into the NEW line entry. */
#if defined(TA_R107_MODE2_OPT_TABLE) && TA_R107_MODE2_OPT_TABLE
#if !TA_R12_OFFSET_CACHE || !TA_R32_BG_LINE_CACHE || !TA_R71_MODE2_VRAM_STAMP
#error R107 needs R12, R32 and R71
#endif
static BgLayer s_taR107Bg3;
static uint8_t s_taR107Pages[64];
static uint32_t s_taR107Generations[64],s_taR107Stamp;
static unsigned s_taR107PageCount;
#if defined(TA_R107_OPT_PROFILE) && TA_R107_OPT_PROFILE
static uint64_t s_taR107Builds,s_taR107Reuses;
void ppu_taR107OptStats(uint64_t* builds,uint64_t* reuses){if(builds)*builds=s_taR107Builds;if(reuses)*reuses=s_taR107Reuses;}
#endif
static inline uint16_t ppu_taR107OffsetAddress(const BgLayer* b,int col,int row){
  const int x=col*8+b->hScroll,y=row*8+b->vScroll;
  const int bits=b->bigTiles?4:3,high=b->bigTiles?0x200:0x100;
  uint16_t adr=b->tilemapAdr+(((y>>bits)&31)<<5|((x>>bits)&31));
  if((x&high)&&b->tilemapWider)adr+=0x400;
  if((y&high)&&b->tilemapHigher)adr+=b->tilemapWider?0x800:0x400;
  return adr;
}
static void ppu_taR107PrepareOffsets(Ppu* ppu){
  const BgLayer* b=&ppu->bgLayer[2];
  /* Direct test/renderer callers also need subsequent writes tracked. */
  s_taR32CacheActive=true;
  bool hit=s_taR107OptValid && b->hScroll==s_taR107Bg3.hScroll &&
      b->vScroll==s_taR107Bg3.vScroll && b->tilemapAdr==s_taR107Bg3.tilemapAdr &&
      b->bigTiles==s_taR107Bg3.bigTiles && b->tilemapWider==s_taR107Bg3.tilemapWider &&
      b->tilemapHigher==s_taR107Bg3.tilemapHigher;
  if(hit && s_taR107Stamp!=s_taR71VramStamp)
    for(unsigned i=0;i<s_taR107PageCount;++i)
      if(s_taR107Generations[i]!=s_taR32PageGeneration[s_taR107Pages[i]]){hit=false;break;}
  if(!hit){
    uint32_t seen[8]={0};s_taR107OptValid=false;s_taR107PageCount=0;
    for(int col=0;col<32;++col){
      for(int row=0;row<2;++row){
        const uint16_t adr=ppu_taR107OffsetAddress(b,col,row);
        const unsigned page=(adr&0x7fffu)>>7;const uint32_t bit=1u<<(page&31);
        if(!(seen[page>>5]&bit)){
          seen[page>>5]|=bit;s_taR107Pages[s_taR107PageCount]=(uint8_t)page;
          s_taR107Generations[s_taR107PageCount++]=s_taR32PageGeneration[page];
        }
        if(row)s_taMode2VOffset[col]=ppu->vram[adr&0x7fff];
        else s_taMode2HOffset[col]=ppu->vram[adr&0x7fff];
      }
    }
    s_taR107Bg3=*b;s_taR107OptValid=true;
#if defined(TA_R107_OPT_PROFILE) && TA_R107_OPT_PROFILE
    ++s_taR107Builds;
#endif
  }else{
#if defined(TA_R107_OPT_PROFILE) && TA_R107_OPT_PROFILE
    ++s_taR107Reuses;
#endif
  }
  s_taR107Stamp=s_taR71VramStamp;
  /* Including stamp-bypass hits: otherwise a newly cached BG line could
   * omit OPT pages, then incorrectly survive a later VRAM/HDMA change. */
  for(unsigned i=0;i<s_taR107PageCount;++i)
    ppu_taR32Dependency((uint16_t)s_taR107Pages[i]<<7);
  s_taMode2OffsetMask=UINT32_MAX;
}
#endif
