/* Tetris Attack OG Xbox R52: native dispatch and WRAM bus improvements. */
#include "ta_platform.h"
#include "ta_native.h"
#include "ta_profile.h"
#include "ta_profile_interval.h"
#include "ta_r113_cpu_diag.h"
#include "ta_dispatch_trace.h"
extern "C" {
#include "ta_r61_workers.h"
#include "ta_r62_sprite.h"
#include "ta_r72_record.h"
#include "ta_r73_chain.h"
#include "ta_r74_fields.h"
#include "ta_r77_dispatch.h"
#include "ta_r78_quiet.h"
#include "ta_r80_sprite.h"
#include "ta_r81_animation.h"
#include "ta_r82_cost.h"
#include "ta_r120_cpu_cost.h"
}
#if TA_R82_COST_ON
static void ta_r60_emit_cost82(const char* line){TA_Log("%s",line);}
#endif
#ifndef TA_R60_NATIVE_STORE
#define TA_R60_NATIVE_STORE 0
#endif
#if TA_R60_DISPATCH_TRACE
static void ta_r60_emit(const char* line){TA_Log("%s",line);}
#endif
#if defined(TA_R57_REGION_PROFILE) && TA_R57_REGION_PROFILE && !(defined(TA_R56_FALLBACK_PC_PROFILE) && TA_R56_FALLBACK_PC_PROFILE)
#error R57 region reporting requires R56 fallback sampling
#endif
#include "ta_diagnostics.h"
extern "C" {
#include "snes.h"
#if defined(TA_R53_XBOX_COUNTERS) && TA_R53_XBOX_COUNTERS
#include "ta_accumulate_loop.h"
#endif
}
#ifndef TA_LOG_INTERVAL_FRAMES
#if defined(TA_R33_AB_DEMO) && TA_R33_AB_DEMO
#define TA_LOG_INTERVAL_FRAMES 600
#else
#define TA_LOG_INTERVAL_FRAMES 60
#endif
#endif
#if TA_LOG_INTERVAL_FRAMES <= 0
#error TA_LOG_INTERVAL_FRAMES must be positive
#endif
#ifndef TA_R37_STREAM_UPLOAD
#define TA_R37_STREAM_UPLOAD 0
#endif
#ifndef TA_R36_APU_SYNC_FAST
#define TA_R36_APU_SYNC_FAST 0
#endif
#ifndef TA_R35_PROFILE
#define TA_R35_PROFILE 0
#endif
#ifndef TA_R33_AB_DEMO
#define TA_R33_AB_DEMO 0
#endif
#ifndef TA_R43_LINE_OUTPUT_CACHE
#define TA_R43_LINE_OUTPUT_CACHE 0
#endif
#ifndef TA_R44_DMA_CLOCK_FAST
#define TA_R44_DMA_CLOCK_FAST 0
#endif
#ifndef TA_R44_DMA_VRAM_FAST
#define TA_R44_DMA_VRAM_FAST 0
#endif
#ifndef TA_R44_SUBSCREEN_CLIP
#define TA_R44_SUBSCREEN_CLIP 0
#endif
#ifndef TA_R44_DMA_VRAM_PROFILE
#define TA_R44_DMA_VRAM_PROFILE 0
#endif
#ifndef TA_R45_INLINE_CODE_FETCH
#define TA_R45_INLINE_CODE_FETCH 0
#endif
#ifndef TA_R46_CPU_DISPATCH
#define TA_R46_CPU_DISPATCH 0
#endif
#ifndef TA_R46_INLINE_WRAM
#define TA_R46_INLINE_WRAM 0
#endif
#ifndef TA_R59_APU_CLOCK
#define TA_R59_APU_CLOCK 0
#endif
#ifndef TA_R59_SPC_FETCH_BATCH
#define TA_R59_SPC_FETCH_BATCH 0
#endif
#ifndef TA_R59_SPC_BLOCKS
#define TA_R59_SPC_BLOCKS 0
#endif
#ifndef TA_R59_SPC_RUN
#define TA_R59_SPC_RUN 0
#endif
#ifndef TA_R59_CPU_FRAME
#define TA_R59_CPU_FRAME 0
#endif
#ifndef TA_R88_PACKED_BG_COPY
#define TA_R88_PACKED_BG_COPY 0
#endif
#ifndef TA_R58_IDLE_SLOTS
#define TA_R58_IDLE_SLOTS 0
#endif
#ifndef TA_R58_COUNTER_BLOCK
#define TA_R58_COUNTER_BLOCK 0
#endif
#ifndef TA_R52_ACCUMULATE_LOOP
#define TA_R52_ACCUMULATE_LOOP 0
#endif
#ifndef TA_R51_GAME_BLOCKS
#define TA_R51_GAME_BLOCKS 0
#endif
#ifndef TA_R50_PACKED_COLOR
#define TA_R50_PACKED_COLOR 0
#endif
#ifndef TA_R50_SPC_BUS
#define TA_R50_SPC_BUS 0
#endif
#ifndef TA_R49_TIMER_BUS
#define TA_R49_TIMER_BUS 0
#endif
#ifndef TA_R48_QUIET_SERVICE
#define TA_R48_QUIET_SERVICE 0
#endif
#ifndef TA_R47_FAST_REJECT
#define TA_R47_FAST_REJECT 0
#endif
#if defined(TA_R103_GUARD_AUDIT) && TA_R103_GUARD_AUDIT
extern "C" void ppu_taR103Mode2Counts(uint64_t counters[5]);
#endif
static uint8_t rom[1048576];
static uint8_t pixels[512*480*4];
static uint8_t activePixels[512*224*4];
static int16_t audio[800*2];
static uint64_t fnv64(const uint8_t *p,unsigned int len){
  uint64_t h=0xcbf29ce484222325ULL;
  for(unsigned i=0;i<len;i++)h=(h^p[i])*0x100000001b3ULL;
  return h;
}
static void status(const char *s){TA_Log("%s",s);}
static void error(const char *s){status(s);}
static bool load_game(void){
  FILE *f=NULL;
#if defined(TA_DINO_BRAND) && TA_DINO_BRAND
  f=fopen("D:\\game.sfc","rb");if(!f)f=fopen("D:\\tetris_attack.sfc","rb");
#else
  f=fopen("D:\\tetris_attack.sfc","rb");
#endif // read-only disc data
  if(!f){
#if defined(TA_DINO_BRAND) && TA_DINO_BRAND
   error("[DBS] Add your compatible game.sfc beside default.xbe.\n");
#else
   error("[TA R0] Missing D:\\tetris_attack.sfc\n");
#endif
   return false;}
  size_t n=fread(rom,1,sizeof(rom),f);
  bool extra=fgetc(f)!=EOF;
  fclose(f);
  if(n!=sizeof(rom)||extra){error("[TA R0] Incorrect ROM size\n");return false;}
  if(fnv64(rom,sizeof(rom))!=0xe448796ad235bc90ULL){error("[TA R0] Incorrect ROM fingerprint\n");return false;}
  if(memcmp(rom+0x7fc0,"TETRIS ATTACK",13)!=0||rom[0x7fd5]!=0x30){
    error("[TA R0] Incorrect SNES header\n");return false;
  }
  return true;
}
static uint32_t ta_r19_fnv32(const void *ptr, size_t size) {
  const uint8_t *p=(const uint8_t*)ptr;
  uint32_t h=2166136261u;
  for(size_t i=0;i<size;i++)h=(h^p[i])*16777619u;
  return h;
}
#if defined(TA_VS_NATIVE_GAME) && TA_VS_NATIVE_GAME
extern "C" void TA_LanGame_Run(const uint8_t*,unsigned);
#endif
#if defined(TA_LAN_PREVIEW) && TA_LAN_PREVIEW
extern "C" void TA_LanPreview_Run(const uint8_t*,unsigned);
#endif
int main(){
#if defined(TA_R55_SERVICE_CHAIN) && TA_R55_SERVICE_CHAIN
  status("[TA R59] boot: integrated core scheduling candidates; features reported below\n");
#else
  status("[TA R53] boot: R52-compatible native Mode 2 renderer\n");
#endif
  if(!load_game())return 1;
  if(FAILED(TA_Video_Init())){error("[TA R0] D3D init failed\n");return 2;}
  if(FAILED(TA_Audio_Init()))status("[TA R0] Audio init failed (muted)\n");
  TA_Log("[TA R59 INPUT MODE] live=%u controller_fix=1\n",(unsigned)!TA_R33_AB_DEMO);
  TA_Input_Init();
#if defined(TA_VS_NATIVE_GAME) && TA_VS_NATIVE_GAME
  TA_LanGame_Run(rom,sizeof rom);
#endif
#if defined(TA_LAN_PREVIEW) && TA_LAN_PREVIEW
  TA_LanPreview_Run(rom,sizeof rom);
#endif
  Snes *s=snes_init();
  if(!s||!snes_loadRom(s,rom,sizeof(rom))){error("[TA R18] SNES load failed\n");return 3;}
#if TA_R60_DISPATCH_TRACE
  ta_r60_trace_reset();
#endif
  bool native_ok=ta_native_init(rom,sizeof(rom));
#if defined(TA_R105_DB81_R73) && TA_R105_DB81_R73
#if defined(TA_R103_GUARD_AUDIT) && TA_R103_GUARD_AUDIT
  TA_Log("[TA R105 DB81] enabled=1 bank81_only=1 additional_resumes=0 audit=1 fps_not_comparable=1\n");
#else
  TA_Log("[TA R105 DB81] enabled=1 bank81_only=1 additional_resumes=0 audit=0 fps_not_comparable=0 EXPERIMENTAL\n");
#endif
#endif
#if defined(TA_R115_R108_REJECT_DETAIL) && TA_R115_R108_REJECT_DETAIL
  TA_Log("[TA R115 R108 REJECTION AUDIT] enabled=1 no_guard_relaxation=1 r105_db81=1 r112=off fps_not_comparable=1\n");
#endif
#if defined(TA_R114_R108_DIAG) && TA_R114_R108_DIAG
  TA_Log("[TA R114 MODE2 GUARD AUDIT] enabled=1 r105_db81=1 r73_reasons=1 r108_counts=1 r112=off fps_not_comparable=1\n");
#endif
#if defined(TA_R103_GUARD_AUDIT) && TA_R103_GUARD_AUDIT
  TA_Log("[TA R103 COVERAGE] enabled=1 diagnostic_only=1 fps_not_comparable=1 baseline=R102 source, R97 performance benchmark report_interval=600\n");
#if defined(TA_R104_GUARD_DETAIL) && TA_R104_GUARD_DETAIL
  TA_Log("[TA R104 CPU GUARD DETAIL] enabled=1 window=600 diagnostic_only=1 fps_not_comparable=1\n");
#endif
#endif
#if defined(TA_R18_WINDOW_CHUNK) && TA_R18_WINDOW_CHUNK
  const char* chunkFeature = "on";
#else
  const char* chunkFeature = "off";
#endif
  TA_Log("[TA R59] rom verified; native_sites=%lu native_guard=%s lookup=6x32KB opcode_pages=on window_bitsets=on window_chunks=%s log_path=%s\n",
         (unsigned long)ta_native_enabled_sites(),native_ok?"enabled":"fallback",chunkFeature,TA_LogPath());
#if defined(TA_R31_ROW_PACKED) && TA_R31_ROW_PACKED
  TA_Log("[TA R31 ROW] packed_rebuild=on cache_bytes=32768 profiler_defaults=off rollback=TA_R31_ROW_PACKED=0\n");
#else
  TA_Log("[TA R31 ROW] packed_rebuild=off cache_bytes=32768 reference=R30\n");
#endif
  TA_Log("[TA R59 FEATURES] bg_cache=%u delay_loop=%u color_pair_cache=%u pc_sampling=%u ppu_timers=%u counters=%u ab_demo=%u window_output=%u\n",
    (unsigned)TA_R32_BG_LINE_CACHE,(unsigned)TA_R32_NATIVE_DELAY_LOOP,(unsigned)TA_R32_COLOR_PAIR_CACHE,
    (unsigned)TA_R2_SAMPLE_PC,(unsigned)TA_R2_PROFILE_TIMERS,(unsigned)TA_R32_DIAGNOSTICS,(unsigned)TA_R33_AB_DEMO,(unsigned)TA_R34_WINDOW_OUTPUT);
  TA_Log("[TA R59 COPY] stream_upload=%u native256=%u native_bytes=229376 legacy_bytes=458752 fence=before_unlock\n",(unsigned)TA_R37_STREAM_UPLOAD,(unsigned)TA_R39_NATIVE256);
  TA_Log("[TA R59 SYNC] apu_sync_fast=%u\n",(unsigned)TA_R36_APU_SYNC_FAST);
  TA_Log("[TA R59 LOG] interval_frames=%u\n",(unsigned)TA_LOG_INTERVAL_FRAMES);
#if defined(TA_R56_FALLBACK_PC_PROFILE) && TA_R56_FALLBACK_PC_PROFILE
  TA_Log("[TA R56 FALLBACK] enabled=1 sample_rate=1/1024 reference_opcodes_only top_exact=8 fps_not_comparable=1\n");
#endif
  TA_Log("[TA R59 DISPATCH] decoded_dispatch=%u descriptor_bytes=%u native_bus_timing=unchanged\n",(unsigned)TA_R40_DECODED_DISPATCH,(unsigned)(TA_R40_DECODED_DISPATCH?393216:196608));
  TA_Log("[TA R59 BUS] bus_end_limit=%u table_bytes=%u native_bus_timing=unchanged\n",(unsigned)TA_R41_BUS_END_LIMIT,(unsigned)(TA_R41_BUS_END_LIMIT?2716:0));
  TA_Log("[TA R59 OUTPUT] line_output_cache=%u cache_bytes=%u equality=exact\n",(unsigned)TA_R43_LINE_OUTPUT_CACHE,(unsigned)ppu_taR43OutputCacheBytes());
  snes_setPixelFormat(s,pixelFormatXRGB);
  /* Interval counters; reset AFTER each report without touching machine state. */
  snes_perfReset();
  ppu_xboxPerfReset();
  TA_Log("[TA R60 CONFIG] native_store=%u dispatch_trace=%u handler=85:87C1 dispatcher=reference fps_not_comparable=%u\n",(unsigned)TA_R60_NATIVE_STORE,(unsigned)TA_R60_DISPATCH_TRACE,(unsigned)TA_R60_DISPATCH_TRACE);
  TA_Log("[TA R61 CONFIG] native_workers=%u worker_coverage=%u supported_handlers=42 fps_not_comparable=%u\n",(unsigned)TA_R61_NATIVE_WORKERS,(unsigned)TA_R61_WORKER_COVERAGE,(unsigned)(TA_R61_WORKER_COVERAGE||TA_R60_DISPATCH_TRACE));
  TA_Log("[TA R62 CONFIG] native_sprite=%u coverage=%u stage_profile=%u fps_not_comparable=%u\n",(unsigned)TA_R62_NATIVE_SPRITE,(unsigned)TA_R62_COVERAGE,(unsigned)TA_R35_PROFILE,(unsigned)(TA_R62_COVERAGE||TA_R35_PROFILE||TA_R60_DISPATCH_TRACE));
#if defined(TA_R63_STAGE_REPORT) && TA_R63_STAGE_REPORT
  TA_Log("[TA R63.1 PROFILE] stage_report=1 stage_sampling=%u bg_deep=%u old_diagnostics=%u interval_frames=%u sampled_ticks_not_fps=1\n",
    (unsigned)TA_R28_STAGE_PROFILE,(unsigned)TA_R29_BG_TILE_PROFILE,(unsigned)TA_R32_DIAGNOSTICS,(unsigned)TA_LOG_INTERVAL_FRAMES);
#endif
#if defined(TA_R64_CACHE_REPORT) && TA_R64_CACHE_REPORT
  TA_Log("[TA R64 CACHE AUDIT] row_cache_profile=1 line_cache_profile=1 bg_deep=0 sampled_fps_not_comparable=1\n");
#endif
#if defined(TA_R65_ROW_CACHE_WAYS)
  TA_Log("[TA R65 CACHE] ways=%u original_hash_sets=2048 row_entry_bytes=16 row_cache_bytes=%u comparison_clean=%u\n",
    (unsigned)TA_R65_ROW_CACHE_WAYS,
    (unsigned)(2048u * 16u * (unsigned)TA_R65_ROW_CACHE_WAYS),
#if defined(TA_R65_CACHE_REPORT) && TA_R65_CACHE_REPORT
    0u);
#else
    1u);
#endif
#endif
#if defined(TA_R66_VICTIM_BITS)
  TA_Log("[TA R66 VICTIM] enabled=%u bits=%u victim_bytes=%u primary_bytes=32768 clean=%u primary_unchanged=1\n",
    (unsigned)(TA_R66_VICTIM_BITS > 0), (unsigned)TA_R66_VICTIM_BITS,
    (unsigned)((TA_R66_VICTIM_BITS > 0) ? (16u << TA_R66_VICTIM_BITS) : 0u),
#if defined(TA_R66_VICTIM_REPORT) && TA_R66_VICTIM_REPORT
    0u);
#else
    1u);
#endif
#endif
#if defined(TA_R67_BG_FAST)
  TA_Log("[TA R67 BGFAST] enabled=%u offsets=line_local_32 columns=predecoded default_y=hoisted vertical_offsets=exact row_hash=hoisted primary_cache=one_way fps_not_comparable=0\n",
         (unsigned)TA_R67_BG_FAST);
#endif
#if defined(TA_R68_MODE1_ONLY)
  TA_Log("[TA R68 MODE SPLIT] enabled=%u mode1=R67_BG_FAST mode2=R66_ORIGINAL primary=one_way clean=1\n",
         (unsigned)TA_R68_MODE1_ONLY);
#if defined(TA_R69_MMX_NO_WIN_SKIP)
  TA_Log("[TA R69 MMX] mode1_no_winning_lanes_skip=%u mode2_compositor=R68_ORIGINAL clean=1\n",
         (unsigned)(TA_R69_MMX_NO_WIN_SKIP != 0));
#endif
#if defined(TA_R70_MODE1_LINE_CACHE)
  TA_Log("[TA R70 MODE1 BG] enabled=%u mode2=R32_original bg3_key=exact priority_order=exact cache_separate=1 clean=1\n",
         (unsigned)(TA_R70_MODE1_LINE_CACHE != 0));
#endif
#if defined(TA_R71_MODE2_VRAM_STAMP)
  TA_Log("[TA R71 STAMP] enabled=%u mode1=R70_unchanged mode2=write_stamp_precheck audit=%u fps_not_comparable=%u\n",
         (unsigned)(TA_R71_MODE2_VRAM_STAMP != 0),
#if defined(TA_R71_STAMP_PROFILE) && TA_R71_STAMP_PROFILE
         1u,1u
#else
         0u,0u
#endif
         );
#endif

#endif
  ta_profile_reset();
#if defined(TA_R72_NATIVE_RECORD)
  TA_Log("[TA R72 RECORD] enabled=%u entry=80:BC33 ops=17 renderer=R71 event_fenced=1 coverage=%u fps_not_comparable=%u\n",(unsigned)(TA_R72_NATIVE_RECORD!=0),(unsigned)TA_R72_COVERAGE,(unsigned)TA_R72_COVERAGE);
#endif
#if TA_R73_CHAIN_ON
  TA_Log("[TA R73 CHAIN] enabled=1 entries=14 segments=8 renderer=R71 r72_record=off event_fenced=1 coverage=%u fps_not_comparable=%u\n",(unsigned)TA_R73_COVERAGE,(unsigned)TA_R73_COVERAGE);
#endif

#if TA_R74_FIELDS_ON
  TA_Log("[TA R74 FIELDS] enabled=1 entry=82:D9D4 slots=6 entries=8 renderer=R73 coverage=%u fps_not_comparable=%u\n",(unsigned)TA_R74_COVERAGE,(unsigned)TA_R74_COVERAGE);
#endif
#if TA_R77_ENTRY_FIX_ON
  TA_Log("[TA R77 ENTRY] enabled=1 sprite_entries=14 added_bd_entries=6 renderer=R74_unchanged coverage=%u fps_not_comparable=%u\n",(unsigned)TA_R73_COVERAGE,(unsigned)TA_R73_COVERAGE);
#endif
#if TA_R82_COST_ON
  ta_r82_reset();
  TA_Log("[TA R82 COST PROFILE] enabled=1 period=60 phase=rotating whole_operation_scopes=1 callbacks=tracked nested_hardware=exclusive observer_ticks=reported fps_not_comparable=1\n");
#endif
#if TA_R83_RESIDUAL_PROFILE
  TA_Log("[TA R83.1 RESIDUAL FIX] enabled=1 cpu_hooks=wired r55_attempts=wired native_ppu_scopes=wired generic_pixel_scope=wired fps_not_comparable=1\n");
#endif
#if TA_R84_COMPOSE_PROFILE
  TA_Log("[TA R84 COMPOSE PROFILE] enabled=1 production_scopes=1 cache_exclusive=1 output_math_store_fused=1 sample_period=60 fps_not_comparable=1\n");
#endif
#if TA_R87_CACHE_MAP
  TA_Log("[TA R87.1 FOCUSED REFPC] enabled=1 pages=16 direct_index=1 bounded_lookup=1 filtered_ticks=reported fps_not_comparable=1\n");
#endif
#if TA_R87_CACHE_MAP
  TA_Log("[TA R87 TRACE] enabled=1 r86_key=%u r85_scalar=%u sampled_one_of_60=1 stages=key,dependency,restore,miss,store refpc=top16 fps_not_comparable=1\n",
   (unsigned)(TA_R86_MODE2_DIRECT_KEY!=0),(unsigned)(TA_R85_MODE1_SCALAR_LUT!=0));
#endif
#if defined(TA_R85_MODE1_SCALAR_LUT)
  TA_Log("[TA R85 MODE1 SCALAR] enabled=%u r84_scalar_fallback=1 r70_r71_preserved=1 cost_profile=0 fps_not_comparable=0\n",(unsigned)(TA_R85_MODE1_SCALAR_LUT!=0));
#endif
#if defined(TA_R86_MODE2_DIRECT_KEY)
  TA_Log("[TA R86 MODE2 KEY] enabled=%u r70_r71_preserved=1 r85_mode1=%u cost_profile=0 fps_not_comparable=0\n",(unsigned)(TA_R86_MODE2_DIRECT_KEY!=0),(unsigned)(TA_R85_MODE1_SCALAR_LUT!=0));
  TA_Log("[TA R88 PACKED RESTORE] enabled=%u cache_mode2_only=1 reference_fallback=1 diagnostics=off fps_not_comparable=0\n",(unsigned)(TA_R88_PACKED_BG_COPY!=0));
#endif
#if defined(TA_R90_ACTIVE_PROLOGUE)
  TA_Log("[TA R90 ACTIVE PROLOGUE] enabled=%u pc=84:8162 active_value=0,1 exit=84:8172 fallback=reference diagnostics=off fps_not_comparable=0\n",
    (unsigned)(TA_R90_ACTIVE_PROLOGUE!=0));
#endif
#if defined(TA_R102_MODE2_DUAL_PUT) && TA_R102_MODE2_DUAL_PUT
#if defined(TA_R112_NATIVE_9AC4) && TA_R112_NATIVE_9AC4
#if defined(TA_R113_CPU_DIAG) && TA_R113_CPU_DIAG
  TA_Log("[TA R112 NATIVE RECORD] enabled=1 entry=80:9AC4 through=9B1D indirect=reference sha256=verified bus=reference r111=off profile=R113 fps_not_comparable=1\n");
#else
  TA_Log("[TA R112 NATIVE RECORD] enabled=1 entry=80:9AC4 through=9B1D indirect=reference sha256=verified bus=reference r111=off profile=off fps_not_comparable=0\n");
#endif
#elif defined(TA_R112_NATIVE_9AC4) && !TA_R112_NATIVE_9AC4
#if defined(TA_R113_CPU_DIAG) && TA_R113_CPU_DIAG
  TA_Log("[TA R112 NATIVE RECORD] enabled=0 baseline=R110 r111=off profile=R113 fps_not_comparable=1\n");
#else
  TA_Log("[TA R112 NATIVE RECORD] enabled=0 baseline=R110 r111=off fps_not_comparable=0\n");
#endif
#endif
#if defined(TA_R111_SPRITE_FREE_SPANS) && TA_R111_SPRITE_FREE_SPANS
  TA_Log("[TA R111 SPRITE-FREE SPANS] enabled=1 mode2=1 fallback=R109 on nonzero_obj_span db81=1 profile=off fps_not_comparable=0\n");
#endif
#if defined(TA_R120_CLEAN_CONTROL) && TA_R120_CLEAN_CONTROL
  TA_Log("[TA R120 CLEAN] baseline=R117 cpu_cost_sample=0 r119=off r118=off fps_not_comparable=0\n");
#endif
#if defined(XBOX_PORT) && TA_R120_CPU_COST_SAMPLE
  TA_Log("[TA R120 CPU+PPU COST] baseline=R117 reference_opcode_sample=1/1024 ranked_pc=16 ranked_op=16 nested_ppu=R82_R83_R84 fps_not_comparable=1\n");
  ta_r120_sample_reset();
#endif
#if defined(TA_R117_R108_CPU_WINDOW_STACK) && TA_R117_R108_CPU_WINDOW_STACK
  TA_Log("[TA R117 R108+CPU WINDOW] enabled=1 r108=1 r109=off r110=off cpu_window=R116 r111=off r112=off profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R116_CPU_BUS_WINDOW) && TA_R116_CPU_BUS_WINDOW
  TA_Log("[TA R116 CPU BUS WINDOW] enabled=1 baseline=R110 instruction_local=1 bus_clocks=exact event_fences=unchanged db81=1 profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R126_DSP_RATE_MASK) && TA_R126_DSP_RATE_MASK
  TA_Log("[TA R126 NATIVE PIPELINE] dsp_rates=%u cpu_prefix=%u direct_mode2=%u gpu_prototype=%u guest_timing=exact profiling=off fps_not_comparable=0\n",(unsigned)TA_R126_DSP_RATE_MASK,(unsigned)TA_R126_CPU_PREFIX,(unsigned)TA_R126_DIRECT_TARGET,(unsigned)TA_R126_GPU_SUBTRACT);
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
  TA_Log("[TA R127 INDEXED PIPELINE] gpu_palette_lookup=1 mmx_index_pack=1 r126_preserved=1 guest_timing=exact profiling=off fps_not_comparable=0\n");
#if defined(TA_R128_GPU_ATLAS_CACHE) && TA_R128_GPU_ATLAS_CACHE
  TA_Log("[TA R128 ATLAS CACHE] enabled=1 exact_tiles=1 no_change_lock=skip r127_preserved=1 profiling=off fps_not_comparable=0\n");
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
  TA_Log("[TA R129 DIRECT PALETTE] enabled=1 mode2=1 full8=1 passes=1 cpu_fallback=1 profiling=off fps_not_comparable=0\n");
#endif
#endif
#endif
#endif
#if defined(TA_R125_MODE2_DIRECT_FIXED) && TA_R125_MODE2_DIRECT_FIXED
  TA_Log("[TA R125 MODE2 DIRECT FIXED] enabled=1 baseline=R124 table_free_full_brightness=1 exact_fixed_subtract=1 profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R124_MODE2_STREAM) && TA_R124_MODE2_STREAM
  TA_Log("[TA R124 MODE2 STREAM] enabled=1 baseline=R117 raw_bg_rows=1 mmx_screen_resolve=1 r121=0 r122=0 r123=0 profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R110_MODE2_FIRST_BG_CACHE) && TA_R110_MODE2_FIRST_BG_CACHE
  TA_Log("[TA R110 MODE2 LAYER CACHE] enabled=1 baseline=R109 independent_bg1=1 exact_vram_dependencies=1 db81=1 profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R109_FUSED_MODE2_COMPOSE) && TA_R109_FUSED_MODE2_COMPOSE
  TA_Log("[TA R109 MODE2 COMPOSE] enabled=1 baseline=R108 fused_obj_output=1 cached_bg=readonly db81=1 profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R108_MODE2_RECORDS) && TA_R108_MODE2_RECORDS
  TA_Log("[TA R108 MODE2 RECORDS] enabled=1 baseline=R107 bank=88 entries=26 callbacks=reference db81=1 profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R107_MODE2_OPT_TABLE) && TA_R107_MODE2_OPT_TABLE
  TA_Log("[TA R107 MODE2 OFFSETS] enabled=1 baseline=R106 db81=1 page_validation=exact profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R106_MODE2_CHAR_EPOCH) && TA_R106_MODE2_CHAR_EPOCH
  TA_Log("[TA R106 MODE2 CHAR CACHE] enabled=1 baseline=R105 db81=1 character_bytes=32 profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R101_SPRITE_RESUME) && TA_R101_SPRITE_RESUME
  TA_Log("[TA R104 NATIVE SPRITE RESUME] enabled=1 r101=1 r102=1 guard_audit=0 fps_not_comparable=0 experimental=1\n");
#else
  TA_Log("[TA R102 MODE2 DUAL] enabled=1 baseline=R97 mode1=reference r101=0 profiling=off fps_not_comparable=0\n");
#endif
#endif
#if defined(TA_R101_SPRITE_RESUME) && TA_R101_SPRITE_RESUME
  TA_Log("[TA R101 SPRITE RESUME] enabled=1 continuation_entries=15 baseline=R97 r99=0 r100=0 profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R100_NATIVE_PIXEL_LOOP) && TA_R100_NATIVE_PIXEL_LOOP
  TA_Log("[TA R100 NATIVE PIXELS] enabled=1 r99_palette_lanes=0 r97_window_memo=1 width=256 profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R99_PALETTE_LANES) && TA_R99_PALETTE_LANES
  TA_Log("[TA R99 PALETTE LANES] enabled=1 r98_first_bg=0 r97_window_memo=1 profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R98_FIRST_BG_WRITE) && TA_R98_FIRST_BG_WRITE
  TA_Log("[TA R98 FIRST BG] enabled=1 r97_window_memo=1 profiling=off fps_not_comparable=0\n");
#endif
#if defined(TA_R93_WINDOW_SPANS) && TA_R93_WINDOW_SPANS
  TA_Log("[TA R93 RENDER] enabled=1 mode1_color_runs=1 mode2_readonly_sub_bg=1 reference_fallback=1 diagnostics=off fps_not_comparable=0\n");
#endif
#if defined(TA_R94_READONLY_MAIN_BG) && TA_R94_READONLY_MAIN_BG
  TA_Log("[TA R94 MAIN VIEW] enabled=1 mode2_bg_hit=1 no_main_obj_writes=1 fallback=R93 diagnostics=off fps_not_comparable=0\n");
#endif
#if defined(TA_R96_MODE2_EARLY_OCCLUSION) && TA_R96_MODE2_EARLY_OCCLUSION
  TA_Log("[TA R96 EARLY OCCLUSION] enabled=1 mode2_hidden_8px_before_tile_fetch=1 exact_windows=1 fallback=R94 diagnostics=off fps_not_comparable=0\n");
#endif
#if defined(TA_R92_FAST_MAP_ROUTING) && TA_R92_FAST_MAP_ROUTING
  TA_Log("[TA R92 TRANSFER MAP] enabled=1 route=existing_R47_descriptor db=7E_or_7F r90_prologue=off diagnostics=off fps_not_comparable=0\n");
#endif
#if defined(TA_R91_TRANSFER_LOOP) && !(defined(TA_R92_FAST_MAP_ROUTING) && TA_R92_FAST_MAP_ROUTING)
  TA_Log("[TA R91 TRANSFER LOOP] enabled=%u pc=80:A5E1 6ops_per_iteration=1 data_bank=7E fallback=reference r90_prologue=off fps_not_comparable=0\n",(unsigned)(TA_R91_TRANSFER_LOOP!=0));
#endif
#if TA_R81_ANIMATION_ON
  TA_Log("[TA R81 ANIMATION] enabled=1 entry=89:A36A entries=10 descriptor_setup_count=1 callbacks=reference coverage=%u fps_not_comparable=%u\n",(unsigned)TA_R81_COVERAGE,(unsigned)TA_R81_COVERAGE);
#endif
#if TA_R80_SPRITE_ON
  TA_Log("[TA R80 SPRITE] enabled=1 entry=80:C0C8 entries=15 mode1_flip_clip_attributes=1 coverage=%u fps_not_comparable=%u\n",(unsigned)TA_R80_COVERAGE,(unsigned)TA_R80_COVERAGE);
#endif
#if TA_R78_QUIET_ON
  TA_Log("[TA R78 QUIET] enabled=1 entry=86:9B2A entries=5 active_fields=reference renderer=R77_unchanged coverage=%u fps_not_comparable=%u\n",(unsigned)TA_R78_COVERAGE,(unsigned)TA_R78_COVERAGE);
#endif
  TA_Log("[TA R59 NATIVE DMA] clock_fast=%u vram_fast=%u source_map=existing_direct_pages per_byte_timing=preserved\n",(unsigned)TA_R44_DMA_CLOCK_FAST,(unsigned)TA_R44_DMA_VRAM_FAST);
  TA_Log("[TA R59 CODE FETCH] inline=%u extra_map_bytes=0 extra_table_bytes=0 bus_timing=preserved\n",(unsigned)TA_R45_INLINE_CODE_FETCH);
  TA_Log("[TA R59 CPU BUS] single_probe=%u inline_wram=%u renderer=R46_preserved added_maps=0 added_tables=0 native_sites_added=0 timing=preserved\n",(unsigned)TA_R46_CPU_DISPATCH,(unsigned)TA_R46_INLINE_WRAM);
  TA_Log("[TA R59 ENTRY] fast_reject=%u bank_map=reused rom_guard=reused new_native_sites=0\n",(unsigned)TA_R47_FAST_REJECT);
  TA_Log("[TA R59 SERVICE] quiet_service=%u game_routine=80:9086 exact_clocks=1 event_fenced=1\n",(unsigned)TA_R48_QUIET_SERVICE);
  TA_Log("[TA R59 TIMER BUS] timer_bus=%u transactions=original irq_edges=reference event_table=reused new_cache_bytes=0\n",(unsigned)TA_R49_TIMER_BUS);
  TA_Log("[TA R59 CORE] packed_color=%u spc_bus=%u extra_cache_bytes=0 dsp_timer_edges=reference\n",(unsigned)TA_R50_PACKED_COLOR,(unsigned)TA_R50_SPC_BUS);
  TA_Log("[TA R59 GAME] game_blocks=%u routines=84:C12D,84:8162,84:C13A exact_clocks=1 event_fenced=1 new_maps=0\n",(unsigned)TA_R51_GAME_BLOCKS);
  TA_Log("[TA R59 LOOP] accumulate_loop=%u routine=80:AEAD event_fenced=1 max_instructions=48 new_maps=0\n",(unsigned)TA_R52_ACCUMULATE_LOOP);
#if defined(TA_R53_GROUP_FENCE) && TA_R53_GROUP_FENCE
  TA_Log("[TA R53 FENCE] precompute=1 routine=80:AEAD clocks=reference branch=exact\n");
#if defined(TA_R54_XBOX_PROFILE) && TA_R54_XBOX_PROFILE
  TA_Log("[TA R54 PROFILE] routine_counts=1 release_overhead=profiling_build_only interval_frames=600\n");
#endif
#else
  TA_Log("[TA R53 FENCE] precompute=0 routine=80:AEAD clocks=reference branch=exact\n");
#endif
#if defined(TA_R55_SERVICE_CHAIN)
  TA_Log("[TA R55 CHAIN] enabled=%u routines=84:C12D,84:8162,84:C13A separate_fences=1 max_opcodes=48 counters=off\n",(unsigned)TA_R55_SERVICE_CHAIN);
#endif
  TA_Log("[TA R59 NATIVE] idle_slots=%u counter_block=%u indirect_tasks=original new_cache_bytes=0\n",(unsigned)TA_R58_IDLE_SLOTS,(unsigned)TA_R58_COUNTER_BLOCK);
  TA_Log("[TA R59 SCHEDULER] cpu_frame=%u apu_clock=%u spc_run=%u spc_blocks=%u spc_fetch_batch=%u timers=exact dsp=original guest_bus_order=original\n",(unsigned)TA_R59_CPU_FRAME,(unsigned)TA_R59_APU_CLOCK,(unsigned)TA_R59_SPC_RUN,(unsigned)TA_R59_SPC_BLOCKS,(unsigned)TA_R59_SPC_FETCH_BATCH);
#if defined(TA_R56_FALLBACK_PC_PROFILE) && TA_R56_FALLBACK_PC_PROFILE
  TA_Log("[TA R59 PROFILE WINDOW] collect_frames=600 reset_frames=600 fps_log_frames=60 fps_not_comparable=1\n");
#endif
#if defined(TA_R113_CPU_DIAG) && TA_R113_CPU_DIAG
  TA_Log("[TA R113 CPU DIAG] enabled=1 r112=%u fallback_pc_sample=1/1024 report=60frames top_pc=600frames counters=host_path_frequency bus=cpu_helpers_only fps_not_comparable=1\n",(unsigned)TA_R112_NATIVE_9AC4);
#endif
#if !defined(TA_NO_LOG) || !TA_NO_LOG
  unsigned long elapsedStart=GetTickCount();
  unsigned count=0;
  uint32_t intervalNativeStart=ta_native_site_hits();
  unsigned long coreMs=0,videoMs=0,audioMs=0;
#endif
#if TA_R35_PROFILE
  uint64_t coreTicks=0;
  uint32_t lastCacheHits=0,lastCacheMisses=0;
  status("[TA R59 PROFILE] diagnostics=on renderer_sample_stride=32 pc_samples=frequency_not_time apu_ticks=measured fps_not_comparable_to_clean_build\n");
#endif
#if TA_R33_AB_DEMO
  unsigned mode2Frames=0;
  status("[TA R59 AB] Controlled attract demo: controllers ignored; compare matching intervals and state hashes. Reset console to exit.\n");
#endif
#if !defined(TA_NO_LOG) || !TA_NO_LOG
  unsigned long directFrames=0,activeFrames=0,legacyFrames=0,packMs=0,native256Frames=0;
#endif
  for(;;){
    uint16_t j[2];
#if TA_R33_AB_DEMO
    /* Exact same released-button route as the validated host attract demo. */
    j[0]=j[1]=0;
#else
    TA_Input_Poll(j);
#endif
    for(int p=0;p<2;p++)for(int b=0;b<12;b++)snes_setButtonState(s,p+1,b,(j[p]&(1u<<b))!=0);
#if !defined(TA_NO_LOG) || !TA_NO_LOG
    unsigned long tick=GetTickCount();
#endif
#if TA_R35_PROFILE
    const uint64_t coreStart=__builtin_readcyclecounter();
#endif
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
    TA_Video_BeginR126(s->ppu);
#endif
    snes_runFrame(s);
#if TA_R35_PROFILE
    coreTicks+=__builtin_readcyclecounter()-coreStart;
#endif
#if !defined(TA_NO_LOG) || !TA_NO_LOG
    coreMs+=(unsigned long)(GetTickCount()-tick);
    tick=GetTickCount();
#endif
    /* On linear D3D textures, copy the 224 already-rendered SNES scanlines
       straight into the locked texture. When ineligible/unsupported, use the
       tested R5 staging route and ultimately the R4 480-line fallback. */
    bool submitted=false;
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
    submitted=TA_Video_EndR126(s->ppu);
#endif
    if(!submitted)submitted=TA_Video_SubmitNative224Direct(s->ppu);
    if(submitted){
#if !defined(TA_NO_LOG) || !TA_NO_LOG
      ++native256Frames;
#endif
    } else submitted=TA_Video_SubmitActive224Direct(s->ppu);
    if(submitted){
#if !defined(TA_NO_LOG) || !TA_NO_LOG
      ++directFrames;
#endif
    } else {
      if(TA_Video_ActiveAvailable()){
#if !defined(TA_NO_LOG) || !TA_NO_LOG
        unsigned long packStart=GetTickCount();
#endif
        if(ppu_taCopyActive224(s->ppu,activePixels,sizeof(activePixels))){
#if !defined(TA_NO_LOG) || !TA_NO_LOG
          packMs+=(unsigned long)(GetTickCount()-packStart);
#endif
          submitted=TA_Video_SubmitActive224(activePixels);
        }
      }
#if !defined(TA_NO_LOG) || !TA_NO_LOG
      if(submitted)++activeFrames;
      else {snes_setPixels(s,pixels);TA_Video_Submit(pixels);++legacyFrames;}
#else
      if(!submitted){snes_setPixels(s,pixels);TA_Video_Submit(pixels);}
#endif
    }
#if !defined(TA_NO_LOG) || !TA_NO_LOG
    videoMs+=(unsigned long)(GetTickCount()-tick);
    tick=GetTickCount();
#endif
    snes_setSamples(s,audio,800);TA_Audio_Submit(audio);
#if !defined(TA_NO_LOG) || !TA_NO_LOG
    audioMs+=(unsigned long)(GetTickCount()-tick);
    count++;
#endif
#if TA_R33_AB_DEMO
    if(s->ppu->mode==2) ++mode2Frames;
#endif
#if !defined(TA_NO_LOG) || !TA_NO_LOG
    if(count%TA_LOG_INTERVAL_FRAMES==0){

      unsigned long now=GetTickCount(),ms=(unsigned long)(now-elapsedStart);
      TA_Log("[TA R59] frames=%lu wall_ms=%lu fps_x100=%lu core_ms=%lu video_ms=%lu audio_ms=%lu pc=%02X:%04X mode=%u native_hits=%lu native_sites=%lu\n",
        (unsigned long)count,ms,(unsigned long)(ms?((((uint64_t)TA_LOG_INTERVAL_FRAMES*100000u)/ms)):0u),
        coreMs,videoMs,audioMs,(unsigned)s->cpu->k,(unsigned)s->cpu->pc,
        (unsigned)s->ppu->mode,(unsigned long)ta_native_site_hits(),
        (unsigned long)ta_native_enabled_sites());
#if defined(TA_R126_DIRECT_TARGET) && TA_R126_DIRECT_TARGET
      unsigned long r126Counts[4];TA_Video_R126Counts(r126Counts);
      TA_Log("[TA R126 OUTPUT] rgb_direct=%lu gpu=%lu fallback=%lu gpu_qualified=%lu cumulative=1\n",r126Counts[0],r126Counts[1],r126Counts[2],r126Counts[3]);
#if defined(TA_R127_GPU_INDEXED) && TA_R127_GPU_INDEXED
      unsigned long r127Counts[2];TA_Video_R127Counts(r127Counts);
      TA_Log("[TA R127 OUTPUT] indexed_gpu=%lu indexed_qualified=%lu cumulative=1\n",r127Counts[0],r127Counts[1]);
#if defined(TA_R128_GPU_ATLAS_CACHE) && TA_R128_GPU_ATLAS_CACHE
      unsigned long r128Counts[4];TA_Video_R128Counts(r128Counts);
      TA_Log("[TA R128 OUTPUT] atlas_frames=%lu unchanged_frames=%lu tiles_written=%lu r127_tiles=%lu cumulative=1\n",r128Counts[0],r128Counts[1],r128Counts[2],r128Counts[3]);
#if defined(TA_R129_GPU_DIRECT_PALETTE) && TA_R129_GPU_DIRECT_PALETTE
      unsigned long r129Counts[4];TA_Video_R129Counts(r129Counts);
      TA_Log("[TA R129 OUTPUT] direct_palette_gpu=%lu qualified=%lu tiles_written=%lu atlas_unchanged=%lu cumulative=1\n",r129Counts[0],r129Counts[1],r129Counts[2],r129Counts[3]);
#endif
#endif
#endif
#endif
#if defined(TA_R113_CPU_DIAG) && TA_R113_CPU_DIAG
      {
        TaR113CpuCounters d;
        ta_r113_cpu_read_reset(&d);
        TA_Log("[TA R113 CPU] end_frame=%lu mode=%u dispatch=%lu ref_ops=%lu r47_try=%lu r47_accept=%lu worker_try=%lu worker_accept=%lu r112_enter=%lu r112_accept=%lu r112_ops=%lu r112_reject_cpu_rom=%lu r112_reject_bus_frame=%lu r112_budget48=%lu cumulative=0 fps_not_comparable=1\n",
         (unsigned long)count,(unsigned)s->ppu->mode,
         (unsigned long)d.frame_dispatches,(unsigned long)d.reference_opcodes,
         (unsigned long)d.native_candidate_attempts,(unsigned long)d.native_decoded_accepts,
         (unsigned long)d.worker_attempts,(unsigned long)d.worker_accepts,
         (unsigned long)d.r112_entries,(unsigned long)d.r112_accepted,
         (unsigned long)d.r112_opcodes,(unsigned long)d.r112_reject_cpu_rom,
         (unsigned long)d.r112_reject_bus_frame,(unsigned long)d.r112_budget_exits);
        TA_Log("[TA R113 BUS] end_frame=%lu fetch=%lu data_read=%lu data_write=%lu idle=%lu wait_idle=%lu scope=cpu.c_helpers_only fps_not_comparable=1\n",
         (unsigned long)count,(unsigned long)d.bus_opcode_fetches,
         (unsigned long)d.bus_data_reads,(unsigned long)d.bus_data_writes,
         (unsigned long)d.bus_idles,(unsigned long)d.bus_wait_idles);
      }
#endif
#if TA_R60_DISPATCH_TRACE
      if(count%600==0 && count<=7800)ta_r60_trace_report(ta_r60_emit);
#endif
#if TA_R61_WORKERS_ON && TA_R61_WORKER_COVERAGE
      if(count%600==0)for(unsigned i=0;i<ta_native_r61_worker_count();++i) {
        uint32_t hits=ta_native_r61_worker_hits(i);
        if(hits)TA_Log("[TA R61 WORKER] frames=%lu pc=85:%04X hits=%lu cumulative=1 fps_not_comparable=1\n",(unsigned long)count,ta_native_r61_worker_pc(i),(unsigned long)hits);
      }
#endif
#if defined(TA_R62_STAGE_PROFILE) && TA_R62_STAGE_PROFILE
      {const uint64_t p=snes_perfPpuTicks(),a=snes_perfApuTicks();
       TA_Log("[TA R62 CORE] end_frame=%lu interval_frames=%u core_ticks=%llu ppu_ticks=%llu apu_ticks=%llu remaining_ticks=%llu fps_not_comparable=1\n",(unsigned long)count,(unsigned)TA_LOG_INTERVAL_FRAMES,(unsigned long long)coreTicks,(unsigned long long)p,(unsigned long long)a,(unsigned long long)(coreTicks>=p+a?coreTicks-p-a:0));coreTicks=0;}
#endif
#if TA_R62_SPRITE_ON && TA_R62_COVERAGE
      if(count%600==0)for(unsigned b=0;b<2;++b){
        uint32_t attempts=0,hits=0;ta_native_r62_counts(b,&attempts,&hits);
        TA_Log("[TA R62 SPRITE] frames=%lu block=%u pc=80:%04X attempts=%lu hits=%lu cumulative=1 fps_not_comparable=1\n",(unsigned long)count,b,b?0xbdca:0xbdb6,(unsigned long)attempts,(unsigned long)hits);
      }
#endif
#if defined(XBOX_PORT) && TA_R120_CPU_COST_SAMPLE
      if(count%600u==0u){
        TA_Log("[TA R120 COST WINDOW] end_frame=%lu interval=600 samples=%lu sampled_cycles=%llu fps_not_comparable=1\n",(unsigned long)count,(unsigned long)ta_r120_sample_count(),(unsigned long long)ta_r120_sample_cycles());
        for(unsigned rank=0;rank<16;rank++){
          uint32_t hits=0;uint64_t cycles=0;uint8_t op=0;
          uint32_t pc=ta_r120_top_pc(rank,&hits,&cycles,&op);
          if(pc==UINT32_MAX || !hits)break;
          TA_Log("[TA R120 COST PC] frame=%lu rank=%u pc=%02X:%04X op=%02X sample_hits=%lu sample_cycles=%llu\n",(unsigned long)count,rank+1u,(unsigned)(pc>>16),(unsigned)(pc&0xffffu),op,(unsigned long)hits,(unsigned long long)cycles);
        }
        for(unsigned rank=0;rank<16;rank++){
          uint32_t hits=0;uint64_t cycles=0;
          uint32_t op=ta_r120_top_opcode(rank,&hits,&cycles);
          if(op==UINT32_MAX||!hits)break;
          TA_Log("[TA R120 COST OP] frame=%lu rank=%u opcode=%02X sample_hits=%lu sample_cycles=%llu\n",(unsigned long)count,rank+1u,op,(unsigned long)hits,(unsigned long long)cycles);
        }
        ta_r120_sample_reset();
      }
#endif
#if defined(TA_R56_FALLBACK_PC_PROFILE) && TA_R56_FALLBACK_PC_PROFILE
      if(ta_profile_interval_report_due(count)) {
        TA_Log("[TA R56 FALLBACK] end_frame=%lu sampled=%lu collisions=%lu\n",
           (unsigned long)count,(unsigned long)ta_profile_samples(),
           (unsigned long)ta_profile_exact_collisions());
#if defined(TA_R57_REGION_PROFILE) && TA_R57_REGION_PROFILE
        /* R57: four densest 256-byte regions among interpreter-only sampled
           opcodes. Diagnostic-only; no SNES writes, no changed clocks. */
        for(unsigned regionRank=0; regionRank<4u; ++regionRank) {
          uint32_t regionHits=0;
          const uint32_t region=ta_profile_top_region(regionRank,&regionHits);
          if(region==UINT32_MAX || !regionHits)break;
          TA_Log("[TA R57 REGION] end_frame=%lu rank=%u region=%02X:%04X samples=%lu\n",
            (unsigned long)count,regionRank+1u,
            (unsigned)(region>>16),(unsigned)(region&0xffffu),(unsigned long)regionHits);
        }
#endif
        for(unsigned rank=0; rank<8u; ++rank) {
          uint32_t hits=0;uint8_t db=0,flags=0;uint16_t dp=0;
          const uint32_t pc=ta_profile_top_exact(rank,&hits,&db,&dp,&flags);
          if(pc==UINT32_MAX || !hits)break;
          TA_Log("[TA R56 PC] rank=%u pc=%02X:%04X samples=%lu db=%02X dp=%04X mx_e=%u%u%u\n",
            rank+1u,(unsigned)(pc>>16),(unsigned)(pc&0xffffu),
            (unsigned long)hits,(unsigned)db,(unsigned)dp,
            (unsigned)(flags&1u),(unsigned)((flags>>1)&1u),(unsigned)((flags>>2)&1u));
        }
      }
#endif
#if defined(TA_R54_XBOX_PROFILE) && TA_R54_XBOX_PROFILE
      /* Cumulative counts sampled only once per 600 frames. Do not compare
         FPS from this instrumented build against non-instrumented releases. */
      if(count%600u==0u) {
        uint32_t hot[4]={0,0,0,0};
        uint32_t attempts=0,accepts=0,iterations=0;
        ta_native_r54_hot_counts(hot);
        ta_native_r53_loop_counts(&attempts,&accepts,&iterations);
        TA_Log("[TA R54 HOT] end_frame=%lu mode=%u service=%lu c12d=%lu b8162=%lu c13a=%lu loop_attempts=%lu loop_accepts=%lu loop_iterations=%lu\n",
          (unsigned long)count,(unsigned)s->ppu->mode,
          (unsigned long)hot[0],(unsigned long)hot[1],
          (unsigned long)hot[2],(unsigned long)hot[3],
          (unsigned long)attempts,(unsigned long)accepts,(unsigned long)iterations);
      }
#endif
#if defined(TA_R53_XBOX_COUNTERS) && TA_R53_XBOX_COUNTERS
      {
        uint32_t attempts=0,accepts=0,iterations=0;
        ta_native_r53_loop_counts(&attempts,&accepts,&iterations);
        TA_Log("[TA R53 COVERAGE] end_frame=%lu attempts=%lu accepts=%lu iterations=%lu\n",
          (unsigned long)count,(unsigned long)attempts,(unsigned long)accepts,
          (unsigned long)iterations);
      }
#endif
      TA_Log("[TA R59 WIDTH] end_frame=%lu native256_frames=%lu\n",(unsigned long)count,native256Frames);
      native256Frames=0;
#if TA_R33_AB_DEMO
      /* Evidence is collected AFTER capturing wall/core/video/audio time.
         Do not charge export, hash or logging time to the next interval. */
      snes_setPixels(s,pixels);
      Cpu* c=s->cpu;
      TA_Log("[TA R59 AB] end_frame=%lu mode2_frames=%u cpu=%02X:%04X a=%04X x=%04X y=%04X sp=%04X dp=%04X db=%02X cznivd=%u%u%u%u%u%u mx=%u%u e=%u int=%u%u%u wr=%08lX vr=%08lX cg=%08lX oa=%08lX video=%08lX pcm=%08lX\n",
        (unsigned long)count,mode2Frames,(unsigned)c->k,(unsigned)c->pc,
        (unsigned)c->a,(unsigned)c->x,(unsigned)c->y,(unsigned)c->sp,(unsigned)c->dp,(unsigned)c->db,
        (unsigned)c->c,(unsigned)c->z,(unsigned)c->n,(unsigned)c->i,(unsigned)c->v,(unsigned)c->d,
        (unsigned)c->mf,(unsigned)c->xf,(unsigned)c->e,
        (unsigned)c->intWanted,(unsigned)c->nmiWanted,(unsigned)c->irqWanted,
        (unsigned long)ta_r19_fnv32(s->ram,sizeof(s->ram)),
        (unsigned long)ta_r19_fnv32(s->ppu->vram,sizeof(s->ppu->vram)),
        (unsigned long)ta_r19_fnv32(s->ppu->cgram,sizeof(s->ppu->cgram)),
        (unsigned long)ta_r19_fnv32(s->ppu->oam,sizeof(s->ppu->oam)),
        (unsigned long)ta_r19_fnv32(pixels,sizeof(pixels)),
        (unsigned long)ta_r19_fnv32(audio,sizeof(audio)));
      mode2Frames=0;
      if(count==3600) status("[TA R59 AB] Six intervals complete; save this log before running the other ISO.\n");
#endif
#if TA_R32_DIAGNOSTICS
      TA_Log("[TA R14 NATIVE] end_frame=%lu sites=%lu total=%lu inx=%lu iny=%lu dex=%lu clc=%lu\n",
        (unsigned long)count,(unsigned long)ta_native_enabled_sites(),
        (unsigned long)ta_native_site_hits(),
        (unsigned long)ta_native_opcode_hits(0),
        (unsigned long)ta_native_opcode_hits(1),
        (unsigned long)ta_native_opcode_hits(2),
        (unsigned long)ta_native_opcode_hits(3));
      TA_Log("[TA R14 STACK_BRANCH] end_frame=%lu bne=%lu beq=%lu php=%lu plp=%lu plb=%lu rtl=%lu\n",
        (unsigned long)count,
        (unsigned long)ta_native_opcode_hits(14),
        (unsigned long)ta_native_opcode_hits(15),
        (unsigned long)ta_native_opcode_hits(16),
        (unsigned long)ta_native_opcode_hits(17),
        (unsigned long)ta_native_opcode_hits(18),
        (unsigned long)ta_native_opcode_hits(19));
      TA_Log("[TA R14 BLOCK] end_frame=%lu calls=%lu instructions=%lu max_length=%lu additional_dispatches_saved=%lu\n",
        (unsigned long)count,
        (unsigned long)ta_native_microblock_calls(),
        (unsigned long)ta_native_microblock_instructions(),
        (unsigned long)ta_native_microblock_max_length(),
        (unsigned long)(ta_native_microblock_instructions() - ta_native_microblock_calls()));
      TA_Log("[TA R14 CPU] end_frame=%lu implied_new=%lu transfer=%lu rep=%lu bit_imm=%lu sep=%lu total=%lu\n",
        (unsigned long)count,
        (unsigned long)(ta_native_opcode_hits(4)+ta_native_opcode_hits(5)),
        (unsigned long)(ta_native_opcode_hits(6)+ta_native_opcode_hits(7)+ta_native_opcode_hits(8)+ta_native_opcode_hits(11)+ta_native_opcode_hits(12)),
        (unsigned long)ta_native_opcode_hits(9),
        (unsigned long)ta_native_opcode_hits(10),
        (unsigned long)ta_native_opcode_hits(13),
        (unsigned long)ta_native_site_hits());
      unsigned long uploadMs=0,drawMs=0,presentMs=0,videoFrames=0;
      TA_Video_ReadReset(&uploadMs,&drawMs,&presentMs,&videoFrames);
      TA_Log("[TA R14 NEWCPU] end_frame=%lu lda_abs=%lu and_imm=%lu cmp_imm=%lu phb=%lu asla=%lu bra=%lu\n",
        (unsigned long)count,
        (unsigned long)ta_native_opcode_hits(20),
        (unsigned long)ta_native_opcode_hits(21),
        (unsigned long)ta_native_opcode_hits(22),
        (unsigned long)ta_native_opcode_hits(23),
        (unsigned long)ta_native_opcode_hits(24),
        (unsigned long)ta_native_opcode_hits(25));
      TA_Log("[TA R14 VIDEO] end_frame=%lu upload_ms=%lu draw_ms=%lu present_ms=%lu video_frames=%lu total_video_ms=%lu\n",
        (unsigned long)count,uploadMs,drawMs,presentMs,videoFrames,videoMs);
      TA_Log("[TA R14 TRANSFER] end_frame=%lu direct_texture_frames=%lu staged_224_frames=%lu legacy_480_frames=%lu staged_pack_ms=%lu\n",
        (unsigned long)count,directFrames,activeFrames,legacyFrames,packMs);
      directFrames=activeFrames=legacyFrames=packMs=0;
      /* These RDTSC ticks are for relative core attribution, not wall-time
         milliseconds. On xemu the guest counter may not represent 733 MHz. */
      const uint64_t pticks=snes_perfPpuTicks();
#if TA_R35_PROFILE
      const uint64_t aticks=snes_perfApuTicks();
      uint64_t uploadTicks[3]={0};
      TA_Video_ReadResetUploadTicks(uploadTicks);
      TA_Log("[TA R59 UPLOAD] end_frame=%lu unbind_lock_ticks=%llu copy_swizzle_ticks=%llu unlock_ticks=%llu\n",
        (unsigned long)count,(unsigned long long)uploadTicks[0],(unsigned long long)uploadTicks[1],(unsigned long long)uploadTicks[2]);
      TA_Log("[TA R59 CORE] end_frame=%lu core_ticks=%llu ppu_ticks=%llu apu_ticks=%llu remaining_core_ticks=%llu remaining_includes=cpu_dma_scheduler_timer_overhead\n",
        (unsigned long)count,(unsigned long long)coreTicks,(unsigned long long)pticks,
        (unsigned long long)aticks,(unsigned long long)(coreTicks>=pticks+aticks?coreTicks-pticks-aticks:0));
      uint32_t cacheHits=0,cacheMisses=0,windowWords[3]={0};
      ppu_taR32CacheStats(&cacheHits,&cacheMisses);
      ppu_taR35WindowStats(windowWords,true);
      const uint32_t hits=cacheHits-lastCacheHits,misses=cacheMisses-lastCacheMisses;
      TA_Log("[TA R59 CACHE] end_frame=%lu bg_hits=%lu bg_misses=%lu bg_hit_percent_x100=%lu\n",
        (unsigned long)count,(unsigned long)hits,(unsigned long)misses,
        (unsigned long)((hits+misses)?(uint64_t)hits*10000/(hits+misses):0));
      TA_Log("[TA R59 OUTPUT] end_frame=%lu direct_words=%lu math_words=%lu mixed_words=%lu pixels_per_word=32\n",
        (unsigned long)count,(unsigned long)windowWords[0],(unsigned long)windowWords[1],(unsigned long)windowWords[2]);
      lastCacheHits=cacheHits;lastCacheMisses=cacheMisses;coreTicks=0;
#endif
      /* APU ticks are not instrumented in Release to avoid per-call overhead. */
      TA_Log("[TA R14 CLOCK] end_frame=%lu exact_shortsteps=%lu fallback=original_scheduler\n", (unsigned long)count, (unsigned long)snes_taR14ShortSteps());
#if defined(TA_R25_INLINE_BUS_SHORTSTEP) && TA_R25_INLINE_BUS_SHORTSTEP
      TA_Log("[TA R25 BUS] end_frame=%lu inline_shortsteps=%lu all_r14_shortsteps=%lu fallback=exact_scheduler\n",
        (unsigned long)count,(unsigned long)snes_taR25InlineBusShortSteps(),
        (unsigned long)snes_taR14ShortSteps());
#endif
#endif /* TA_R32_DIAGNOSTICS: report the sampled R28/R29 stages independently. */
#if TA_R32_DIAGNOSTICS || (defined(TA_R63_STAGE_REPORT) && TA_R63_STAGE_REPORT)
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
      {
        for(unsigned mode=1;mode<=2;mode++) {
          uint64_t stageTicks[5]={0,0,0,0,0};
          uint32_t stageSamples=0, colorPaths[4]={0,0,0,0};
          ppu_taR28StageRead(mode,stageTicks,&stageSamples,colorPaths);
          uint64_t total=0;
          for(unsigned i=0;i<5;i++) total+=stageTicks[i];
          /* Compact sampled cycle attribution. Not equivalent to wall-clock ms. */
          TA_Log("[TA R28 STAGE] end_frame=%lu mode=%lu samples=%lu sample_stride=32 total_ticks=%llu prep=%llu bg1=%llu bg2=%llu sprite=%llu output=%llu color_direct=%lu color_fixed=%lu color_submath=%lu color_generic=%lu\n",
            (unsigned long)count,(unsigned long)mode,(unsigned long)stageSamples,(unsigned long long)total,
            (unsigned long long)stageTicks[0],(unsigned long long)stageTicks[1],
            (unsigned long long)stageTicks[2],(unsigned long long)stageTicks[3],
            (unsigned long long)stageTicks[4],(unsigned long)colorPaths[0],
            (unsigned long)colorPaths[1],(unsigned long)colorPaths[2],
            (unsigned long)colorPaths[3]);
        }
        for(unsigned rank=0;rank<3;rank++) {
          unsigned cfg=0;uint32_t hits=0;
          ppu_taR28ConfigTop(rank,&cfg,&hits);
          if(hits) TA_Log("[TA R28 COLORCFG] end_frame=%lu rank=%lu cfg=%lu lines=%lu clip=%u prevent=%u subscreen=%u half=%u subtract=%u math=%u\n",
            (unsigned long)count,(unsigned long)(rank+1),(unsigned long)cfg,
            (unsigned long)hits,cfg&3u,(cfg>>2)&3u,(cfg>>4)&1u,
            (cfg>>5)&1u,(cfg>>6)&1u,(cfg>>7)&1u);
        }
      }
#endif
#if defined(TA_R28_STAGE_PROFILE) && TA_R28_STAGE_PROFILE
      TA_Log("[TA R28 MASK] end_frame=%lu masked_mode2_lines=%lu\n",(unsigned long)count,(unsigned long)ppu_taR28MaskedLines());
#endif
#if defined(TA_R30_ROW_CACHE_PROFILE) && TA_R30_ROW_CACHE_PROFILE
      for(unsigned bg=0; bg<2; ++bg) {
        uint64_t c[3]={0}; ppu_taR30RowStats(bg,c);
        TA_Log("[TA R30 ROWCACHE] end_frame=%lu layer=%lu hits=%llu epoch_misses=%llu key_conflicts=%llu total=%llu\n",
          (unsigned long)count,(unsigned long)(bg+1),
          (unsigned long long)c[0],(unsigned long long)c[1],
          (unsigned long long)c[2],(unsigned long long)(c[0]+c[1]+c[2]));
      }
#endif
#if defined(TA_R29_BG_TILE_PROFILE) && TA_R29_BG_TILE_PROFILE
      for(unsigned bg=0; bg<2; ++bg) {
        uint64_t ts[3]={0}, counts[4]={0};
        ppu_taR29BGRead(bg,ts,counts);
        TA_Log("[TA R29 BG] end_frame=%lu layer=%lu sample_stride=32 lookup_ticks=%llu row_ticks=%llu compose_ticks=%llu vector_tiles=%llu scalar_tiles=%llu transparent_tiles=%llu culled_tiles=%llu\n",
          (unsigned long)count,(unsigned long)(bg+1),
          (unsigned long long)ts[0],(unsigned long long)ts[1],(unsigned long long)ts[2],
          (unsigned long long)counts[0],(unsigned long long)counts[1],
          (unsigned long long)counts[2],(unsigned long long)counts[3]);
      }
#endif
#if defined(TA_R66_VICTIM_REPORT) && TA_R66_VICTIM_REPORT
      for(unsigned bg=0; bg<2; ++bg) {
        uint64_t v[3]={0};
        ppu_taR66VictimStats(bg,v);
        TA_Log("[TA R66 VICTIM AUDIT] end_frame=%lu layer=%lu probes=%llu hits=%llu fills=%llu interval=60\n",
            (unsigned long)count,(unsigned long)(bg+1),(unsigned long long)v[0],
            (unsigned long long)v[1],(unsigned long long)v[2]);
      }
#endif
#if defined(TA_R64_CACHE_REPORT) && TA_R64_CACHE_REPORT
      /* The pre-existing [TA R30 ROWCACHE] report above supplies exact
         per-interval row hits/misses; this adds cumulative R32 line-cache
         counters. Subtract adjacent reports rather than summing them. */
      {
        uint32_t hits=0,misses=0;
        ppu_taR32CacheStats(&hits,&misses);
        TA_Log("[TA R64 LINECACHE] end_frame=%lu hits=%lu misses=%lu cumulative=1 mode2_only=1\n",
          (unsigned long)count,(unsigned long)hits,(unsigned long)misses);
      }
#endif
#endif /* TA_R32_DIAGNOSTICS || TA_R63_STAGE_REPORT */
#if TA_R32_DIAGNOSTICS
      TA_Log("[TA R14 PROF] end_frame=%lu ppu_ticks=%llu apu_ticks=measured mode1_fast_lines=%llu mode2_fast_lines=%llu mode2_rejected_lines=%llu generic_lines=%llu tile_hits=%llu tile_misses=%llu samples=%lu\n",
        (unsigned long)count,(unsigned long long)pticks,
        (unsigned long long)ppu_xboxPerfFastLines(),
        (unsigned long long)ppu_taMode2FastLines(),
        (unsigned long long)ppu_taMode2RejectedLines(),
        (unsigned long long)ppu_xboxPerfGenericLines(),
        (unsigned long long)ppu_xboxPerfTileCacheHits(),
        (unsigned long long)ppu_xboxPerfTileCacheMisses(),
        (unsigned long)ta_profile_samples());
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
      TA_Log("[TA R21 CHAIN] end_frame=%lu conditional_branch_links=%lu max_instructions=%lu verified_extra_fallthroughs=28 verified_branch_sources=29\n",
        (unsigned long)count, (unsigned long)ta_native_r21_dynamic_links(),
        (unsigned long)ta_native_microblock_max_length());
#endif
#if defined(TA_R24_LOOP_BACKEDGE) && TA_R24_LOOP_BACKEDGE && defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
      TA_Log("[TA R24 LOOPBACK] end_frame=%lu verified_taken_links=%lu source=80:AEB7 target=80:AEAD max_instructions=48\n",
        (unsigned long)count,(unsigned long)ta_native_r24_loop_links());
#endif
      /* R12 PPU counters are per-reporting-interval only. Keep the expensive
         per-pixel reference instrumentation disabled on Release. */
      TA_Log("[TA R14 PPU] end_frame=%lu transparent_rows=%llu sprite_cache_builds=%llu sprite_cached_lines=%llu sprite_mmx_slivers=%llu sprite_scalar_slivers=%llu\n",
        (unsigned long)count,
        (unsigned long long)ppu_xboxPerfTransparentBgRows(),
        (unsigned long long)ppu_xboxPerfSpriteCacheBuilds(),
        (unsigned long long)ppu_xboxPerfSpriteCachedLines(),
        (unsigned long long)ppu_xboxPerfSpriteMmxSlivers(),
        (unsigned long long)ppu_xboxPerfSpriteScalarSlivers());
#if defined(TA_R26_COMPACT_SUBMATH) && TA_R26_COMPACT_SUBMATH
      TA_Log("[TA R26 SUBMATH] end_frame=%lu math_subscreen_lines=%llu lookup_bytes=1024 legacy_bytes=262144\n",
        (unsigned long)count,(unsigned long long)ppu_xboxPerfSubMathLines());
#endif
#if defined(TA_R27_SPRITE_GROUP_SKIP) && TA_R27_SPRITE_GROUP_SKIP
      TA_Log("[TA R27 SPRITE] end_frame=%lu empty_groups=%llu live_groups=%llu total_groups=%llu\n",
        (unsigned long)count,
        (unsigned long long)ppu_taR27EmptySpriteGroups(),
        (unsigned long long)ppu_taR27LiveSpriteGroups(),
        (unsigned long long)(ppu_taR27EmptySpriteGroups()+ppu_taR27LiveSpriteGroups()));
#endif
      TA_Log("[TA R14 OPT] end_frame=%lu cache_hits=%llu cache_misses=%llu reads_avoided=%llu\n",
        (unsigned long)count,
        (unsigned long long)ppu_taMode2OffsetHits(),
        (unsigned long long)ppu_taMode2OffsetMisses(),
        (unsigned long long)(2u * ppu_taMode2OffsetHits()));
#if defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
      TA_Log("[TA R17 WINDOW] end_frame=%lu bitset_scanlines=%lu wide_fallback=original_window_test\n",
        (unsigned long)count, (unsigned long)ppu_taWindowBitsetLines());
#endif
#if defined(TA_R18_WINDOW_CHUNK) && TA_R18_WINDOW_CHUNK && defined(TA_R17_WINDOW_BITSETS) && TA_R17_WINDOW_BITSETS
      uint32_t fastChunks=0,culledChunks=0,mixedChunks=0;
      ppu_taR18WindowStats(&fastChunks,&culledChunks,&mixedChunks);
      TA_Log("[TA R18 WINDOW] end_frame=%lu vector_eligible=%lu fully_culled=%lu mixed_scalar=%lu\n",
        (unsigned long)count,(unsigned long)fastChunks,(unsigned long)culledChunks,(unsigned long)mixedChunks);
#endif
      for(unsigned rank=0;rank<4;rank++){
        uint32_t samples=0,region=ta_profile_top_region(rank,&samples);
        if(region==UINT32_MAX || !samples)break;
        TA_Log("[TA R14 PC] end_frame=%lu rank=%u region=%02X:%04X samples=%lu\n",
          (unsigned long)count,(unsigned)(rank+1),(unsigned)((region>>16)&255u),
          (unsigned)(region&65535u),(unsigned long)samples);
      }
#if defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
      TA_Log("[TA R23 LOOP] end_frame=%lu lda_long_x=%lu adc_dp=%lu sta_dp=%lu extra_sites=3\n",
        (unsigned long)count,
        (unsigned long)ta_native_r23_lda_longx_hits(),
        (unsigned long)ta_native_r23_adc_dp_hits(),
        (unsigned long)ta_native_r23_sta_dp_hits());
#endif
      TA_Log("[TA R22 FLOW] end_frame=%lu jsl=%lu jmp_abs=%lu pea=%lu native_sites=%lu\n",
        (unsigned long)count,
        (unsigned long)ta_native_opcode_hits(28),
        (unsigned long)ta_native_opcode_hits(29),
        (unsigned long)ta_native_opcode_hits(30),
        (unsigned long)ta_native_enabled_sites());
      TA_Log("[TA R20 MEMCPU] end_frame=%lu cmp_abs=%lu lda_long=%lu native_sites=%lu\n",
             (unsigned long)count, (unsigned long)ta_native_opcode_hits(26),
             (unsigned long)ta_native_opcode_hits(27), (unsigned long)ta_native_enabled_sites());
      /* R20: evidence-first diagnostic summary, once per reporting interval.
         Hashes are 32-bit snapshots for change detection, NOT parity proof.
         No per-instruction text output and no ROM bytes dumped. */
      const uint32_t wr=ta_r19_fnv32(s->ram,sizeof(s->ram));
      const uint32_t vr=ta_r19_fnv32(s->ppu->vram,sizeof(s->ppu->vram));
      const uint32_t cg=ta_r19_fnv32(s->ppu->cgram,sizeof(s->ppu->cgram));
      const uint32_t oa=ta_r19_fnv32(s->ppu->oam,sizeof(s->ppu->oam));
      const uint32_t fallback=ta_profile_interpreted_count();
      const uint32_t nativeCount=ta_native_site_hits() - intervalNativeStart;
      const uint32_t countTotal=nativeCount+fallback;
      TA_Log("[TA R20 EVIDENCE] end_frame=%lu wr=%08lX vr=%08lX cg=%08lX oam=%08lX native=%lu interpreted=%lu native_share_x100=%lu dma_pending_calls=%lu dma_skips=%lu exact_sample_collisions=%lu\n",
          (unsigned long)count,(unsigned long)wr,(unsigned long)vr,
          (unsigned long)cg,(unsigned long)oa,(unsigned long)nativeCount,
          (unsigned long)fallback,(unsigned long)(countTotal?((uint64_t)nativeCount*10000u/countTotal):0u),
          (unsigned long)snes_perfDmaCalls(),(unsigned long)snes_perfDmaSkips(),
          (unsigned long)ta_profile_exact_collisions());
      TA_Log("[TA R20 CPUSTATE] end_frame=%lu bank=%02X pc=%04X db=%02X dp=%04X m=%u x=%u e=%u nmi=%u irq=%u int=%u h_irq=%u v_irq=%u in_dma=%u\n",
          (unsigned long)count,(unsigned)s->cpu->k,(unsigned)s->cpu->pc,
          (unsigned)s->cpu->db,(unsigned)s->cpu->dp,(unsigned)s->cpu->mf,
          (unsigned)s->cpu->xf,(unsigned)s->cpu->e,(unsigned)s->cpu->nmiWanted,
          (unsigned)s->cpu->irqWanted,(unsigned)s->cpu->intWanted,
          (unsigned)s->hIrqEnabled,(unsigned)s->vIrqEnabled,(unsigned)s->inDma);
      for(unsigned rank=0; rank<8; ++rank) {
        uint32_t hits=0, addr=0;uint8_t db=0,flags=0;uint16_t dp=0;
        addr=ta_profile_top_exact(rank,&hits,&db,&dp,&flags);
        if(addr==UINT32_MAX || !hits)break;
        TA_Log("[TA R20 SITE] end_frame=%lu rank=%u pc=%02X:%04X samples=%lu m=%u x=%u e=%u db=%02X dp=%04X\n",
            (unsigned long)count,rank+1,(unsigned)(addr>>16),(unsigned)(addr&0xFFFFu),
            (unsigned long)hits,(unsigned)(flags&1u),(unsigned)((flags>>1)&1u),
            (unsigned)((flags>>2)&1u),(unsigned)db,(unsigned)dp);
      }
#endif
#if defined(TA_R71_MODE2_VRAM_STAMP) && TA_R71_MODE2_VRAM_STAMP && defined(TA_R71_STAMP_PROFILE) && TA_R71_STAMP_PROFILE
      if(count%600u==0u){uint64_t bypass=0,checked=0;ppu_taR71StampStats(&bypass,&checked);
        TA_Log("[TA R71 STAMP AUDIT] end_frame=%lu bypass=%llu exact_checks=%llu cumulative=1 mode2_same_key_only=1 fps_not_comparable=1\n",(unsigned long)count,(unsigned long long)bypass,(unsigned long long)checked);}
#endif
#if TA_R72_RECORD_ON && TA_R72_COVERAGE
      if(count%600u==0u){uint32_t attempts,hits;ta_native_r72_counts(&attempts,&hits);
        TA_Log("[TA R72 RECORD COUNTS] end_frame=%lu attempts=%lu hits=%lu cumulative=1 fps_not_comparable=1\n",(unsigned long)count,(unsigned long)attempts,(unsigned long)hits);}
#endif
#if TA_R73_CHAIN_ON && TA_R73_COVERAGE
      if(count%600u==0u){uint32_t stages[8],calls,rejects;uint64_t ops;ta_native_r73_counts(stages,&calls,&rejects,&ops);
        TA_Log("[TA R73 COUNTS] end_frame=%lu calls=%lu rejects=%lu segments=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu ops=%llu cumulative=1 fps_not_comparable=1\n",(unsigned long)count,(unsigned long)calls,(unsigned long)rejects,(unsigned long)stages[0],(unsigned long)stages[1],(unsigned long)stages[2],(unsigned long)stages[3],(unsigned long)stages[4],(unsigned long)stages[5],(unsigned long)stages[6],(unsigned long)stages[7],(unsigned long long)ops);}
#endif
#if TA_R74_FIELDS_ON && TA_R74_COVERAGE
      if(count%600u==0u){uint32_t paths[4],calls;uint64_t ops;ta_native_r74_counts(paths,&calls,&ops);TA_Log("[TA R74 COUNTS] frame=%lu calls=%lu paths=%lu,%lu,%lu,%lu ops=%llu cumulative=1 fps_not_comparable=1\n",(unsigned long)count,(unsigned long)calls,(unsigned long)paths[0],(unsigned long)paths[1],(unsigned long)paths[2],(unsigned long)paths[3],(unsigned long long)ops);}
#endif
#if TA_R82_COST_ON
      if(count%600u==0u)ta_r82_report(ta_r60_emit_cost82,count);
#endif
#if TA_R81_ANIMATION_ON && TA_R81_COVERAGE
      if(count%600u==0u){uint32_t stages[8],calls;uint64_t ops;ta_native_r81_counts(stages,&calls,&ops);TA_Log("[TA R81 COUNTS] frame=%lu calls=%lu ops=%llu cumulative=1 fps_not_comparable=1\n",(unsigned long)count,(unsigned long)calls,(unsigned long long)ops);}
#endif
#if TA_R80_SPRITE_ON && TA_R80_COVERAGE
      if(count%600u==0u){uint32_t stages[8],calls;uint64_t ops;ta_native_r80_counts(stages,&calls,&ops);TA_Log("[TA R80 COUNTS] frame=%lu calls=%lu ops=%llu cumulative=1 fps_not_comparable=1\n",(unsigned long)count,(unsigned long)calls,(unsigned long long)ops);}
#endif
#if TA_R78_QUIET_ON && TA_R78_COVERAGE
      if(count%600u==0u){uint32_t stages[5],calls;uint64_t ops;ta_native_r78_counts(stages,&calls,&ops);TA_Log("[TA R78 COUNTS] frame=%lu calls=%lu stages=%lu,%lu,%lu,%lu,%lu ops=%llu cumulative=1 fps_not_comparable=1\n",(unsigned long)count,(unsigned long)calls,(unsigned long)stages[0],(unsigned long)stages[1],(unsigned long)stages[2],(unsigned long)stages[3],(unsigned long)stages[4],(unsigned long long)ops);}
#endif
#if defined(TA_R103_GUARD_AUDIT) && TA_R103_GUARD_AUDIT
      if(count%600u==0u){
        for(unsigned f=0;f<TA_R103_FAMILY_COUNT;++f){
          uint64_t a[5];ta_native_r103_get(f,a);
          TA_Log("[TA R103 GUARD] frame=%lu family=%s attempts=%llu accepted=%llu rejected=%llu prefilter_skips=%llu guest_ops=%llu cumulative=1 fps_not_comparable=1\n",
            (unsigned long)count,ta_native_r103_name(f),
            (unsigned long long)a[0],(unsigned long long)a[1],
            (unsigned long long)a[2],(unsigned long long)a[3],
            (unsigned long long)a[4]);
        }
        uint64_t rr[4];ta_native_r103_r73_reasons(rr);
        TA_Log("[TA R103 R73 REASONS] frame=%lu cpu_guard=%llu rom_dma_observer=%llu irq_time_fence=%llu no_stage=%llu cumulative=1 fps_not_comparable=1\n",
         (unsigned long)count,(unsigned long long)rr[0],(unsigned long long)rr[1],
         (unsigned long long)rr[2],(unsigned long long)rr[3]);
#if defined(TA_R104_GUARD_DETAIL) && TA_R104_GUARD_DETAIL
        uint64_t rs[14];ta_native_r104_r73_cpu_reasons(rs);
        TA_Log("[TA R104 R73 CPU] frame=%lu identity=%llu db80=%llu db81=%llu db_other_rom=%llu db_nonrom=%llu dp=%llu emulation=%llu m8=%llu x8=%llu decimal=%llu xbounds=%llu ybounds=%llu cpu_pending=%llu other=%llu cumulative=1 fps_not_comparable=1\n",
        (unsigned long)count,(unsigned long long)rs[0],(unsigned long long)rs[1],
        (unsigned long long)rs[2],(unsigned long long)rs[3],(unsigned long long)rs[4],
        (unsigned long long)rs[5],(unsigned long long)rs[6],(unsigned long long)rs[7],
        (unsigned long long)rs[8],(unsigned long long)rs[9],(unsigned long long)rs[10],
        (unsigned long long)rs[11],(unsigned long long)rs[12],(unsigned long long)rs[13]);
#endif
#if defined(TA_R114_R108_DIAG) && TA_R114_R108_DIAG
        uint64_t m2[3];ta_native_r114_r108_counts(m2);
        TA_Log("[TA R114 R108 NATIVE] frame=%lu attempts=%llu accepted=%llu guest_ops=%llu cumulative=1 fps_not_comparable=1\n",
          (unsigned long)count,(unsigned long long)m2[0],(unsigned long long)m2[1],(unsigned long long)m2[2]);
#if defined(TA_R115_R108_REJECT_DETAIL) && TA_R115_R108_REJECT_DETAIL
        uint64_t r115[12];ta_native_r115_r108_rejection_counts(r115);
        TA_Log("[TA R115 R108 REJECT] frame=%lu rom=%llu bank_db=%llu cpu_mode=%llu cpu_pending=%llu observer_dma=%llu hpos=%llu stage=%llu stack=%llu budget=%llu bus_limit=%llu timer=%llu other=%llu cumulative=1 fps_not_comparable=1\n",
          (unsigned long)count,(unsigned long long)r115[0],(unsigned long long)r115[1],
          (unsigned long long)r115[2],(unsigned long long)r115[3],(unsigned long long)r115[4],
          (unsigned long long)r115[5],(unsigned long long)r115[6],(unsigned long long)r115[7],
          (unsigned long long)r115[8],(unsigned long long)r115[9],(unsigned long long)r115[10],
          (unsigned long long)r115[11]);
#endif
#endif
        uint64_t bg[5];ppu_taR103Mode2Counts(bg);
        TA_Log("[TA R103 MODE2 BG] frame=%lu main_only=%llu sub_only=%llu both_candidate=%llu neither=%llu actual_dual_calls=%llu cumulative=1 fps_not_comparable=1\n",
          (unsigned long)count,(unsigned long long)bg[0],(unsigned long long)bg[1],
          (unsigned long long)bg[2],(unsigned long long)bg[3],(unsigned long long)bg[4]);
      }
#endif
      intervalNativeStart=ta_native_site_hits();
      snes_perfReset(); ppu_xboxPerfReset();
      if(ta_profile_interval_reset_due(count))ta_profile_reset();
      elapsedStart=GetTickCount();coreMs=videoMs=audioMs=0;
    }
#endif /* R120 NOLOG: no per-interval telemetry, hashing, formatting or reset work. */
  }
  /* Main loop remains unmodified; power/reset console normally. */
  return 0;
}
