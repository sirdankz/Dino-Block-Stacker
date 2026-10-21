#include "ta_diagnostics.h"
/* R10: Tetris Attack ROM-identity-guarded native 65C816 instruction sites.
   Generated from the exact Tetris Attack USA ROM / host-execution trace.
   Ordinary native sites execute one CPU instruction with the same bus accesses,
   idle cycles and interrupt checkpoints as the SNES reference. R48 additionally
   fuses one exact-ROM game branch only inside a proven observer/event-free span.
   This is incremental native CPU execution, NOT a whole-game native port.
   Never use site addresses on another game or ROM revision. */
#include "ta_native.h"
#if TA_R103_GUARD_AUDIT
/* R103: diagnostic-only dispatch counters: descriptor-routed attempts,
 * handler acceptances, handler returns of zero, prefilter skips, accepted
 * guest-opcode work. For each family, calls+returns0=attempts.
 * Counters do not identify the *reason* for rejection inside a handler. */
static uint64_t s_r103Audit[TA_R103_FAMILY_COUNT][5];
static const char* const s_r103Names[TA_R103_FAMILY_COUNT]={
 "r55_service_chain", "r52_accumulate", "r58_counter", "r58_idle",
 "r48_quiet_service", "r62_sprite", "r73_sprite_chain", "r81_animation",
 "r80_mode1_sprite", "r74_fields", "r78_quiet", "r91_transfer",
 "r58_task_prefix"};
const char* ta_native_r103_name(unsigned f){return f<TA_R103_FAMILY_COUNT?s_r103Names[f]:"unknown";}
void ta_native_r103_get(unsigned f,uint64_t out[5]){
 if(!out)return;
 for(unsigned i=0;i<5;++i)out[i]=f<TA_R103_FAMILY_COUNT?s_r103Audit[f][i]:0;
}
/* Deliberately outside the native handler, to account for early returns. */
static bool ta_r103AuditCall(unsigned f,unsigned ops){
 ++s_r103Audit[f][0];
 if(ops){++s_r103Audit[f][1];s_r103Audit[f][4]+=ops;return true;}
 ++s_r103Audit[f][2];return false;
}
#define TA_R103_ACCEPT(f,expression) ta_r103AuditCall((f),(expression))
#define TA_R103_PREFILTER_SKIP(f) (++s_r103Audit[(f)][3])
#else
#define TA_R103_ACCEPT(f,expression) ((expression)!=0)
#define TA_R103_PREFILTER_SKIP(f) ((void)0)
#endif
#if defined(TA_R114_R108_DIAG) && TA_R114_R108_DIAG
/* R114: diagnostic-only accepted/rejected R108 segment coverage.
 * Never infer elapsed Xbox cycles from these frequency counters. */
static uint64_t s_r114_r108_attempts,s_r114_r108_accepts,s_r114_r108_guest_ops;
void ta_native_r114_r108_counts(uint64_t out[3]){
 if(!out)return;
 out[0]=s_r114_r108_attempts;
 out[1]=s_r114_r108_accepts;
 out[2]=s_r114_r108_guest_ops;
}
#endif
#if defined(TA_R115_R108_REJECT_DETAIL) && TA_R115_R108_REJECT_DETAIL
/* R115 diagnostic-only, one first rejection classification per rejected entry. */
static uint64_t s_r115R108Reject[12];
void ta_native_r115_r108_rejection_counts(uint64_t out[12]) {
 for(unsigned i=0;i<12u;++i)out[i]=s_r115R108Reject[i];
}
#define TA_R115_R108_REJECT(reason) (++s_r115R108Reject[(reason)])
#else
#define TA_R115_R108_REJECT(reason) ((void)0)
#endif
#include "snes.h"
#include "ta_code_fetch.h"
#include "ta_data_bus.h"
#include "ta_dispatch_entry.h"
#include "ta_game_service.h"
#include "ta_game_blocks.h"
#include "ta_accumulate_loop.h"
#include "ta_task_prefix.h"
#include "ta_idle_slots.h"
#include "ta_counter_block.h"
#include "ta_profile.h"
#include "ta_r60_store.h"
#include "ta_r61_workers.h"
#include "ta_r62_sprite.h"
#include "ta_r72_record.h"
#include "ta_r73_chain.h"
#include "ta_r80_sprite.h"
#include "ta_r81_animation.h"
#include "ta_r74_fields.h"
#include "ta_r77_dispatch.h"
#include "ta_r78_quiet.h"
#include "ta_r82_cost.h"
#if TA_R60_STORE_ON || TA_R61_WORKERS_ON || TA_R62_SPRITE_ON
#include "ta_rom_sha256.h"
static bool s_r60ShaVerified;
#if TA_R61_WORKERS_ON && TA_R61_WORKER_COVERAGE
static uint32_t s_r61WorkerHits[49];
#endif
#endif
#if defined(TA_R19_HOST_DEEP) && TA_R19_HOST_DEEP
#include "ta_evidence.h"
#endif
extern uint8_t snes_cpuReadOpcodeFast(void *mem, uint32_t adr);

#define TA_R8_CLASSES 32u
#if TA_R47_REJECT_ON
bool ta_r47IdenticalRom=false;
#define s_identical_rom ta_r47IdenticalRom
#else
static bool s_identical_rom = false;
#endif
static uint32_t s_opcode_hits[TA_R8_CLASSES];
#if defined(TA_R54_XBOX_PROFILE) && TA_R54_XBOX_PROFILE
/* Keep these increments strictly out of the non-profiling release binary. */
static uint32_t s_r54Hot[4];
void ta_native_r54_hot_counts(uint32_t counts[4]) {
  if (!counts) return;
  for (unsigned i=0;i<4u;++i) counts[i]=s_r54Hot[i];
}
#endif
#if defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
/* Three exact-ROM hot-loop sites: counted separately to keep R15's byte-packed
   5-bit opcode-class lookup unchanged. Never activate on a different ROM. */
static uint32_t s_r23HotHits[3];
uint32_t ta_native_r23_lda_longx_hits(void){return s_r23HotHits[0];}
uint32_t ta_native_r23_adc_dp_hits(void){return s_r23HotHits[1];}
uint32_t ta_native_r23_sta_dp_hits(void){return s_r23HotHits[2];}
#endif
static uint64_t fingerprint(const uint8_t *buf, size_t bytes) {
  uint64_t h=UINT64_C(0xcbf29ce484222325);
  for (size_t i=0;i<bytes;i++) h=(h^buf[i])*UINT64_C(0x100000001b3);
  return h;
}

/* R10 guarded native microblock adjacency (verified against exact USA LoROM).
   A match is only a candidate: the CPU's actual next bank:PC must equal the
   expected successor after every complete instruction/bus-timing sequence. */
static uint32_t s_microblock_calls = 0;
static uint32_t s_microblock_instructions = 0;
static uint32_t s_microblock_max_length = 0;
uint32_t ta_native_microblock_calls(void) { return s_microblock_calls; }
uint32_t ta_native_microblock_instructions(void) { return s_microblock_instructions; }
uint32_t ta_native_microblock_max_length(void) { return s_microblock_max_length; }
void ta_native_record_microblock(uint32_t n) {
  if(!TA_R32_DIAGNOSTICS || n<2) return;
  ++s_microblock_calls;
  s_microblock_instructions += n;
  if(n>s_microblock_max_length)s_microblock_max_length=n;
}

/* R15: Exact-ROM native dispatch table, 6 ROM banks x 32 KiB.
   Zero means fallback to the original SNES interpreter. Only installed after
   full-ROM fingerprint verification, so no other ROM can reuse these sites. */
#if defined(TA_R15_NATIVE_LOOKUP) && TA_R15_NATIVE_LOOKUP
typedef struct { uint32_t pc; uint8_t cls_plus_one; } TaLookupEntry;
#if defined(TA_R40_DECODED_DISPATCH) && TA_R40_DECODED_DISPATCH
/* Exact-ROM descriptors: class[5:0], successor delta[8:6], branch[9].
   One lookup replaces repeated site, successor and branch classification. */
typedef uint16_t TaNativeMapWord;
#define TA_MAP_CLASS_MASK 63u
#define TA_MAP_DELTA_SHIFT 6u
#else
typedef uint8_t TaNativeMapWord;
#define TA_MAP_CLASS_MASK 31u
#define TA_MAP_DELTA_SHIFT 5u
#endif
#if defined(TA_R108_MODE2_RECORDS) && TA_R108_MODE2_RECORDS
static TaNativeMapWord s_nativeOpcodeMap[7][0x8000];
#else
static TaNativeMapWord s_nativeOpcodeMap[6][0x8000];
#endif
#if TA_R47_REJECT_ON
const uint16_t* ta_r47NativeBanks[256];
#define s_nativeOpcodeBank ta_r47NativeBanks
#else
static const TaNativeMapWord* s_nativeOpcodeBank[256];
#endif
static const TaLookupEntry s_nativeOpcodeSites[] = {
  {0x808069u,22u},   {0x80806Cu,23u},   {0x80806Fu,16u},   {0x808071u,23u},   {0x808074u,16u},
  {0x808076u,23u},   {0x808079u,16u},   {0x80807Bu,1u},   {0x80807Fu,15u},   {0x808120u,1u},
  {0x808121u,1u},   {0x808122u,15u},   {0x808131u,1u},   {0x808132u,1u},   {0x808133u,15u},
  {0x808E27u,24u},   {0x808E2Du,19u},   {0x808E34u,21u},   {0x808E9Fu,19u},   {0x809086u,17u},
  {0x809087u,10u},   {0x80908Du,11u},   {0x809090u,15u},   {0x809092u,11u},   {0x809095u,15u},
  {0x809097u,18u},   {0x809098u,20u},   {0x8090F3u,17u},   {0x8090FFu,18u},   {0x809101u,20u},
  {0x8091B2u,17u},   {0x8091B3u,24u},   {0x8091B5u,19u},   {0x8091C4u,19u},   {0x8091C5u,18u},
  {0x8091C6u,20u},   {0x8091D7u,26u},   {0x8091F1u,26u},   {0x80920Bu,26u},   {0x809225u,26u},
  {0x80924Eu,26u},   {0x809357u,17u},   {0x809358u,24u},   {0x80935Au,19u},   {0x8093FDu,19u},
  {0x8093FEu,18u},   {0x8093FFu,20u},   {0x80942Eu,15u},   {0x809431u,10u},   {0x809436u,14u},
  {0x809438u,3u},   {0x809439u,15u},   {0x809479u,21u},   {0x80947Cu,15u},   {0x809494u,2u},
  {0x809495u,15u},   {0x8094D8u,15u},   {0x8094DDu,15u},   {0x8094E0u,22u},   {0x8094ECu,16u},
  {0x8094F1u,7u},   {0x8094F3u,2u},   {0x8094F4u,2u},   {0x8094F5u,15u},   {0x8095A4u,25u},
  {0x8095AFu,20u},   {0x8096AAu,20u},   {0x809B1Fu,17u},   {0x809B9Au,21u},   {0x809BA0u,21u},
  {0x809BA6u,21u},   {0x809BACu,21u},   {0x809BB2u,21u},   {0x809BB8u,21u},   {0x809BBEu,21u},
  {0x809BC4u,21u},   {0x809BCAu,21u},   {0x809BD0u,21u},   {0x809BD6u,21u},   {0x809BDCu,18u},
  {0x809BDDu,20u},   {0x809BDEu,17u},   {0x809C02u,18u},   {0x809C03u,20u},   {0x809C04u,17u},
  {0x809C07u,21u},   {0x809C0Au,22u},   {0x809C10u,21u},   {0x809C40u,21u},   {0x809C50u,21u},
  {0x809C80u,18u},   {0x809C81u,20u},   {0x809CD3u,15u},   {0x809D94u,21u},   {0x809DADu,16u},
  {0x809DB6u,20u},   {0x809F5Eu,1u},   {0x809F5Fu,1u},   {0x809F60u,5u},   {0x809F61u,5u},
  {0x809F62u,15u},   {0x80A07Fu,17u},   {0x80A0C8u,18u},   {0x80A0C9u,20u},   {0x80A0CAu,17u},
  {0x80A11Au,18u},   {0x80A11Bu,20u},   {0x80A138u,21u},   {0x80A144u,20u},   {0x80A595u,2u},
  {0x80A596u,15u},   {0x80A59Eu,16u},   {0x80A5A7u,1u},   {0x80A5AEu,2u},   {0x80A5AFu,15u},
  {0x80A5B7u,16u},   {0x80A5BDu,25u},   {0x80A5CAu,22u},   {0x80A5CFu,2u},   {0x80A5D0u,15u},
  {0x80A5D6u,10u},   {0x80A5DAu,12u},   {0x80A5DBu,6u},   {0x80A5DEu,8u},   {0x80A5DFu,14u},
  {0x80A5E7u,2u},   {0x80A5E8u,1u},   {0x80A5F0u,16u},   {0x80A5F8u,23u},   {0x80A5FCu,22u},
  {0x80A612u,25u},   {0x80A61Au,26u},   {0x80A6FBu,17u},   {0x80A6FDu,19u},   {0x80A700u,17u},
  {0x80A706u,25u},   {0x80A724u,18u},   {0x80A725u,18u},   {0x80A726u,20u},   {0x80A79Du,22u},
  {0x80A7B1u,23u},   {0x80A7C2u,20u},   {0x80A7EAu,23u},   {0x80A814u,21u},   {0x80A8FDu,24u},
  {0x80A8FFu,19u},   {0x80A976u,19u},   {0x80AE15u,17u},   {0x80AE25u,18u},   {0x80AE92u,17u},
  {0x80AE93u,24u},   {0x80AE95u,19u},   {0x80AEB1u,4u},   {0x80AEB6u,3u},   {0x80AEB7u,15u},
  {0x80AEC8u,19u},   {0x80AEC9u,18u},   {0x80B636u,17u},   {0x80BBAFu,17u},   {0x80BBB0u,10u},
  {0x80BBB2u,24u},   {0x80BBB5u,25u},   {0x80BBB6u,4u},   {0x80BBB9u,8u},   {0x80BBBAu,4u},
  {0x80BBBFu,7u},   {0x80BBC0u,2u},   {0x80BBC1u,2u},   {0x80BBC2u,14u},   {0x80BBC9u,19u},
  {0x80BBCAu,10u},   {0x80BBCFu,22u},   {0x80BBD4u,25u},   {0x80BBD5u,25u},   {0x80BBD6u,4u},
  {0x80BBDAu,23u},   {0x80BBDFu,9u},   {0x80BBE0u,2u},   {0x80BBE1u,2u},   {0x80BBE5u,26u},
  {0x80BBEFu,4u},   {0x80BBF5u,11u},   {0x80BBF8u,16u},   {0x80BC0Au,4u},   {0x80BC10u,11u},
  {0x80BC13u,16u},   {0x80BC36u,4u},   {0x80BC42u,2u},   {0x80BC43u,2u},   {0x80BC44u,2u},
  {0x80BC45u,2u},   {0x80BC46u,2u},   {0x80BC47u,1u},   {0x80BC48u,1u},   {0x80BC49u,1u},
  {0x80BC4Au,1u},   {0x80BC4Du,15u},   {0x80BC52u,19u},   {0x80BC53u,18u},   {0x80BC54u,20u},
  {0x80C08Du,17u},   {0x80C08Eu,10u},   {0x80C090u,24u},   {0x80C093u,25u},   {0x80C094u,4u},
  {0x80C097u,8u},   {0x80C098u,4u},   {0x80C09Du,7u},   {0x80C09Eu,2u},   {0x80C09Fu,2u},
  {0x80C0A0u,14u},   {0x80C0A7u,19u},   {0x80C0A8u,10u},   {0x80C0ADu,22u},   {0x80C0B2u,25u},
  {0x80C0B3u,25u},   {0x80C0B4u,4u},   {0x80C0B8u,23u},   {0x80C0BDu,9u},   {0x80C0BEu,2u},
  {0x80C0BFu,2u},   {0x80C0C3u,26u},   {0x80C0CFu,11u},   {0x80C0D2u,16u},   {0x80C0D7u,6u},
  {0x80C0DBu,26u},   {0x80C0E0u,4u},   {0x80C0E6u,11u},   {0x80C0E9u,16u},   {0x80C0FDu,11u},
  {0x80C100u,16u},   {0x80C105u,6u},   {0x80C109u,26u},   {0x80C10Eu,4u},   {0x80C114u,11u},
  {0x80C117u,16u},   {0x80C137u,26u},   {0x80C13Bu,11u},   {0x80C13Eu,16u},   {0x80C15Du,11u},
  {0x80C160u,15u},   {0x80C162u,22u},   {0x80C165u,26u},   {0x80C16Au,4u},   {0x80C170u,4u},
  {0x80C174u,23u},   {0x80C17Fu,26u},   {0x80C183u,11u},   {0x80C186u,16u},   {0x80C1A5u,11u},
  {0x80C1A8u,15u},   {0x80C1AAu,22u},   {0x80C1ADu,26u},   {0x80C1B2u,4u},   {0x80C1B8u,4u},
  {0x80C1BCu,23u},   {0x80C1C7u,26u},   {0x80C1CCu,22u},   {0x80C1CFu,4u},   {0x80C1D2u,22u},
  {0x80C1DAu,22u},   {0x80C1E8u,2u},   {0x80C1E9u,2u},   {0x80C1EAu,2u},   {0x80C1EBu,2u},
  {0x80C1ECu,2u},   {0x80C1EDu,1u},   {0x80C1EEu,1u},   {0x80C1EFu,1u},   {0x80C1F0u,1u},
  {0x80C1F3u,16u},   {0x80C1FBu,19u},   {0x80C1FCu,18u},   {0x80C1FDu,20u},   {0x80C6D6u,17u},
  {0x80C6D7u,24u},   {0x80C6D9u,19u},   {0x80CA2Fu,19u},   {0x80CA30u,18u},   {0x80CA31u,20u},
  {0x82A33Du,23u},   {0x82A353u,20u},   {0x8480DCu,17u},   {0x8480E5u,22u},   {0x8480F2u,22u},
  {0x8480F5u,26u},   {0x8480FBu,18u},   {0x8480FCu,20u},   {0x848162u,17u},   {0x848163u,10u},
  {0x848165u,21u},   {0x848168u,16u},   {0x84816Au,23u},   {0x84816Du,16u},   {0x848176u,22u},
  {0x848179u,16u},   {0x8481A6u,15u},   {0x8481BAu,15u},   {0x8481CBu,18u},   {0x8481CCu,20u},
  {0x8481D6u,17u},   {0x8481DAu,24u},   {0x8481DCu,19u},   {0x8481DDu,21u},   {0x8481E1u,22u},
  {0x8481F4u,22u},   {0x8481F7u,19u},   {0x8481F9u,18u},   {0x8481FAu,20u},   {0x848C5Fu,17u},
  {0x848C62u,24u},   {0x848C98u,25u},   {0x848CA8u,18u},   {0x848CF9u,25u},   {0x848D3Fu,25u},
  {0x848E05u,23u},   {0x848F1Eu,17u},   {0x848F21u,24u},   {0x848F65u,18u},   {0x848F90u,26u},
  {0x848FC7u,17u},   {0x848FCAu,24u},   {0x84903Bu,3u},   {0x84903Cu,3u},   {0x849041u,18u},
  {0x84908Bu,17u},   {0x84908Eu,24u},   {0x84909Fu,18u},   {0x849175u,22u},   {0x849178u,25u},
  {0x84918Eu,13u},   {0x84918Fu,6u},   {0x849193u,8u},   {0x84C12Du,17u},   {0x84C12Eu,24u},
  {0x84C132u,19u},   {0x84C133u,19u},   {0x84C134u,10u},   {0x84C13Au,21u},   {0x84C13Du,11u},
  {0x84C140u,16u},   {0x84C166u,21u},   {0x84C169u,11u},   {0x84C16Cu,16u},   {0x84C192u,19u},
  {0x84C193u,18u},   {0x84C194u,20u},   {0x86D972u,20u},   {0x86D9CDu,20u},   {0x86D9F0u,20u},
  {0x899E69u,17u},   {0x899E6Au,24u},   {0x899EF0u,26u},   {0x89A1B6u,18u},   {0x89A1C1u,23u},
  {0x89A1C9u,25u},   {0x89A325u,17u},   {0x89A364u,18u},   {0x89A366u,17u},   {0x89A367u,10u},
  {0x89A369u,24u},   {0x89A36Du,19u},   {0x89A373u,11u},   {0x89A376u,16u},   {0x89A378u,14u},
  {0x89A37Eu,16u},   {0x89A380u,10u},   {0x89A382u,24u},   {0x89A389u,14u},   {0x89A392u,19u},
  {0x89A393u,10u},   {0x89A395u,17u},   {0x89A39Au,18u},   {0x89A39Bu,19u},   {0x89A39Cu,14u},
  {0x89A3A2u,16u},   {0x89A3A7u,10u},   {0x89A3ABu,19u},   {0x89A3ACu,18u},   {0x89A3ADu,20u},
  {0x89A3C2u,23u},   {0x89A3EAu,24u},   {0x89A3F5u,17u},   {0x89A3FAu,18u},   {0x89A40Eu,18u},
  {0x89A431u,17u},   {0x89A436u,18u},   {0x89BEC1u,26u},   {0x89BECEu,26u},   {0x8A8891u,3u},
  {0x8A88E9u,3u},   {0x8A88EAu,15u},   {0x8A8AC3u,3u},   {0x8A8AC4u,15u},   {0x8ABD79u,25u},
  {0x8ABD7Au,25u},   {0x8ABD7Bu,25u},   {0x8ABD9Eu,22u},   {0x8ABDA5u,24u},   {0x8ABDB0u,26u},
  {0x8ABE03u,17u},   {0x8ABE05u,19u},   {0x8ABE0Bu,25u},   {0x8ABE13u,23u},   {0x8ABE65u,24u},
  {0x8ABE70u,26u},   {0x8ABE7Au,24u},   {0x8ABE85u,26u},   {0x8ABE8Fu,18u},   {0x8ABE90u,20u},
  {0x8AC087u,24u},   {0x8AC0AFu,7u},   {0x8AC0B4u,22u},   {0x8AC0B7u,16u},   {0x8AC0C4u,7u},
  {0x8AC0C9u,22u},   {0x8AC0CCu,16u},   {0x8AC0D6u,4u},   {0x8AC0E0u,4u},   {0x8AC0E7u,3u},
  {0x8AC0E8u,15u},   {0x8AC0EBu,20u},   {0x8AC0F0u,16u},   {0x8AC0F7u,25u},   {0x8AC101u,25u},
  {0x8AC14Du,23u},   {0x8AC16Au,26u},   {0x8AC1A9u,23u},   {0x8AC1D5u,24u},   {0x8AC1DDu,19u},
  {0x8AC21Cu,19u},   {0x8AC21Du,20u},   {0x8AC93Du,7u},   {0x8AC954u,22u},   {0x8AC95Du,22u},
  {0x8AC96Du,20u},   {0x8AD305u,24u},   {0x8AD309u,19u},   {0x8AD30Au,19u},   {0x8AD314u,19u},
  {0x8AD315u,20u},   {0x8AD31Du,25u},   {0x8AD31Eu,7u},   {0x8AD322u,15u},   {0x8AD344u,15u},
  {0x8AD37Au,23u},   {0x8AD37Du,16u},   {0x8AD382u,23u},   {0x8AD385u,15u},   {0x8AD3C6u,2u},
  {0x8AD3C7u,2u},   {0x8AD3CDu,16u},   {0x8AD3EAu,25u},   {0x8AD3EBu,7u},   {0x8AD3EFu,15u},
  {0x8AD411u,15u},   {0x8AD447u,23u},   {0x8AD44Au,16u},   {0x8AD44Fu,23u},   {0x8AD452u,15u},
  {0x8AD493u,2u},   {0x8AD494u,2u},   {0x8AD49Au,16u},   {0x8AD4B7u,25u},   {0x8AD514u,23u},
  {0x8AD51Cu,23u}, 
};
/* R20: observed exact-USA-ROM memory read / compare sites. This separate
   manifest is guarded both by the full ROM fingerprint and an optional build
   flag. No native site is enabled for any other ROM. */
#if defined(TA_R20_NATIVE_MEMORY_OPS) && TA_R20_NATIVE_MEMORY_OPS
static const TaLookupEntry s_r20MemorySites[] = {
  {0x80805Eu,27u}, /* observed=1 */
  {0x809089u,28u}, /* observed=224957 */
  {0x8090F6u,28u}, /* observed=1736 */
  {0x809368u,27u}, /* observed=1611 */
  {0x80936Du,27u}, /* observed=1611 */
  {0x80938Eu,27u}, /* observed=1611 */
  {0x809393u,27u}, /* observed=1611 */
  {0x8093B4u,27u}, /* observed=1611 */
  {0x8093B9u,27u}, /* observed=1611 */
  {0x8093DAu,27u}, /* observed=1611 */
  {0x8093DFu,27u}, /* observed=1611 */
  {0x80942Bu,27u}, /* observed=137892 */
  {0x80943Bu,27u}, /* observed=66 */
  {0x809470u,27u}, /* observed=129 */
  {0x8094C4u,27u}, /* observed=4334 */
  {0x8094C9u,27u}, /* observed=2 */
  {0x8094D5u,27u}, /* observed=131817 */
  {0x8094DAu,27u}, /* observed=11392 */
  {0x80A082u,28u}, /* observed=2275 */
  {0x80A0BAu,28u}, /* observed=2275 */
  {0x80A0CDu,28u}, /* observed=2714 */
  {0x80A10Cu,28u}, /* observed=2714 */
  {0x80AC3Au,28u}, /* observed=1607 */
  {0x80AEBBu,28u}, /* observed=1607 */
  {0x82A32Eu,28u}, /* observed=1607 */
  {0x82A334u,28u}, /* observed=1607 */
  {0x8480E0u,28u}, /* observed=2436 */
  {0x86D8FAu,28u}, /* observed=1607 */
  {0x86D93Au,28u}, /* observed=1608 */
  {0x86D9D8u,28u}, /* observed=1608 */
  {0x8AC0ABu,28u}, /* observed=26980 */
  {0x8AC0C0u,28u}, /* observed=26980 */
  {0x8AC0D2u,28u}, /* observed=26980 */
  {0x8AC939u,28u}, /* observed=7207 */
};
#endif

/* R22: exact-USA-ROM 65C816 control-flow and stack sites.  Loaded only
   after whole-ROM fingerprint validation and behind TA_R22_NATIVE_FLOW_OPS.
   The entries are observed opcode PCs, NOT inferred function boundaries. */
#if defined(TA_R22_NATIVE_FLOW_OPS) && TA_R22_NATIVE_FLOW_OPS
static const TaLookupEntry s_r22FlowSites[] = {
  /* opcode 22: 40 sampled sites */
  {0x80A6F0u,29u}, /* host_hits_600=102981 */
  {0x80A6F4u,29u}, /* host_hits_600=102977 */
  {0x84C136u,29u}, /* host_hits_600=102977 */
  {0x8AC0B9u,29u}, /* host_hits_600=2366 */
  {0x8AD36Bu,29u}, /* host_hits_600=1183 */
  {0x8481CDu,29u}, /* host_hits_600=1135 */
  {0x8AD438u,29u}, /* host_hits_600=845 */
  {0x89A396u,29u}, /* host_hits_600=810 */
  {0x8AD231u,29u}, /* host_hits_600=556 */
  {0x8AC0CEu,29u}, /* host_hits_600=507 */
  {0x8AC969u,29u}, /* host_hits_600=507 */
  {0x8AD505u,29u}, /* host_hits_600=507 */
  {0x808E6Fu,29u}, /* host_hits_600=427 */
  {0x808E73u,29u}, /* host_hits_600=427 */
  {0x808E77u,29u}, /* host_hits_600=427 */
  {0x808E7Bu,29u}, /* host_hits_600=427 */
  {0x808E7Fu,29u}, /* host_hits_600=427 */
  {0x808E83u,29u}, /* host_hits_600=427 */
  {0x808E87u,29u}, /* host_hits_600=427 */
  {0x808E8Bu,29u}, /* host_hits_600=427 */
  {0x80A778u,29u}, /* host_hits_600=427 */
  {0x80A77Fu,29u}, /* host_hits_600=427 */
  {0x80A786u,29u}, /* host_hits_600=427 */
  {0x80A78Au,29u}, /* host_hits_600=427 */
  {0x80A794u,29u}, /* host_hits_600=427 */
  {0x80A7B9u,29u}, /* host_hits_600=427 */
  {0x80AE96u,29u}, /* host_hits_600=427 */
  {0x80AE9Au,29u}, /* host_hits_600=427 */
  {0x80AE9Eu,29u}, /* host_hits_600=427 */
  {0x8AD287u,29u}, /* host_hits_600=304 */
  {0x80B594u,29u}, /* host_hits_600=264 */
  {0x80B5B9u,29u}, /* host_hits_600=264 */
  {0x80B604u,29u}, /* host_hits_600=257 */
  {0x80B649u,29u}, /* host_hits_600=257 */
  {0x80B653u,29u}, /* host_hits_600=257 */
  {0x8AD2DDu,29u}, /* host_hits_600=215 */
  {0x80B5DFu,29u}, /* host_hits_600=191 */
  {0x8ABDABu,29u}, /* host_hits_600=169 */
  {0x8ABE18u,29u}, /* host_hits_600=169 */
  {0x8ABE1Cu,29u}, /* host_hits_600=169 */
  /* opcode 4C: 16 sampled sites */
  {0x80A6F8u,30u}, /* host_hits_600=102989 */
  {0x84816Fu,30u}, /* host_hits_600=78418 */
  {0x8AD3CFu,30u}, /* host_hits_600=1014 */
  {0x8AD49Cu,30u}, /* host_hits_600=676 */
  {0x80A905u,30u}, /* host_hits_600=427 */
  {0x80AC40u,30u}, /* host_hits_600=427 */
  {0x80C6E1u,30u}, /* host_hits_600=427 */
  {0x809DEDu,30u}, /* host_hits_600=426 */
  {0x809E34u,30u}, /* host_hits_600=352 */
  {0x8AD569u,30u}, /* host_hits_600=338 */
  {0x80AE40u,30u}, /* host_hits_600=257 */
  {0x80A6C7u,30u}, /* host_hits_600=237 */
  {0x80A67Au,30u}, /* host_hits_600=192 */
  {0x80A4FEu,30u}, /* host_hits_600=173 */
  {0x8490AEu,30u}, /* host_hits_600=169 */
  {0x80A526u,30u}, /* host_hits_600=166 */
  /* opcode F4: 7 sampled sites */
  {0x84C12Fu,31u}, /* host_hits_600=102976 */
  {0x848C65u,31u}, /* host_hits_600=169 */
  {0x848F22u,31u}, /* host_hits_600=169 */
  {0x848FCCu,31u}, /* host_hits_600=169 */
  {0x849091u,31u}, /* host_hits_600=169 */
  {0x8AC1D9u,31u}, /* host_hits_600=169 */
  {0x8AD306u,31u}, /* host_hits_600=169 */
};
#endif

/* R21: verified LoROM static successors for the exact USA ROM only.
   Never infer an edge from a PC that was not validated against the ROM.
   Fallthrough entries are loaded into the R15 map; branches use a sparse
   sorted table because conditional destinations are not one fixed delta. */
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
static const struct {uint32_t pc;uint8_t step;} s_r21FallEdges[] = {
  {0x809087u,2u}, /* cls=9, ROM-confirmed */
  {0x8091B2u,1u}, /* cls=16, ROM-confirmed */
  {0x809357u,1u}, /* cls=16, ROM-confirmed */
  {0x809479u,3u}, /* cls=20, ROM-confirmed */
  {0x809C07u,3u}, /* cls=20, ROM-confirmed */
  {0x80AE92u,1u}, /* cls=16, ROM-confirmed */
  {0x80BBB0u,2u}, /* cls=9, ROM-confirmed */
  {0x80BBB5u,1u}, /* cls=24, ROM-confirmed */
  {0x80BBD4u,1u}, /* cls=24, ROM-confirmed */
  {0x80BBD5u,1u}, /* cls=24, ROM-confirmed */
  {0x80C08Eu,2u}, /* cls=9, ROM-confirmed */
  {0x80C093u,1u}, /* cls=24, ROM-confirmed */
  {0x80C0B2u,1u}, /* cls=24, ROM-confirmed */
  {0x80C0B3u,1u}, /* cls=24, ROM-confirmed */
  {0x80C6D6u,1u}, /* cls=16, ROM-confirmed */
  {0x848163u,2u}, /* cls=9, ROM-confirmed */
  {0x848165u,3u}, /* cls=20, ROM-confirmed */
  {0x8481DCu,1u}, /* cls=18, ROM-confirmed */
  {0x84C12Du,1u}, /* cls=16, ROM-confirmed */
  {0x84C13Au,3u}, /* cls=20, ROM-confirmed */
  {0x84C166u,3u}, /* cls=20, ROM-confirmed */
  {0x899E69u,1u}, /* cls=16, ROM-confirmed */
  {0x89A367u,2u}, /* cls=9, ROM-confirmed */
  {0x89A380u,2u}, /* cls=9, ROM-confirmed */
  {0x8ABD79u,1u}, /* cls=24, ROM-confirmed */
  {0x8ABD7Au,1u}, /* cls=24, ROM-confirmed */
  {0x8AD31Du,1u}, /* cls=24, ROM-confirmed */
  {0x8AD3EAu,1u}, /* cls=24, ROM-confirmed */
};
typedef struct {uint32_t pc,fall,taken;} TaR21BranchEdge;
static const TaR21BranchEdge s_r21BranchEdges[] = {
  {0x80806Fu,0x808071u,0x000000u}, /* cls=15, known branch destination(s) */
  {0x808074u,0x808076u,0x000000u}, /* cls=15, known branch destination(s) */
  {0x808079u,0x80807Bu,0x000000u}, /* cls=15, known branch destination(s) */
  {0x809090u,0x809092u,0x000000u}, /* cls=14, known branch destination(s) */
  {0x809095u,0x809097u,0x000000u}, /* cls=14, known branch destination(s) */
  {0x80942Eu,0x000000u,0x80942Bu}, /* cls=14, known branch destination(s) */
  {0x809439u,0x80943Bu,0x000000u}, /* cls=14, known branch destination(s) */
  {0x80947Cu,0x000000u,0x809479u}, /* cls=14, known branch destination(s) */
  {0x8094D8u,0x8094DAu,0x8094D5u}, /* cls=14, known branch destination(s) */
  {0x8094DDu,0x000000u,0x8094D5u}, /* cls=14, known branch destination(s) */
  {0x8094F5u,0x000000u,0x8094D5u}, /* cls=14, known branch destination(s) */
  {0x809DADu,0x000000u,0x809DB6u}, /* cls=15, known branch destination(s) */
  {0x80C0DBu,0x000000u,0x80C0E0u}, /* cls=25, known branch destination(s) */
  {0x80C109u,0x000000u,0x80C10Eu}, /* cls=25, known branch destination(s) */
  {0x80C160u,0x80C162u,0x000000u}, /* cls=14, known branch destination(s) */
  {0x80C165u,0x000000u,0x80C16Au}, /* cls=25, known branch destination(s) */
  {0x80C1A8u,0x80C1AAu,0x000000u}, /* cls=14, known branch destination(s) */
  {0x80C1ADu,0x000000u,0x80C1B2u}, /* cls=25, known branch destination(s) */
  {0x848168u,0x84816Au,0x000000u}, /* cls=15, known branch destination(s) */
  {0x8481BAu,0x000000u,0x8481CBu}, /* cls=14, known branch destination(s) */
  {0x84C140u,0x000000u,0x84C166u}, /* cls=15, known branch destination(s) */
  {0x84C16Cu,0x000000u,0x84C192u}, /* cls=15, known branch destination(s) */
  {0x89A376u,0x89A378u,0x89A3A7u}, /* cls=15, known branch destination(s) */
  {0x89A37Eu,0x89A380u,0x000000u}, /* cls=15, known branch destination(s) */
  {0x8ABE70u,0x000000u,0x8ABE7Au}, /* cls=25, known branch destination(s) */
  {0x8ABE85u,0x000000u,0x8ABE8Fu}, /* cls=25, known branch destination(s) */
  {0x8AC0CCu,0x000000u,0x8AC0D2u}, /* cls=15, known branch destination(s) */
  {0x8AD385u,0x000000u,0x8AD3C6u}, /* cls=14, known branch destination(s) */
  {0x8AD452u,0x000000u,0x8AD493u}, /* cls=14, known branch destination(s) */
};
static uint32_t s_r21DynamicLinks;
#if defined(TA_R24_LOOP_BACKEDGE) && TA_R24_LOOP_BACKEDGE && defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
static uint32_t s_r24LoopBackLinks;
void ta_native_r24_record_loop_link(void){TA_DIAG(++s_r24LoopBackLinks);}
uint32_t ta_native_r24_loop_links(void){return s_r24LoopBackLinks;}
#endif
/* Collision-free (for the 29 verified PCs) 64-slot multiplicative hash.
   Initialized only after the exact-ROM fingerprint passes. */
static uint8_t s_r21BranchIndex[64];
#define TA_R21_BRANCH_HASH(pc) ((uint32_t)(((uint32_t)(pc) * UINT32_C(0x9f440af9)) >> 26))
uint32_t ta_native_r21_dynamic_links(void){return s_r21DynamicLinks;}
void ta_native_r21_record_dynamic_link(void){TA_DIAG(++s_r21DynamicLinks);}
/* Lookup is performed only at branch endpoints, not for every opcode. */
static const TaR21BranchEdge *ta_r21BranchFind(uint32_t pc) {
  const uint8_t slot=s_r21BranchIndex[TA_R21_BRANCH_HASH(pc)];
  if(!slot)return NULL;
  const TaR21BranchEdge *e=&s_r21BranchEdges[(unsigned)slot-1u];
  return e->pc==pc ? e : NULL;
}
bool ta_native_r21_has_branch_next(uint32_t pc) {
#if defined(TA_R32_NATIVE_DELAY_LOOP) && TA_R32_NATIVE_DELAY_LOOP
  if(s_identical_rom && pc==0x83BF57u)return true;
#endif
  #if defined(TA_R24_LOOP_BACKEDGE) && TA_R24_LOOP_BACKEDGE && defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
  /* Exact-ROM D0 F4 at $80:AEB7: taken BNE goes back to $80:AEAD.
     A non-taken branch falls through to interpreted REP at $80:AEB9. */
  if(s_identical_rom && pc==0x80AEB7u)return true;
#endif
  return s_identical_rom && ta_r21BranchFind(pc)!=NULL;
}
bool ta_native_r21_allow_branch_next(uint32_t from,uint32_t to) {
  if(!s_identical_rom)return false;
#if defined(TA_R32_NATIVE_DELAY_LOOP) && TA_R32_NATIVE_DELAY_LOOP
  if(from==0x83BF57u)return to==0x83BF52u || to==0x83BF59u;
#endif
#if defined(TA_R24_LOOP_BACKEDGE) && TA_R24_LOOP_BACKEDGE && defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
  /* Checked only at native branch endpoints; the special R23 site is not
     stored in the older 5-bit opcode-class table. */
  if(from==0x80AEB7u)return to==0x80AEADu;
#endif
  const TaR21BranchEdge *e=ta_r21BranchFind(from);
  if(!e || (to!=e->fall && to!=e->taken) || !to)return false;
#if defined(TA_R15_NATIVE_LOOKUP) && TA_R15_NATIVE_LOOKUP
  const TaNativeMapWord *row=s_nativeOpcodeBank[(uint8_t)(to>>16)];
  return row && (to&0x8000u) && (row[to&0x7fffu]&TA_MAP_CLASS_MASK);
#else
  return true; /* ta_native_try_execute checks target site, then falls back */
#endif
}
#endif

/* The two high bits store verified successor deltas (+1/+2), with zero
   representing no safe continuation. The lower five bits store native
   instruction class+1. Keep both guarded by the exact ROM fingerprint. */
static const struct {uint32_t pc;uint8_t delta;} s_nativeSuccessors[] = {
  {0x808120u,1u},   {0x808121u,1u},   {0x808131u,1u},   {0x808132u,1u},   {0x809086u,1u},
  {0x809097u,1u},   {0x8091C4u,1u},   {0x8091C5u,1u},   {0x8093FDu,1u},   {0x8093FEu,1u},
  {0x809436u,2u},   {0x809438u,1u},   {0x809494u,1u},   {0x8094F3u,1u},   {0x8094F4u,1u},
  {0x809BDCu,1u},   {0x809C02u,1u},   {0x809C80u,1u},   {0x809F5Eu,1u},   {0x809F5Fu,1u},
  {0x809F60u,1u},   {0x809F61u,1u},   {0x80A0C8u,1u},   {0x80A11Au,1u},   {0x80A595u,1u},
  {0x80A5AEu,1u},   {0x80A5CFu,1u},   {0x80A5DAu,1u},   {0x80A5DEu,1u},   {0x80A5E7u,1u},
  {0x80A724u,1u},   {0x80A725u,1u},   {0x80AEB6u,1u},   {0x80AEC8u,1u},   {0x80BBAFu,1u},
  {0x80BBB9u,1u},   {0x80BBBFu,1u},   {0x80BBC0u,1u},   {0x80BBC1u,1u},   {0x80BBC9u,1u},
  {0x80BBDFu,1u},   {0x80BBE0u,1u},   {0x80BC42u,1u},   {0x80BC43u,1u},   {0x80BC44u,1u},
  {0x80BC45u,1u},   {0x80BC46u,1u},   {0x80BC47u,1u},   {0x80BC48u,1u},   {0x80BC49u,1u},
  {0x80BC52u,1u},   {0x80BC53u,1u},   {0x80C08Du,1u},   {0x80C097u,1u},   {0x80C09Du,1u},
  {0x80C09Eu,1u},   {0x80C09Fu,1u},   {0x80C0A7u,1u},   {0x80C0BDu,1u},   {0x80C0BEu,1u},
  {0x80C1E8u,1u},   {0x80C1E9u,1u},   {0x80C1EAu,1u},   {0x80C1EBu,1u},   {0x80C1ECu,1u},
  {0x80C1EDu,1u},   {0x80C1EEu,1u},   {0x80C1EFu,1u},   {0x80C1FBu,1u},   {0x80C1FCu,1u},
  {0x80CA2Fu,1u},   {0x80CA30u,1u},   {0x8480FBu,1u},   {0x848162u,1u},   {0x8481CBu,1u},
  {0x8481F9u,1u},   {0x84903Bu,1u},   {0x84918Eu,1u},   {0x84C132u,1u},   {0x84C133u,1u},
  {0x84C192u,1u},   {0x84C193u,1u},   {0x89A366u,1u},   {0x89A392u,1u},   {0x89A393u,2u},
  {0x89A39Au,1u},   {0x89A39Bu,1u},   {0x89A3ABu,1u},   {0x89A3ACu,1u},   {0x8A88E9u,1u},
  {0x8A8AC3u,1u},   {0x8ABE8Fu,1u},   {0x8AC0E7u,1u},   {0x8AC21Cu,1u},   {0x8AD309u,1u},
  {0x8AD314u,1u},   {0x8AD3C6u,1u},   {0x8AD493u,1u}, 
};
static void ta_r15InitNativeMap(void) {
  /* Static BSS tables begin zeroed. Entries and bank pointers are immutable
     after installation; repeated exact-ROM initialization is idempotent. */
  s_nativeOpcodeBank[0x80]=s_nativeOpcodeMap[0];
  s_nativeOpcodeBank[0x82]=s_nativeOpcodeMap[1];
  s_nativeOpcodeBank[0x84]=s_nativeOpcodeMap[2];
  s_nativeOpcodeBank[0x86]=s_nativeOpcodeMap[3];
  s_nativeOpcodeBank[0x89]=s_nativeOpcodeMap[4];
  s_nativeOpcodeBank[0x8A]=s_nativeOpcodeMap[5];
#if defined(TA_R108_MODE2_RECORDS) && TA_R108_MODE2_RECORDS
  s_nativeOpcodeBank[0x88]=s_nativeOpcodeMap[6];
  static const uint16_t r108entries[]={0x02ed,0x02ee,0x02f2,0x02f3,0x02f7,0x02fa,0x0300,0x0303,0x0307,0x0308,0x030c,0x030f,0x0315,0x0319,0x031a,0x031d,0x0321,0x0323,0x0324,0x0327,0x0329,0x032a,0x032b,0x032f,0x033b,0x0355};
  for(unsigned i=0;i<sizeof r108entries/sizeof r108entries[0];++i)s_nativeOpcodeMap[6][r108entries[i]]|=32768u;
#endif
  for (unsigned i=0;i<sizeof(s_nativeOpcodeSites)/sizeof(s_nativeOpcodeSites[0]);++i) {
    const TaLookupEntry *e=&s_nativeOpcodeSites[i];
    TaNativeMapWord* row=(TaNativeMapWord*)s_nativeOpcodeBank[e->pc>>16];
    if(row)row[e->pc&0x7fff]=e->cls_plus_one;
  }
#if defined(TA_R20_NATIVE_MEMORY_OPS) && TA_R20_NATIVE_MEMORY_OPS
  for(unsigned i=0;i<sizeof(s_r20MemorySites)/sizeof(s_r20MemorySites[0]);++i) {
    const TaLookupEntry *e=&s_r20MemorySites[i];
    TaNativeMapWord *row=(TaNativeMapWord *)s_nativeOpcodeBank[e->pc>>16];
    if(row)row[e->pc&0x7fffu]=e->cls_plus_one;
  }
#endif
#if defined(TA_R22_NATIVE_FLOW_OPS) && TA_R22_NATIVE_FLOW_OPS
  for(unsigned i=0;i<sizeof(s_r22FlowSites)/sizeof(s_r22FlowSites[0]);++i) {
    const TaLookupEntry *e=&s_r22FlowSites[i];
    TaNativeMapWord *row=(TaNativeMapWord *)s_nativeOpcodeBank[e->pc>>16];
    if(row) row[e->pc&0x7fffu]=e->cls_plus_one;
  }
#endif
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
  /* Exact-ROM verified, linear fallthrough edges. Use the existing 3-bit
     successor-delta field, with no extra lookup in the hot dispatcher. */
  for(unsigned i=0;i<sizeof(s_r21FallEdges)/sizeof(s_r21FallEdges[0]);++i) {
    const uint32_t pc=s_r21FallEdges[i].pc;
    TaNativeMapWord *row=(TaNativeMapWord *)s_nativeOpcodeBank[pc>>16];
    if(row && (row[pc&0x7fffu]&TA_MAP_CLASS_MASK) &&
       (row[(pc+s_r21FallEdges[i].step)&0x7fffu]&TA_MAP_CLASS_MASK))
      row[pc&0x7fffu]|=(TaNativeMapWord)(s_r21FallEdges[i].step<<TA_MAP_DELTA_SHIFT);
  }
#endif
  for (unsigned i=0;i<sizeof(s_nativeSuccessors)/sizeof(s_nativeSuccessors[0]);++i) {
    const uint32_t pc=s_nativeSuccessors[i].pc;
    TaNativeMapWord* row=(TaNativeMapWord*)s_nativeOpcodeBank[pc>>16];
    if(row)row[pc&0x7fff]|=(TaNativeMapWord)(s_nativeSuccessors[i].delta<<TA_MAP_DELTA_SHIFT);
  }
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
  for(unsigned i=0;i<sizeof(s_r21BranchEdges)/sizeof(s_r21BranchEdges[0]);++i) {
    const unsigned slot=TA_R21_BRANCH_HASH(s_r21BranchEdges[i].pc);
    s_r21BranchIndex[slot]=(uint8_t)(i+1u);
  }
#endif
#if defined(TA_R40_DECODED_DISPATCH) && TA_R40_DECODED_DISPATCH
#if defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
  s_nativeOpcodeMap[0][0x2eadu]=33u;
  s_nativeOpcodeMap[0][0x2eb2u]=34u;
  s_nativeOpcodeMap[0][0x2eb4u]=35u;
#endif
  static const uint8_t banks[6]={0x80,0x82,0x84,0x86,0x89,0x8a};
  for(unsigned b=0;b<6;++b)for(unsigned off=0;off<0x8000u;++off) {
    const unsigned cls=s_nativeOpcodeMap[b][off]&TA_MAP_CLASS_MASK;
    if(!cls)continue;
    const uint32_t pc=((uint32_t)banks[b]<<16)|0x8000u|off;
    const uint32_t next=ta_native_next_in_microblock(pc);
    unsigned word=cls;
    if(next)word|=(unsigned)(next-pc)<<TA_MAP_DELTA_SHIFT;
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
    else if(ta_native_r21_has_branch_next(pc))word|=512u;
#endif
    s_nativeOpcodeMap[b][off]=(TaNativeMapWord)word;
  }
#if TA_R51_BLOCKS_ON
  /* Bit 10 is outside class/successor/branch fields; reuse the existing map. */
  s_nativeOpcodeMap[2][0x412d] |= 1024u;
  s_nativeOpcodeMap[2][0x0162] |= 1024u;
  s_nativeOpcodeMap[2][0x413a] |= 1024u;
#endif
#if TA_R52_LOOP_ON
  s_nativeOpcodeMap[0][0x2ead] |= 2048u;
  s_nativeOpcodeMap[0][0x1086] |= 4096u; /* R48 service, same existing class. */
#endif
#if TA_R58_TASK_ON
  s_nativeOpcodeMap[3][0x370c] |= 8192u; /* Existing bank-86 row; class stays zero. */
#endif
#if TA_R58_IDLE_ON
  for(unsigned slot=0;slot<6;++slot)s_nativeOpcodeMap[3][0x36dc+5u*slot] |= 8192u;
#endif
#if TA_R62_SPRITE_ON
#if TA_R80_SPRITE_ON
  const uint16_t r80entries[]={0x40c8,0x40cd,0x40fb,0x40e3,0x4111,0x40eb,0x4119,0x4129,0x4139,0x4181,0x416a,0x41b2,0x41c9,0x41d7,0x41e8};
  for(unsigned i=0;i<sizeof r80entries/sizeof r80entries[0];++i)s_nativeOpcodeMap[0][r80entries[i]]|=32768u;
#endif
#if TA_R81_ANIMATION_ON
  const uint16_t r81entries[]={0x236a,0x236e,0x2371,0x237a,0x2383,0x238b,0x239e,0x23a4,0x23a7,0x23a9};
  for(unsigned i=0;i<sizeof r81entries/sizeof r81entries[0];++i)s_nativeOpcodeMap[4][r81entries[i]]|=32768u;
#endif
#if defined(TA_R92_FAST_MAP_ROUTING) && TA_R92_FAST_MAP_ROUTING && defined(TA_R91_TRANSFER_LOOP) && TA_R91_TRANSFER_LOOP
  /* R92: reuse the existing R47 descriptor/dispatch bit for this guarded head.
   * Avoid the R91 pre-lookup test on every 65816 dispatch. */
  s_nativeOpcodeMap[0][0x25e1] |= 32768u;
#endif
  s_nativeOpcodeMap[0][0x3db6] |= 32768u;
  s_nativeOpcodeMap[0][0x3dca] |= 32768u;
#if TA_R78_QUIET_ON
  const uint16_t r78entries[]={0x1b2a,0x1b59,0x1b75,0x1b91,0x1be8};
  for(unsigned i=0;i<sizeof r78entries/sizeof r78entries[0];++i)s_nativeOpcodeMap[3][r78entries[i]]|=32768u;
#endif
#if TA_R74_FIELDS_ON
  s_nativeOpcodeMap[1][0x59d4]|=32768u;s_nativeOpcodeMap[1][0x5ac0]|=32768u;
  for(unsigned i=0;i<6;++i)s_nativeOpcodeMap[1][0x59d6+39u*i]|=32768u;
#endif
#if TA_R73_CHAIN_ON
#if defined(TA_R101_SPRITE_RESUME) && TA_R101_SPRITE_RESUME
  /* Instruction-boundary continuations observed after an exact event fence. */
  const uint16_t r101entries[]={0x3c37,0x3c39,0x3c3c,0x3c3f,0x3c4b,0x3dba,0x3dbc,0x3dc3,0x3dc5,0x3dc8,0x3dcd,0x3dcf,0x3dd1,0x3dd3,0x3de0};
  for(unsigned i=0;i<sizeof r101entries/sizeof r101entries[0];++i)s_nativeOpcodeMap[0][r101entries[i]]|=32768u;
#endif
  const uint16_t r73entries[]={0x3bea,0x3bfa,0x3c15,0x3c25,0x3c33,0x3c42,0x3d6d,0x3d7d,0x3d98,0x3da8,0x3dbf,0x3dd6};
  /* R73 declared twelve entries but registered only the BB half.
   * R77 enables the six BD continuations; historical builds keep their map. */
#if TA_R77_ENTRY_FIX_ON
  for(unsigned i=0;i<sizeof r73entries/sizeof r73entries[0];++i)
#else
  for(unsigned i=0;i<6;++i)
#endif
    s_nativeOpcodeMap[0][r73entries[i]]|=32768u;
#endif
#if TA_R72_RECORD_ON
  s_nativeOpcodeMap[0][0x3c33] |= 32768u; /* Reuse existing record-entry bit. */
#endif
#endif
#if TA_R58_COUNTER_ON
  s_nativeOpcodeMap[2][0x0172] |= 16384u;
#endif
#endif
}
#endif

#if TA_R62_SPRITE_ON && TA_R62_COVERAGE
static uint32_t s_r62Attempts[2],s_r62Hits[2];
void ta_native_r62_counts(unsigned b,uint32_t* a,uint32_t* h){if(a)*a=b<2?s_r62Attempts[b]:0;if(h)*h=b<2?s_r62Hits[b]:0;}
#endif
bool ta_native_init(const uint8_t *rom, size_t length) {
#if TA_R62_SPRITE_ON && TA_R62_COVERAGE
  for(unsigned b=0;b<2;++b)s_r62Attempts[b]=s_r62Hits[b]=0;
#endif
  s_identical_rom=false;
#if TA_R60_STORE_ON || TA_R61_WORKERS_ON || TA_R62_SPRITE_ON
  s_r60ShaVerified=false;
#if TA_R61_WORKERS_ON && TA_R61_WORKER_COVERAGE
  for(unsigned i=0;i<49;++i)s_r61WorkerHits[i]=0;
#endif
#endif
  for(unsigned i=0;i<TA_R8_CLASSES;i++) s_opcode_hits[i]=0;
  s_microblock_calls=s_microblock_instructions=s_microblock_max_length=0;
#if defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
  for(unsigned i=0;i<3;++i)s_r23HotHits[i]=0;
#endif
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
  s_r21DynamicLinks=0;
#if defined(TA_R24_LOOP_BACKEDGE) && TA_R24_LOOP_BACKEDGE && defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
  s_r24LoopBackLinks=0;
#endif
#endif
#if defined(TA_NATIVE_ENABLED) && TA_NATIVE_ENABLED
  if(rom && length==1048576u && rom[0x132u]==0xe8u &&
     fingerprint(rom,length)==UINT64_C(0xe448796ad235bc90))
    s_identical_rom=true;
#else
  (void)rom;(void)length;
#endif
#if defined(TA_R15_NATIVE_LOOKUP) && TA_R15_NATIVE_LOOKUP
  if(s_identical_rom) ta_r15InitNativeMap();
#endif
#if TA_R60_STORE_ON || TA_R61_WORKERS_ON || TA_R62_SPRITE_ON
  if(s_identical_rom)s_r60ShaVerified=ta_r60_rom_sha256(rom,length);
#endif
  return s_identical_rom;
}
uint32_t ta_native_next_in_microblock(uint32_t pc) {
#if defined(TA_R32_NATIVE_DELAY_LOOP) && TA_R32_NATIVE_DELAY_LOOP
  if(s_identical_rom && pc>=0x83BF52u && pc<0x83BF57u)return pc+1u;
#endif
#if defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
  /* R23: verified instruction-length fallthroughs in the hot AEAD loop.
     cpu_runFrameOpcode still checks PC/frame/interrupt boundaries after each. */
  if(s_identical_rom) {
    switch(pc) {
      case 0x80AEADu: return 0x80AEB1u; /* LDA long,X -> CLC */
      case 0x80AEB1u: return 0x80AEB2u; /* CLC -> ADC dp */
      case 0x80AEB2u: return 0x80AEB4u; /* ADC dp -> STA dp */
      case 0x80AEB4u: return 0x80AEB6u; /* STA dp -> DEX */
      default: break;
    }
  }
#endif
#if defined(TA_NATIVE_ENABLED) && TA_NATIVE_ENABLED && defined(TA_R10_MICROBLOCKS) && TA_R10_MICROBLOCKS
  if(!s_identical_rom)return 0;
#if defined(TA_R20_NATIVE_MEMORY_OPS) && TA_R20_NATIVE_MEMORY_OPS
  /* Verified fall-through successors after 3/4-byte memory instructions.
     Unlike the preexisting 1/2-byte successor-map entries, these use a
     small explicit switch. cpu_runFrameOpcode rechecks actual PC, frame,
     NMI/IRQ, and DMA after EACH instruction before continuing. */
  switch(pc) {
    case 0x80942Bu: return 0x80942Eu; /* CMP abs -> BNE */
    case 0x8094D5u: return 0x8094D8u; /* CMP abs -> BNE */
    case 0x8094DAu: return 0x8094DDu; /* CMP abs -> BNE */
    case 0x809089u: return 0x80908Du; /* LDA long -> BIT imm */
    case 0x8AC0ABu: return 0x8AC0AFu;
    case 0x8AC0C0u: return 0x8AC0C4u;
    case 0x8AC0D2u: return 0x8AC0D6u;
    case 0x8AC939u: return 0x8AC93Du;
    default: break;
  }
#endif
#if defined(TA_R15_NATIVE_LOOKUP) && TA_R15_NATIVE_LOOKUP
  /* A zero successor means the original frame-level dispatcher resumes. */
  const TaNativeMapWord *row=s_nativeOpcodeBank[(uint8_t)(pc>>16)];
  if(!row || !(pc&0x8000u))return 0;
  unsigned delta=(unsigned)(row[pc&0x7fffu]>>TA_MAP_DELTA_SHIFT)&7u;
  return delta ? pc+(uint32_t)delta : 0u;
#else
  switch(pc){
    case 0x808120u: return 0x808121u;
    case 0x808121u: return 0x808122u;
    case 0x808131u: return 0x808132u;
    case 0x808132u: return 0x808133u;
    case 0x809086u: return 0x809087u;
    case 0x809097u: return 0x809098u;
    case 0x8091C4u: return 0x8091C5u;
    case 0x8091C5u: return 0x8091C6u;
    case 0x8093FDu: return 0x8093FEu;
    case 0x8093FEu: return 0x8093FFu;
    case 0x809436u: return 0x809438u;
    case 0x809438u: return 0x809439u;
    case 0x809494u: return 0x809495u;
    case 0x8094F3u: return 0x8094F4u;
    case 0x8094F4u: return 0x8094F5u;
    case 0x809BDCu: return 0x809BDDu;
    case 0x809C02u: return 0x809C03u;
    case 0x809C80u: return 0x809C81u;
    case 0x809F5Eu: return 0x809F5Fu;
    case 0x809F5Fu: return 0x809F60u;
    case 0x809F60u: return 0x809F61u;
    case 0x809F61u: return 0x809F62u;
    case 0x80A0C8u: return 0x80A0C9u;
    case 0x80A11Au: return 0x80A11Bu;
    case 0x80A595u: return 0x80A596u;
    case 0x80A5AEu: return 0x80A5AFu;
    case 0x80A5CFu: return 0x80A5D0u;
    case 0x80A5DAu: return 0x80A5DBu;
    case 0x80A5DEu: return 0x80A5DFu;
    case 0x80A5E7u: return 0x80A5E8u;
    case 0x80A724u: return 0x80A725u;
    case 0x80A725u: return 0x80A726u;
    case 0x80AEB6u: return 0x80AEB7u;
    case 0x80AEC8u: return 0x80AEC9u;
    case 0x80BBAFu: return 0x80BBB0u;
    case 0x80BBB9u: return 0x80BBBAu;
    case 0x80BBBFu: return 0x80BBC0u;
    case 0x80BBC0u: return 0x80BBC1u;
    case 0x80BBC1u: return 0x80BBC2u;
    case 0x80BBC9u: return 0x80BBCAu;
    case 0x80BBDFu: return 0x80BBE0u;
    case 0x80BBE0u: return 0x80BBE1u;
    case 0x80BC42u: return 0x80BC43u;
    case 0x80BC43u: return 0x80BC44u;
    case 0x80BC44u: return 0x80BC45u;
    case 0x80BC45u: return 0x80BC46u;
    case 0x80BC46u: return 0x80BC47u;
    case 0x80BC47u: return 0x80BC48u;
    case 0x80BC48u: return 0x80BC49u;
    case 0x80BC49u: return 0x80BC4Au;
    case 0x80BC52u: return 0x80BC53u;
    case 0x80BC53u: return 0x80BC54u;
    case 0x80C08Du: return 0x80C08Eu;
    case 0x80C097u: return 0x80C098u;
    case 0x80C09Du: return 0x80C09Eu;
    case 0x80C09Eu: return 0x80C09Fu;
    case 0x80C09Fu: return 0x80C0A0u;
    case 0x80C0A7u: return 0x80C0A8u;
    case 0x80C0BDu: return 0x80C0BEu;
    case 0x80C0BEu: return 0x80C0BFu;
    case 0x80C1E8u: return 0x80C1E9u;
    case 0x80C1E9u: return 0x80C1EAu;
    case 0x80C1EAu: return 0x80C1EBu;
    case 0x80C1EBu: return 0x80C1ECu;
    case 0x80C1ECu: return 0x80C1EDu;
    case 0x80C1EDu: return 0x80C1EEu;
    case 0x80C1EEu: return 0x80C1EFu;
    case 0x80C1EFu: return 0x80C1F0u;
    case 0x80C1FBu: return 0x80C1FCu;
    case 0x80C1FCu: return 0x80C1FDu;
    case 0x80CA2Fu: return 0x80CA30u;
    case 0x80CA30u: return 0x80CA31u;
    case 0x8480FBu: return 0x8480FCu;
    case 0x848162u: return 0x848163u;
    case 0x8481CBu: return 0x8481CCu;
    case 0x8481F9u: return 0x8481FAu;
    case 0x84903Bu: return 0x84903Cu;
    case 0x84918Eu: return 0x84918Fu;
    case 0x84C132u: return 0x84C133u;
    case 0x84C133u: return 0x84C134u;
    case 0x84C192u: return 0x84C193u;
    case 0x84C193u: return 0x84C194u;
    case 0x89A366u: return 0x89A367u;
    case 0x89A392u: return 0x89A393u;
    case 0x89A393u: return 0x89A395u;
    case 0x89A39Au: return 0x89A39Bu;
    case 0x89A39Bu: return 0x89A39Cu;
    case 0x89A3ABu: return 0x89A3ACu;
    case 0x89A3ACu: return 0x89A3ADu;
    case 0x8A88E9u: return 0x8A88EAu;
    case 0x8A8AC3u: return 0x8A8AC4u;
    case 0x8ABE8Fu: return 0x8ABE90u;
    case 0x8AC0E7u: return 0x8AC0E8u;
    case 0x8AC21Cu: return 0x8AC21Du;
    case 0x8AD309u: return 0x8AD30Au;
    case 0x8AD314u: return 0x8AD315u;
    case 0x8AD3C6u: return 0x8AD3C7u;
    case 0x8AD493u: return 0x8AD494u;
    default:return 0;
  }
#endif
#else
  (void)pc;return 0;
#endif
}

#if defined(TA_R112_NATIVE_9AC4) && TA_R112_NATIVE_9AC4
bool ta_native_r112_sha_verified(void){return s_identical_rom && s_r60ShaVerified;}
#endif
uint32_t ta_native_enabled_sites(void) {
#if defined(TA_R32_NATIVE_DELAY_LOOP) && TA_R32_NATIVE_DELAY_LOOP
  const uint32_t extra=s_identical_rom ? 6u : 0u;
#else
  const uint32_t extra=0;
#endif
#if defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP && \
    defined(TA_R22_NATIVE_FLOW_OPS) && TA_R22_NATIVE_FLOW_OPS && \
    defined(TA_R20_NATIVE_MEMORY_OPS) && TA_R20_NATIVE_MEMORY_OPS && \
    defined(TA_R15_NATIVE_LOOKUP) && TA_R15_NATIVE_LOOKUP
  return (s_identical_rom?566u:0u)+extra;
#elif defined(TA_R22_NATIVE_FLOW_OPS) && TA_R22_NATIVE_FLOW_OPS && \
    defined(TA_R20_NATIVE_MEMORY_OPS) && TA_R20_NATIVE_MEMORY_OPS && \
    defined(TA_R15_NATIVE_LOOKUP) && TA_R15_NATIVE_LOOKUP
  return (s_identical_rom?563u:0u)+extra;
#elif defined(TA_R20_NATIVE_MEMORY_OPS) && TA_R20_NATIVE_MEMORY_OPS && \
    defined(TA_R15_NATIVE_LOOKUP) && TA_R15_NATIVE_LOOKUP
  return (s_identical_rom?500u:0u)+extra;
#else
  return (s_identical_rom?466u:0u)+extra;
#endif
}
uint32_t ta_native_opcode_hits(unsigned cls){return cls<TA_R8_CLASSES?s_opcode_hits[cls]:0u;}
uint32_t ta_native_site_hits(void) {
  uint32_t n=0;for(unsigned i=0;i<TA_R8_CLASSES;i++)n+=s_opcode_hits[i];
#if defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
  for(unsigned i=0;i<3;++i)n+=s_r23HotHits[i];
#endif
  return n;
}
static void ta_zn_x(Cpu *cpu){
  cpu->z=cpu->xf?((cpu->x&255u)==0u):(cpu->x==0u);
  cpu->n=cpu->xf?((cpu->x&128u)!=0u):((cpu->x&32768u)!=0u);
}
static void ta_zn_y(Cpu *cpu){
  cpu->z=cpu->xf?((cpu->y&255u)==0u):(cpu->y==0u);
  cpu->n=cpu->xf?((cpu->y&128u)!=0u):((cpu->y&32768u)!=0u);
}
static void ta_zn_a(Cpu *cpu){
  cpu->z=cpu->mf?((cpu->a&255u)==0u):(cpu->a==0u);
  cpu->n=cpu->mf?((cpu->a&128u)!=0u):((cpu->a&32768u)!=0u);
}
static uint8_t ta_getFlags(const Cpu *cpu){
  return (uint8_t)((cpu->n<<7)|(cpu->v<<6)|(cpu->mf<<5)|
                   (cpu->xf<<4)|(cpu->d<<3)|(cpu->i<<2)|(cpu->z<<1)|cpu->c);
}
static void ta_setFlags(Cpu *cpu,uint8_t val){
  cpu->n=(val&0x80u)!=0;cpu->v=(val&0x40u)!=0;
  cpu->mf=(val&0x20u)!=0;cpu->xf=(val&0x10u)!=0;
  cpu->d=(val&8u)!=0;cpu->i=(val&4u)!=0;
  cpu->z=(val&2u)!=0;cpu->c=(val&1u)!=0;
  if(cpu->e){cpu->mf=true;cpu->xf=true;cpu->sp=(cpu->sp&255u)|256u;}
  if(cpu->xf){cpu->x&=255u;cpu->y&=255u;}
}
#if TA_R48_SERVICE_ON
#if defined(TA_R48_HOST_COVERAGE) && TA_R48_HOST_COVERAGE
uint64_t ta_r48HostServiceHits;
uint64_t ta_r48HostRejects[11];
#define TA_R48_REJECT(reason) do {++ta_r48HostRejects[reason];return 0;} while(0)
#else
#define TA_R48_REJECT(reason) return 0
#endif
/* Proven no-work branch: PHP; REP #$20; LDA $0000A1; BIT #$4000;
 * BNE (not taken); BIT #$2000; BNE (not taken); PLP; RTL.
 * No hardware or observer can see intermediate transactions inside the
 * existing event-free fence. Keep stack residue, final bus and every clock.
 * 19 ROM bytes + 7 WRAM bytes + 6 six-clock internal cycles. */
unsigned ta_native_r48_quiet_service(Cpu* cpu,unsigned opcodeBudget) {
#if defined(TA_R48_HOST_COVERAGE) && TA_R48_HOST_COVERAGE
  if(cpu){if(cpu->sp<0x100)++ta_r48HostRejects[6];if(cpu->sp>0x1ffc)++ta_r48HostRejects[7];
    if(cpu->nmiWanted)++ta_r48HostRejects[8];if(cpu->irqWanted&&!cpu->i)++ta_r48HostRejects[9];
    if(cpu->e)++ta_r48HostRejects[10];}
#endif
  if(!cpu || opcodeBudget<9u || !s_identical_rom || cpu->k!=0x80u ||
     cpu->pc!=0x9086u || cpu->e || cpu->waiting || cpu->stopped ||
     cpu->resetWanted || cpu->intWanted || cpu->nmiWanted || (cpu->irqWanted && !cpu->i) ||
     cpu->sp<0x0100u || cpu->sp>0x1ffcu) TA_R48_REJECT(0);
  Snes* s=(Snes*)cpu->mem;
  if(!s || s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
     s->dma->hdmaInitRequested || s->dma->hdmaRunRequested || s->irqCondition) TA_R48_REJECT(1);
  const unsigned clocks=19u*(s->fastMem?6u:8u)+92u;
  const unsigned h=s->hPos,end=h+clocks;
  if((h&1u) || h>=1358u || end>ta_r41_busEndLimit[h]) TA_R48_REJECT(2);
  /* Match snes_runCycle's comparator over every two-clock position [h,end).
   * A vertical mismatch cannot assert an IRQ on this fenced line. Otherwise
   * horizontal comparison must remain false, including any edge at h.
   * An edge at end is left to the next original bus step. */
  /* Real auto-joy timers are even. Keep the scalar reference's uint16_t
   * behavior for synthetic odd timers when timer IRQs select scalar stepping. */
  if((s->hIrqEnabled || s->vIrqEnabled) && (s->autoJoyTimer&1u)) TA_R48_REJECT(3);
  if((s->hIrqEnabled || s->vIrqEnabled) &&
     (!s->vIrqEnabled || s->vPos==s->vTimer)) {
    const unsigned irqClock=(unsigned)s->hTimer*4u;
    if(!s->hIrqEnabled || (irqClock>=h && irqClock<end)) TA_R48_REJECT(4);
  }
  const uint16_t value=(uint16_t)(s->ram[0xa1]|((uint16_t)s->ram[0xa2]<<8));
  if(value&0x6000u) TA_R48_REJECT(5);
  const unsigned sp=cpu->sp;
  const uint8_t flags=ta_getFlags(cpu);
  const uint16_t returnPc=(uint16_t)(s->ram[sp+1u]|((uint16_t)s->ram[sp+2u]<<8));
  const uint8_t returnBank=s->ram[sp+3u];
  s->ram[sp]=flags;
  cpu->a=value;
  ta_setFlags(cpu,flags);
  cpu->sp=(uint16_t)(sp+3u);
  cpu->pc=(uint16_t)(returnPc+1u);
  cpu->k=returnBank;
  s->openBus=returnBank;
  s->apuMasterPending+=clocks;
  s->hPos=(uint16_t)end;
  s->cycles+=clocks;
  if(s->autoJoyTimer>0) s->autoJoyTimer=s->autoJoyTimer<=clocks?0:
    (uint16_t)(s->autoJoyTimer-clocks);
#if defined(TA_R48_HOST_COVERAGE) && TA_R48_HOST_COVERAGE
  ++ta_r48HostServiceHits;
#endif
#if defined(TA_R54_XBOX_PROFILE) && TA_R54_XBOX_PROFILE
  ++s_r54Hot[0];
#endif
  return 9u;
}
#undef TA_R48_REJECT
#endif
#if TA_R51_BLOCKS_ON
#if defined(TA_R51_HOST_COVERAGE) && TA_R51_HOST_COVERAGE
uint64_t ta_r51HostBlockHits[3];
#endif
/* Three separately fenced portions of $84:C12D's recurring service call.
 * Clocks count every ROM byte, WRAM byte and internal six-clock cycle.
 * No group crosses a refresh, raster, IRQ edge, DMA or bus observer. */
unsigned ta_native_r51_game_block(Cpu* cpu,unsigned opcodeBudget) {
  if(!cpu || !s_identical_rom || cpu->k!=0x84 || cpu->e ||
     cpu->waiting || cpu->stopped || cpu->resetWanted || cpu->intWanted ||
     cpu->nmiWanted || (cpu->irqWanted&&!cpu->i))return 0;
  unsigned kind,count,romBytes,otherClocks;
  const unsigned sp=cpu->sp;
  if(cpu->pc==0xc12d) {
    kind=0;count=7;romBytes=13;otherClocks=120;
    if(sp<0x108 || sp>0x1fff)return 0;
  } else if(cpu->pc==0x8162) {
    kind=1;count=9;romBytes=18;otherClocks=92;
    if(cpu->db!=0x7e || sp<0x100 || sp>0x1ffc || sp==0x2a0 || sp==0x2a1)return 0;
  } else if(cpu->pc==0xc13a) {
    kind=2;count=9;romBytes=19;otherClocks=120;
    if(cpu->db!=0x7e || cpu->mf || sp<0x100 || sp>0x1ffa)return 0;
  } else return 0;
  if(opcodeBudget<count)return 0;
  Snes* s=(Snes*)cpu->mem;
  if(!s || s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
     s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return 0;
  const unsigned clocks=romBytes*(s->fastMem?6u:8u)+otherClocks;
  const unsigned h=s->hPos,end=h+clocks;
  if((h&1u) || h>=1358u || end>ta_r41_busEndLimit[h] ||
     !ta_r49_noTimerEdge(s,h,end))return 0;
  uint16_t value=0;
  if(kind==1) {
    value=(uint16_t)(s->ram[0x2a0]|((uint16_t)s->ram[0x2a1]<<8));
    if(value<2)return 0;
  } else if(kind==2) {
    if(s->ram[0xdc51]&0x80)return 0;
    value=(uint16_t)(s->ram[0xe042]|((uint16_t)s->ram[0xe043]<<8));
    if(value&0x8000)return 0;
  }
  if(kind==0) {
    s->ram[sp]=ta_getFlags(cpu);s->ram[sp-1]=cpu->db;
    s->ram[sp-2]=0x84;s->ram[sp-3]=0xc1;s->ram[sp-4]=0x39;
    cpu->sp=(uint16_t)(sp-5);cpu->pc=0x8162;cpu->db=0x7e;
    cpu->mf=false;cpu->xf=false;cpu->n=false;cpu->z=false;s->openBus=0x39;
  } else if(kind==1) {
    const uint8_t flags=ta_getFlags(cpu);s->ram[sp]=flags;
    cpu->a=value;ta_setFlags(cpu,flags);cpu->sp=(uint16_t)(sp+3);
    cpu->pc=(uint16_t)((s->ram[sp+1]|((uint16_t)s->ram[sp+2]<<8))+1);
    cpu->k=s->ram[sp+3];s->openBus=cpu->k;
  } else {
    cpu->a=value;cpu->db=s->ram[sp+1];ta_setFlags(cpu,s->ram[sp+2]);
    cpu->sp=(uint16_t)(sp+5);
    cpu->pc=(uint16_t)((s->ram[sp+3]|((uint16_t)s->ram[sp+4]<<8))+1);
    cpu->k=s->ram[sp+5];s->openBus=cpu->k;
    /* RTL samples after PLP; a saved unmasked P can expose a pending IRQ. */
    cpu->intWanted=cpu->irqWanted&&!cpu->i;
  }
  s->hPos=(uint16_t)end;s->cycles+=clocks;s->apuMasterPending+=clocks;
  if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if defined(TA_R51_HOST_COVERAGE) && TA_R51_HOST_COVERAGE
  ++ta_r51HostBlockHits[kind];
#endif
#if defined(TA_R54_XBOX_PROFILE) && TA_R54_XBOX_PROFILE
  ++s_r54Hot[1u+kind];
#endif
  return count;
}
#endif
#if TA_R51_BLOCKS_ON && defined(TA_R55_SERVICE_CHAIN) && TA_R55_SERVICE_CHAIN
/* R55: chain *existing* R51 fenced services after each complete group.
 * $84:C12D -> $84:8162 -> $84:C13A is an observed real-game path.
 * Every stage uses unchanged R51 checks for state, observer, bus and IRQ
 * fences. A rejected stage ends the chain without changing the fallback.
 * Never merge clocks or skip any R51 memory writes. */
#if defined(TA_R55_HOST_COVERAGE) && TA_R55_HOST_COVERAGE
uint64_t ta_r55HostChainedCalls,ta_r55HostExtraBlocks;
#endif
static unsigned ta_native_r55_chained_game_blocks(Cpu* cpu,unsigned opcodeBudget) {
  unsigned n=ta_native_r51_game_block(cpu,opcodeBudget);
#if TA_R83_RESIDUAL_PROFILE
  /* Count one R55 attempt at the actual dispatch site, not in a synthetic test. */
  ta_r83_service_attempt(n!=0u);
#endif
  if(!n)return 0u;
  unsigned blocks=1u;
  if(cpu->k==0x84u && cpu->pc==0x8162u) {
    const unsigned extra=ta_native_r51_game_block(cpu,opcodeBudget-n);
    if(extra){ n+=extra; ++blocks; }
  }
  if(cpu->k==0x84u && cpu->pc==0xc13au) {
    const unsigned extra=ta_native_r51_game_block(cpu,opcodeBudget-n);
    if(extra){ n+=extra; ++blocks; }
  }
#if defined(TA_R55_HOST_COVERAGE) && TA_R55_HOST_COVERAGE
  if(blocks>1u){ ++ta_r55HostChainedCalls;ta_r55HostExtraBlocks+=blocks-1u; }
#endif
  return n;
}
#endif
#if TA_R52_LOOP_ON
#if defined(TA_R53_XBOX_COUNTERS) && TA_R53_XBOX_COUNTERS
static uint32_t s_r53Attempts=0,s_r53Accepts=0,s_r53Iterations=0;
void ta_native_r53_loop_counts(uint32_t *attempts,uint32_t *accepts,uint32_t *iterations) {
  if(attempts)*attempts=s_r53Attempts;
  if(accepts)*accepts=s_r53Accepts;
  if(iterations)*iterations=s_r53Iterations;
}
#endif
#if defined(TA_R52_HOST_COVERAGE) && TA_R52_HOST_COVERAGE
uint64_t ta_r52HostLoopCalls,ta_r52HostLoopIterations;
#endif
/* $80:AEAD: LDA $7EF148,X; CLC; ADC $00; STA $00; DEX; BNE $AEAD.
 * M=8, X=16, binary arithmetic; DP must map bank-00 WRAM. Source bytes lie
 * in $7E:F148..$7F:F147, disjoint from the guarded direct-page destination.
 * Aggregate only complete iterations within the original event/IRQ fence.
 * No bus observer can see intermediate writes in this span. */
unsigned ta_native_r52_accumulate(Cpu* cpu,unsigned opcodeBudget) {
#if defined(TA_R53_XBOX_COUNTERS) && TA_R53_XBOX_COUNTERS
  ++s_r53Attempts;
#endif
  if(!cpu || opcodeBudget<6 || !s_identical_rom || cpu->k!=0x80 || cpu->pc!=0xaead ||
     cpu->e || !cpu->mf || cpu->xf || cpu->d || cpu->dp>=0x2000 ||
     cpu->waiting || cpu->stopped || cpu->resetWanted || cpu->intWanted ||
     cpu->nmiWanted || (cpu->irqWanted&&!cpu->i))return 0;
  Snes* s=(Snes*)cpu->mem;
  if(!s || s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
     s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return 0;
  unsigned h=s->hPos;
  if((h&1u) || h>=1358)return 0;
  unsigned limit=ta_r41_busEndLimit[h];
  /* Resolve the timer fence once for this group, retaining the R49 rule. */
  if(s->irqCondition)return 0;
  if(s->hIrqEnabled || s->vIrqEnabled) {
    if(s->autoJoyTimer&1u)return 0;
    if(!s->vIrqEnabled || s->vPos==s->vTimer) {
      if(!s->hIrqEnabled)return 0;
      const unsigned edge=(unsigned)s->hTimer*4u;
      if(edge>=h && edge<limit)limit=edge;
    }
  }
  /* Twelve code bytes, three WRAM accesses, CLC/DEX idles; taken BNE adds 6.
   * Each unaligned direct-page access adds its original six-clock idle. */
  const unsigned base=12u*(s->fastMem?6u:8u)+36u+((cpu->dp&255u)?12u:0u);
  unsigned run=0,sum=0,value=0,prior=0;
  uint16_t x=cpu->x;
  uint8_t accumulated=s->ram[cpu->dp];
#if defined(TA_R53_GROUP_FENCE) && TA_R53_GROUP_FENCE
  /* R53: calculate the exact number of complete six-opcode iterations once.
   * Taken BNE adds six clocks; the terminating BNE does not. Handle that
   * one-iteration edge explicitly, including when only a terminating body
   * fits the original timing fence. No writes occur before acceptance. */
  const unsigned takenClocks=base+6u;
  if(limit<h)return 0; /* Table may reject the current horizontal slot. */
  const unsigned available=limit-h;
  const unsigned untilZero=cpu->x?(unsigned)cpu->x:65536u;
  const unsigned allowed=opcodeBudget/6u;
  const unsigned capped=allowed<untilZero?allowed:untilZero;
  /* 18-bit fixed-point reciprocal: exact floor(available/takenClocks)
   * for available <= 1358. R41's entire table is bounded by 1358.
   * Avoid the Pentium III variable 32-bit hardware division. */
  const unsigned reciprocal=s->fastMem?
      ((cpu->dp&255u)?2081u:2300u):((cpu->dp&255u)?1748u:1900u);
  const unsigned takenFit=(available*reciprocal)>>18;
  unsigned count=capped<takenFit?capped:takenFit;
  if(untilZero==takenFit+1u && untilZero<=allowed &&
     available>=(untilZero-1u)*takenClocks+base) count=untilZero;
  if(!count)return 0;
  const uint16_t lastX=(uint16_t)(cpu->x-count);
  for(unsigned i=0;i<count;++i) {
    value=s->ram[0xf148u+x];prior=accumulated;
    sum=value+prior;accumulated=(uint8_t)sum;
    x=(uint16_t)(x-1u);
  }
  /* x==0 implies exactly one final untaken branch. */
  h+=count*takenClocks-(lastX==0?6u:0u);
  run=count*6u;
#else
  while(opcodeBudget-run>=6u) {
    const uint16_t next=(uint16_t)(x-1u);
    const unsigned end=h+base+(next?6u:0u);
    if(end>limit)break;
    value=s->ram[0xf148u+x];prior=accumulated;
    sum=value+prior;accumulated=(uint8_t)sum;
    x=next;h=end;run+=6;
    if(!x)break;
  }
  if(!run)return 0;
#endif
  s->ram[cpu->dp]=accumulated;
  cpu->a=(uint16_t)((cpu->a&0xff00u)|accumulated);cpu->x=x;
  cpu->c=sum>255u;cpu->v=((value^sum)&(prior^sum)&0x80u)!=0;
  cpu->z=x==0;cpu->n=(x&0x8000u)!=0;cpu->pc=x?0xaead:0xaeb9;
  s->openBus=0xf4;
  const unsigned clocks=h-s->hPos;
  s->hPos=(uint16_t)h;s->cycles+=clocks;s->apuMasterPending+=clocks;
  if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if defined(TA_R53_XBOX_COUNTERS) && TA_R53_XBOX_COUNTERS
  ++s_r53Accepts;s_r53Iterations+=run/6u;
#endif
#if defined(TA_R52_HOST_COVERAGE) && TA_R52_HOST_COVERAGE
  ++ta_r52HostLoopCalls;ta_r52HostLoopIterations+=run/6;
#endif
  return run;
}
#endif
#if TA_R58_TASK_ON
#if defined(TA_R58_HOST_COVERAGE) && TA_R58_HOST_COVERAGE
uint64_t ta_r58HostTaskHits[3];
#endif
/* $86:B70C: finish zero/inactive entries through RTS, or compile the active
 * prefix through TAX and leave JSR ($B724,X) at $B720 to the original core.
 * DB=$86 low RAM mirrors bank 00; guarded source ranges and DP do not alias.
 * Preserve both stack reads and every clock; no event or observer is skipped. */
unsigned ta_native_r58_task_prefix(Cpu* cpu,unsigned opcodeBudget) {
  if(!cpu || !s_identical_rom || cpu->k!=0x86 || cpu->pc!=0xb70c ||
     cpu->db!=0x86 || cpu->dp || cpu->e || cpu->mf || cpu->xf ||
     cpu->y>0x0e82 || cpu->waiting || cpu->stopped || cpu->resetWanted ||
     cpu->intWanted || cpu->nmiWanted || (cpu->irqWanted&&!cpu->i))return 0;
  Snes* s=(Snes*)cpu->mem;
  if(!s || s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
     s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return 0;
  const unsigned adr=0x0f7cu+cpu->y;
  const uint16_t value=(uint16_t)(s->ram[adr]|((uint16_t)s->ram[adr+1]<<8));
  const unsigned kind=value==0?0:(value&0x8000u)?2:1;
  const unsigned count=kind==0?3:kind==1?5:9;
  if(opcodeBudget<count)return 0;
  const unsigned sp=cpu->sp;
  if(kind!=2 && (sp<0x100 || sp>0x1ffd))return 0;
  const unsigned clocks=(kind==0?6u:kind==1?11u:20u)*(s->fastMem?6u:8u)+(kind==2?72u:62u);
  const unsigned h=s->hPos,end=h+clocks;
  if((h&1u) || h>=1358 || end>ta_r41_busEndLimit[h] || !ta_r49_noTimerEdge(s,h,end))return 0;
  if(kind!=2) {
    cpu->a=value;cpu->z=true;cpu->n=false;
    const uint16_t ret=(uint16_t)(s->ram[sp+1]|((uint16_t)s->ram[sp+2]<<8));
    cpu->pc=(uint16_t)(ret+1);cpu->sp=(uint16_t)(sp+2);s->openBus=(uint8_t)(ret>>8);
  } else {
    s->ram[0]=(uint8_t)value;s->ram[1]=0;
    const unsigned second=0x117cu+cpu->y;
    const uint16_t v=(uint16_t)(s->ram[second]|((uint16_t)s->ram[second+1]<<8));
    cpu->a=(uint16_t)(v<<1);cpu->x=cpu->a;cpu->c=(v&0x8000u)!=0;
    cpu->z=cpu->a==0;cpu->n=(cpu->a&0x8000u)!=0;cpu->pc=0xb720;s->openBus=0xaa;
  }
  s->hPos=(uint16_t)end;s->cycles+=clocks;s->apuMasterPending+=clocks;
  if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if defined(TA_R58_HOST_COVERAGE) && TA_R58_HOST_COVERAGE
  ++ta_r58HostTaskHits[kind];
#endif
  return count;
}
#endif
#if TA_R58_IDLE_ON
#if defined(TA_R58_HOST_COVERAGE) && TA_R58_HOST_COVERAGE
uint64_t ta_r58HostIdleCalls,ta_r58HostIdleSlots;
#endif
/* The caller visits six adjacent task slots. For zero or inactive entries,
 * batch complete JSR/RTS and INY pairs, ending at an original opcode boundary.
 * Active callbacks, source/stack aliases and timing events stay original. */
unsigned ta_native_r58_idle_slots(Cpu* cpu,unsigned opcodeBudget) {
  if(!cpu || !s_identical_rom || cpu->k!=0x86 || cpu->db!=0x86 || cpu->e || cpu->mf || cpu->xf ||
     cpu->pc<0xb6dc || cpu->pc>0xb6f5 || (cpu->pc-0xb6dc)%5u || cpu->waiting || cpu->stopped ||
     cpu->resetWanted || cpu->intWanted || cpu->nmiWanted || (cpu->irqWanted&&!cpu->i) ||
     cpu->sp<0x102 || cpu->sp>0x1fff)return 0;
  unsigned slot=(cpu->pc-0xb6dc)/5u;
  const unsigned source=0xf7cu+cpu->y,top=source+(6u-slot)*2u-1u,sp=cpu->sp;
  if(top>0x1fff || (sp>=source && sp<=top) || (sp-1u>=source && sp-1u<=top))return 0;
  Snes* s=(Snes*)cpu->mem;
  if(!s || s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
     s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return 0;
  unsigned h=s->hPos;
  if((h&1u) || h>=1358)return 0;
  const unsigned limit=ta_r41_busEndLimit[h],fetch=s->fastMem?6u:8u;
  uint16_t y=cpu->y,value=0;
  unsigned run=0,slots=0,last=slot;
  while(slot<6u) {
    const unsigned adr=0xf7cu+y;
    const uint16_t v=(uint16_t)(s->ram[adr]|((uint16_t)s->ram[adr+1]<<8));
    if(v&0x8000u)break;
    const bool more=slot<5u;
    const unsigned count=(v?6u:4u)+(more?2u:0u);
    const unsigned clocks=(v?14u:9u)*fetch+84u+(more?2u*fetch+12u:0u);
    const unsigned end=h+clocks;
    if(count>opcodeBudget-run || end>limit || !ta_r49_noTimerEdge(s,h,end))break;
    value=v;last=slot;run+=count;++slots;h=end;++slot;
    if(more)y=(uint16_t)(y+2u);
  }
  if(!run)return 0;
  /* All source words were proven disjoint from these repeated stack pushes. */
  s->ram[sp]=0xb6;s->ram[sp-1]=(uint8_t)(0xdeu+last*5u);
  cpu->a=value;cpu->y=y;cpu->pc=(uint16_t)(last<5u?0xb6e1u+last*5u:0xb6f8u);
  if(last<5u){cpu->n=(y&0x8000u)!=0;cpu->z=y==0;s->openBus=0xc8;}
  else {cpu->n=false;cpu->z=true;s->openBus=0xb6;}
  const unsigned clocks=h-s->hPos;s->hPos=(uint16_t)h;s->cycles+=clocks;s->apuMasterPending+=clocks;
  if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if defined(TA_R58_HOST_COVERAGE) && TA_R58_HOST_COVERAGE
  ++ta_r58HostIdleCalls;ta_r58HostIdleSlots+=slots;
#endif
  return run;
}
#endif
#include "ta_counter_block_impl.h"
/* R90: ROM-proven $84:8162 active prologue. No-work path remains R51.
 * Fast path exits at $84:8172, where R58 preserves its own independent
 * counter-write/RTL guards. The full original executor remains fallback. */
#if defined(TA_R90_ACTIVE_PROLOGUE) && TA_R90_ACTIVE_PROLOGUE
unsigned ta_native_r90_active_prologue(Cpu* c,unsigned budget){
 if(!c || !s_identical_rom || c->k!=0x84u || c->pc!=0x8162u ||
    c->db!=0x7eu || c->dp!=0 || c->e || c->mf || c->xf ||
    c->waiting || c->stopped || c->resetWanted || c->intWanted ||
    c->nmiWanted || (c->irqWanted&&!c->i) ||
    c->sp<0x100u || c->sp>0x1ffcu || c->sp==0x2a0u || c->sp==0x2a1u)return 0;
 Snes* s=(Snes*)c->mem;
 if(!s || s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
    s->dma->hdmaInitRequested || s->dma->hdmaRunRequested || s->irqCondition)return 0;
 const unsigned value=(unsigned)s->ram[0x2a0u]|((unsigned)s->ram[0x2a1u]<<8);
 if(value>1u)return 0; /* The existing R51 no-work branch owns >=2. */
 const unsigned ops=value?6u:4u;
 if(budget<ops)return 0;
 const unsigned fetch=s->fastMem?6u:8u;
 /* PHP F+6+WRAM8; REP 2F+6; LDA abs 3F+WRAM16;
    BEQ (2F+6 when taken), optional CMP #1 3F + second BEQ 2F+6. */
 const unsigned clocks=(value?13u:8u)*fetch+42u;
 const unsigned h=s->hPos,end=h+clocks;
 if((h&1u) || h>=1358u || end>ta_r41_busEndLimit[h] ||
    !ta_r49_noTimerEdge(s,h,end))return 0;
 const unsigned sp=c->sp;
 s->ram[sp]=ta_getFlags(c); /* PHP stack byte, before REP. */
 c->sp=(uint16_t)(sp-1u);
 c->mf=false;c->xf=false;c->a=(uint16_t)value;
 c->n=false;c->z=true;
 if(value)c->c=true; /* CMP #$0001 is reached iff value==1. */
 c->pc=0x8172u;
 s->openBus=value?0x03u:0x08u; /* Final taken BEQ displacement. */
 s->hPos=(uint16_t)end;s->cycles+=clocks;s->apuMasterPending+=clocks;
 if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
 return ops;
}
#endif

/* R91: byte-exact native executor for the ROM loop $80:A5E1..A5EC.
 * LDA $0000,Y / STA $0000,X / INY / INX / DEC $81 / BPL -12.
 * DB=$7E or $7F and DP=0 guarantee mapped WRAM for reads/writes. The
 * source/destination/counter may ALIAS: process bytes strictly in original
 * instruction order. Never substitute memcpy or precompute the counter.
 * Fence every WHOLE six-op iteration against raster, IRQ, DMA, hooks, and
 * the enclosing decoded opcode budget. R90's slower prologue stays OFF.
 */
#if defined(TA_R91_TRANSFER_LOOP) && TA_R91_TRANSFER_LOOP
unsigned ta_native_r91_transfer_loop(Cpu* c,unsigned budget){
 if(!c || !s_identical_rom || c->k!=0x80u || c->pc!=0xa5e1u ||
    (c->db!=0x7eu && c->db!=0x7fu) || c->dp || c->e || !c->mf || c->xf ||
    c->waiting || c->stopped || c->resetWanted || c->intWanted ||
    c->nmiWanted || (c->irqWanted&&!c->i) || budget<6u) return 0;
 Snes* s=(Snes*)c->mem;
 if(!s || s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
    s->dma->hdmaInitRequested || s->dma->hdmaRunRequested ||
    s->irqCondition) return 0;
 const unsigned bankBase=((unsigned)c->db&1u)<<16; /* $7E lower 64KB, $7F upper 64KB */
 const unsigned h0=s->hPos, fetch=s->fastMem?6u:8u;
 if((h0&1u) || h0>=1358u) return 0;
 const unsigned limit=ta_r41_busEndLimit[h0], maxClocks=12u*fetch+68u;
 if(limit<h0+maxClocks) return 0;
 /* Conservatively require space for a TAKEN branch even if final BPL isn't
  * taken. No writes are made before the first complete-iteration preflight. */
 unsigned h=h0,run=0;
 uint16_t x=c->x,y=c->y,a=c->a;
 while(run+6u<=budget && h+maxClocks<=limit &&
       ta_r49_noTimerEdge(s,h0,h+maxClocks)){
   const uint8_t byte=s->ram[bankBase+y];    /* LDA abs,Y, DBR bank */
   a=(uint16_t)((a&0xff00u)|byte);
   s->ram[bankBase+x]=byte;                  /* STA abs,X, DBR bank */
   y=(uint16_t)(y+1u);                      /* INY */
   x=(uint16_t)(x+1u);                      /* INX */
   const uint8_t count=(uint8_t)(s->ram[0x81u]-1u);
   s->ram[0x81u]=count;                     /* DEC $81 */
   const bool taken=(count&0x80u)==0;      /* BPL $A5E1 */
   h+=12u*fetch+(taken?68u:62u);
   run+=6u;
   c->n=(count&0x80u)!=0u;c->z=count==0;
   c->pc=taken?0xa5e1u:0xa5edu;
   if(!taken)break;
 }
 if(!run)return 0;
 c->a=a;c->x=x;c->y=y;
 s->openBus=0xf4u; /* branch displacement: final opcode bus read */
 const unsigned clocks=h-h0;
 s->hPos=(uint16_t)h;s->cycles+=clocks;s->apuMasterPending+=clocks;
 if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
 return run;
}
#endif

static void ta_checkInt(Cpu *cpu){
  cpu->intWanted=cpu->nmiWanted || (cpu->irqWanted && !cpu->i);
}
static uint8_t ta_fetch(Cpu *cpu){
  uint32_t adr=((uint32_t)cpu->k<<16)|cpu->pc;
  uint8_t value=ta_r45_readCode(cpu->mem,adr);
  cpu->pc=(uint16_t)(cpu->pc+1u);
  return value;
}
#if defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
/* Exact-ROM native execution of the $80:AEAD hot-loop's three previously
   interpreted memory operations. Keep each original CPU bus transaction and
   its interrupt-sampling position; do not collapse multiple SNES opcodes into
   a single opaque operation or optimize away external memory writes. */
static bool ta_r23Execute(Cpu *cpu,uint32_t site) {
  if(site!=0x80AEADu && site!=0x80AEB2u && site!=0x80AEB4u)return false;
  (void)ta_fetch(cpu); /* the SNES opcode fetch still consumes bus time */
  if(site==0x80AEADu) { /* BF: LDA abs-long,X: full 24-bit indexed address */
    uint32_t adr=(uint32_t)ta_fetch(cpu);
    adr|=(uint32_t)ta_fetch(cpu)<<8;
    adr|=(uint32_t)ta_fetch(cpu)<<16;
    adr=(adr+(uint32_t)cpu->x)&0xffffffu;
    if(cpu->mf) {
      ta_checkInt(cpu);
      cpu->a=(uint16_t)((cpu->a&0xff00u)|ta_r46_readData(cpu->mem,adr));
    } else {
      uint16_t value=ta_r46_readData(cpu->mem,adr);
      ta_checkInt(cpu);
      value|=(uint16_t)((uint16_t)ta_r46_readData(cpu->mem,(adr+1u)&0xffffffu)<<8);
      cpu->a=value;
    }
    ta_zn_a(cpu);TA_DIAG(++s_r23HotHits[0]);return true;
  }
  /* 65 ADC dp and 85 STA dp: operand addressing always wraps within bank 00.
     Unaligned direct-page registers incur one exact SNES idle clock. */
  uint32_t adr=(uint32_t)ta_fetch(cpu);
  if(cpu->dp&0xffu)snes_cpuIdle(cpu->mem,false);
  uint32_t lo=((uint32_t)cpu->dp+adr)&0xffffu;
  uint32_t hi=((uint32_t)cpu->dp+adr+1u)&0xffffu;
  if(site==0x80AEB4u) { /* 85: STA dp (no Z/N changes) */
    if(cpu->mf) {
      ta_checkInt(cpu);
      ta_r46_writeData(cpu->mem,lo,(uint8_t)cpu->a);
    } else {
      ta_r46_writeData(cpu->mem,lo,(uint8_t)cpu->a);
      ta_checkInt(cpu);
      ta_r46_writeData(cpu->mem,hi,(uint8_t)(cpu->a>>8));
    }
    TA_DIAG(++s_r23HotHits[2]);return true;
  }
  /* 65: ADC dp. Mirrors the reference's 8/16-bit decimal and binary paths. */
  if(cpu->mf) {
    ta_checkInt(cpu);
    uint8_t value=ta_r46_readData(cpu->mem,lo);
    int result=0;
    if(cpu->d) {
      result=(cpu->a&15)+(value&15)+cpu->c;
      if(result>9)result=((result+6)&15)+16;
      result=(cpu->a&0xf0)+(value&0xf0)+result;
    }else result=(cpu->a&255)+value+cpu->c;
    cpu->v=(cpu->a&0x80)==(value&0x80) && (value&0x80)!=(result&0x80);
    if(cpu->d && result>0x9f)result+=0x60;
    cpu->c=result>255;
    cpu->a=(uint16_t)((cpu->a&0xff00u)|(result&255));
  } else {
    uint16_t value=ta_r46_readData(cpu->mem,lo);
    ta_checkInt(cpu);
    value|=(uint16_t)((uint16_t)ta_r46_readData(cpu->mem,hi)<<8);
    int result=0;
    if(cpu->d) {
      result=(cpu->a&15)+(value&15)+cpu->c;
      if(result>9)result=((result+6)&15)+16;
      result=(cpu->a&0xf0)+(value&0xf0)+result;
      if(result>0x9f)result=((result+0x60)&255)+256;
      result=(cpu->a&0xf00)+(value&0xf00)+result;
      if(result>0x9ff)result=((result+0x600)&4095)+4096;
      result=(cpu->a&0xf000)+(value&0xf000)+result;
    }else result=cpu->a+value+cpu->c;
    cpu->v=(cpu->a&0x8000)==(value&0x8000) && (value&0x8000)!=(result&0x8000);
    if(cpu->d && result>0x9fff)result+=0x6000;
    cpu->c=result>65535;
    cpu->a=(uint16_t)result;
  }
  ta_zn_a(cpu);TA_DIAG(++s_r23HotHits[1]);return true;
}
#endif

#if defined(TA_NATIVE_ENABLED) && TA_NATIVE_ENABLED
static inline __attribute__((always_inline)) bool ta_executeClass(Cpu* cpu,unsigned cls) {
  (void)ta_fetch(cpu); /* original opcode: consumes one bus clock */
  if(cls==28u) { /* R22 JSL long: word fetch, PB push, idle,
                     destination bank fetch, then 16-bit return push. */
    const uint16_t low=ta_fetch(cpu);
    const uint16_t dest=(uint16_t)(low | ((uint16_t)ta_fetch(cpu)<<8));
    ta_r46_writeData(cpu->mem,cpu->sp,cpu->k);
    cpu->sp=(uint16_t)(cpu->sp-1u);
    if(cpu->e)cpu->sp=(uint16_t)((cpu->sp&255u)|256u);
    snes_cpuIdle(cpu->mem,false);
    const uint8_t destBank=ta_fetch(cpu);
    const uint16_t ret=(uint16_t)(cpu->pc-1u);
    ta_r46_writeData(cpu->mem,cpu->sp,(uint8_t)(ret>>8));
    cpu->sp=(uint16_t)(cpu->sp-1u);
    if(cpu->e)cpu->sp=(uint16_t)((cpu->sp&255u)|256u);
    ta_checkInt(cpu); /* cpu_pushWord(...,true): BETWEEN stack bytes */
    ta_r46_writeData(cpu->mem,cpu->sp,(uint8_t)ret);
    cpu->sp=(uint16_t)(cpu->sp-1u);
    if(cpu->e)cpu->sp=(uint16_t)((cpu->sp&255u)|256u);
    cpu->k=destBank;
    cpu->pc=dest;
  } else if(cls==29u) { /* R22 JMP abs: interrupt sample between bytes */
    const uint16_t lo=ta_fetch(cpu);
    ta_checkInt(cpu);
    cpu->pc=(uint16_t)(lo | ((uint16_t)ta_fetch(cpu)<<8));
  } else if(cls==30u) { /* R22 PEA: push immediate 16-bit address */
    const uint16_t lo=ta_fetch(cpu);
    const uint16_t word=(uint16_t)(lo|((uint16_t)ta_fetch(cpu)<<8));
    ta_r46_writeData(cpu->mem,cpu->sp,(uint8_t)(word>>8));
    cpu->sp=(uint16_t)(cpu->sp-1u);
    if(cpu->e)cpu->sp=(uint16_t)((cpu->sp&255u)|256u);
    ta_checkInt(cpu);
    ta_r46_writeData(cpu->mem,cpu->sp,(uint8_t)word);
    cpu->sp=(uint16_t)(cpu->sp-1u);
    if(cpu->e)cpu->sp=(uint16_t)((cpu->sp&255u)|256u);
  } else if(cls==26u || cls==27u) { /* R20: CMP abs or LDA long, exact bus sequence */
    uint32_t addr=(uint32_t)ta_fetch(cpu);
    addr|=(uint32_t)ta_fetch(cpu)<<8;
    if(cls==26u) addr|=(uint32_t)cpu->db<<16;
    else addr|=(uint32_t)ta_fetch(cpu)<<16;
    if(cls==26u) {
      uint32_t value;
      if(cpu->mf) {
        ta_checkInt(cpu);
        value=(uint32_t)ta_r46_readData(cpu->mem,addr);
      } else {
        value=(uint32_t)ta_r46_readData(cpu->mem,addr);
        ta_checkInt(cpu);
        value|=(uint32_t)ta_r46_readData(cpu->mem,(addr+1u)&0xffffffu)<<8;
      }
      const uint32_t mask=cpu->mf?0xffu:0xffffu;
      uint32_t result=((uint32_t)(cpu->a&mask) + ((~value)&mask)+1u);
      cpu->c=result>mask;
      cpu->z=(result&mask)==0u;
      cpu->n=(result&(cpu->mf?0x80u:0x8000u))!=0u;
    } else { /* LDA long; full 24-bit absolute address with wrap */
      if(cpu->mf) {
        ta_checkInt(cpu);
        cpu->a=(uint16_t)((cpu->a&0xff00u)|ta_r46_readData(cpu->mem,addr));
      } else {
        uint16_t value=ta_r46_readData(cpu->mem,addr);
        ta_checkInt(cpu);
        value|=(uint16_t)((uint16_t)ta_r46_readData(cpu->mem,(addr+1u)&0xffffffu)<<8);
        cpu->a=value;
      }
      ta_zn_a(cpu);
    }
  } else   if(cls==20u) { /* LDA abs: fetch 16-bit address from code bank, data DBR. */
    uint16_t offset=(uint16_t)ta_fetch(cpu);
    offset|=(uint16_t)((uint16_t)ta_fetch(cpu)<<8);
    uint32_t addr=((uint32_t)cpu->db<<16)|offset;
    if(cpu->mf) {
      ta_checkInt(cpu);
      cpu->a=(uint16_t)((cpu->a&0xff00u)|ta_r46_readData(cpu->mem,addr));
    } else {
      uint16_t value=ta_r46_readData(cpu->mem,addr);
      ta_checkInt(cpu);
      value|=(uint16_t)((uint16_t)ta_r46_readData(cpu->mem,(addr+1u)&0xffffffu)<<8);
      cpu->a=value;
    }
    ta_zn_a(cpu);
  } else if(cls==21u || cls==22u) { /* AND/CMP immediate, M flag width */
    uint16_t immediate;
    if(cpu->mf) {
      ta_checkInt(cpu);
      immediate=ta_fetch(cpu);
    } else {
      immediate=ta_fetch(cpu);
      ta_checkInt(cpu);
      immediate|=(uint16_t)((uint16_t)ta_fetch(cpu)<<8);
    }
    if(cls==21u) {
      cpu->a=cpu->mf
        ? (uint16_t)((cpu->a&0xff00u)|((cpu->a&immediate)&0xffu))
        : (uint16_t)(cpu->a&immediate);
      ta_zn_a(cpu);
    } else {
      const unsigned mask=cpu->mf?255u:65535u;
      const unsigned result=((unsigned)(cpu->a&mask)+(unsigned)((~immediate)&mask)+1u);
      cpu->c=result>mask;
      cpu->z=(result&mask)==0u;
      cpu->n=(result&(cpu->mf?128u:32768u))!=0u;
    }
  } else if(cls==23u) { /* PHB: idle, IRQ sample, push data bank */
    snes_cpuIdle(cpu->mem,false);
    ta_checkInt(cpu);
    ta_r46_writeData(cpu->mem,cpu->sp,cpu->db);
    cpu->sp=(uint16_t)(cpu->sp-1u);
    if(cpu->e)cpu->sp=(uint16_t)((cpu->sp&255u)|256u);
  } else if(cls==25u) { /* BRA rel: unconditional branch and taken idle */
    int8_t relative=(int8_t)ta_fetch(cpu);
    ta_checkInt(cpu);
    snes_cpuIdle(cpu->mem,false);
    cpu->pc=(uint16_t)(cpu->pc+(int)relative);
  } else if(cls==9u || cls==13u){ /* REP/SEP imm(s): read operand, check IRQ,
                               update flags, then idle. */
    uint8_t operand=ta_fetch(cpu);
    ta_checkInt(cpu);
    uint8_t flags=ta_getFlags(cpu);
    ta_setFlags(cpu,cls==9u?(uint8_t)(flags&~operand):(uint8_t)(flags|operand));
    snes_cpuIdle(cpu->mem,false);
  } else if(cls==14u || cls==15u){ /* BNE / BEQ: 2-byte branch with conditional idle */
    bool taken=cls==14u ? !cpu->z : cpu->z;
    if(!taken)ta_checkInt(cpu);
    int8_t displacement=(int8_t)ta_fetch(cpu);
    if(taken){
      ta_checkInt(cpu);
      snes_cpuIdle(cpu->mem,false);
      cpu->pc=(uint16_t)(cpu->pc+(int)displacement);
    }
  } else if(cls==16u){ /* PHP: idle, interrupt checkpoint, stack push */
    snes_cpuIdle(cpu->mem,false);
    ta_checkInt(cpu);
    ta_r46_writeData(cpu->mem,cpu->sp,ta_getFlags(cpu));
    cpu->sp=(uint16_t)(cpu->sp-1u);
    if(cpu->e)cpu->sp=(uint16_t)((cpu->sp&0xffu)|0x100u);
  } else if(cls==17u || cls==18u){ /* PLP / PLB: two idle clocks, read stack */
    snes_cpuIdle(cpu->mem,false);
    snes_cpuIdle(cpu->mem,false);
    ta_checkInt(cpu);
    cpu->sp=(uint16_t)(cpu->sp+1u);
    if(cpu->e)cpu->sp=(uint16_t)((cpu->sp&0xffu)|0x100u);
    uint8_t value=ta_r46_readData(cpu->mem,cpu->sp);
    if(cls==17u)ta_setFlags(cpu,value);
    else {cpu->db=value;cpu->z=(value==0u);cpu->n=((value&0x80u)!=0u);}
  } else if(cls==19u){ /* RTL: two idle clocks, pull 16-bit PC, check IRQ, pull bank */
    snes_cpuIdle(cpu->mem,false);
    snes_cpuIdle(cpu->mem,false);
    cpu->sp=(uint16_t)(cpu->sp+1u);
    if(cpu->e)cpu->sp=(uint16_t)((cpu->sp&0xffu)|0x100u);
    uint16_t value=ta_r46_readData(cpu->mem,cpu->sp);
    cpu->sp=(uint16_t)(cpu->sp+1u);
    if(cpu->e)cpu->sp=(uint16_t)((cpu->sp&0xffu)|0x100u);
    value|=(uint16_t)((uint16_t)ta_r46_readData(cpu->mem,cpu->sp)<<8);
    cpu->pc=(uint16_t)(value+1u);
    ta_checkInt(cpu);
    cpu->sp=(uint16_t)(cpu->sp+1u);
    if(cpu->e)cpu->sp=(uint16_t)((cpu->sp&0xffu)|0x100u);
    cpu->k=ta_r46_readData(cpu->mem,cpu->sp);
  } else if(cls==10u){ /* BIT #imm(m): flags after immediate operand */
    uint16_t operand;
    if(cpu->mf){
      ta_checkInt(cpu);
      operand=ta_fetch(cpu);
      cpu->z=((cpu->a&255u)&operand)==0u;
    } else {
      operand=ta_fetch(cpu);
      ta_checkInt(cpu);
      operand|=(uint16_t)((uint16_t)ta_fetch(cpu)<<8);
      cpu->z=(cpu->a&operand)==0u;
    }
  } else {
    /* 65C816 2-cycle implied addressing. Detect the IRQ/NMI checkpoint
       BEFORE the final idle/read cycle, just like cpu_adrImp(). */
    ta_checkInt(cpu);
    if(cpu->intWanted)
      (void)ta_r46_readData(cpu->mem,((uint32_t)cpu->k<<16)|cpu->pc);
    else snes_cpuIdle(cpu->mem,false);
    switch(cls){
      case 31:break; /* exact-ROM NOP; implied bus and IRQ work above */
      case 0:cpu->x=cpu->xf?(uint16_t)((cpu->x+1u)&255u):(uint16_t)(cpu->x+1u);ta_zn_x(cpu);break;
      case 1:cpu->y=cpu->xf?(uint16_t)((cpu->y+1u)&255u):(uint16_t)(cpu->y+1u);ta_zn_y(cpu);break;
      case 2:cpu->x=cpu->xf?(uint16_t)((cpu->x-1u)&255u):(uint16_t)(cpu->x-1u);ta_zn_x(cpu);break;
      case 3:cpu->c=false;break;
      case 4:cpu->y=cpu->xf?(uint16_t)((cpu->y-1u)&255u):(uint16_t)(cpu->y-1u);ta_zn_y(cpu);break;
      case 5:cpu->c=true;break;
      case 6:cpu->x=cpu->xf?(uint16_t)(cpu->a&255u):cpu->a;ta_zn_x(cpu);break;
      case 7:cpu->y=cpu->xf?(uint16_t)(cpu->a&255u):cpu->a;ta_zn_y(cpu);break;
      case 8:cpu->y=cpu->xf?(uint16_t)(cpu->x&255u):cpu->x;ta_zn_y(cpu);break;
      case 11:cpu->a=cpu->mf?(uint16_t)((cpu->a&0xff00u)|(cpu->x&255u)):cpu->x;ta_zn_a(cpu);break;
      case 12:cpu->a=cpu->mf?(uint16_t)((cpu->a&0xff00u)|(cpu->y&255u)):cpu->y;ta_zn_a(cpu);break;
      case 24: /* ASL A: two-cycle accumulator shift with exact M-width carry */
        if(cpu->mf) {
          cpu->c=(cpu->a&0x80u)!=0;
          cpu->a=(uint16_t)((cpu->a&0xff00u)|((cpu->a<<1)&255u));
        } else {
          cpu->c=(cpu->a&0x8000u)!=0;
          cpu->a=(uint16_t)(cpu->a<<1);
        }
        ta_zn_a(cpu);break;
      default:return false;
    }
  }
  TA_DIAG(++s_opcode_hits[cls]);return true;
}
#endif

bool ta_native_try_execute(Cpu *cpu){
#if defined(TA_NATIVE_ENABLED) && TA_NATIVE_ENABLED
  unsigned cls;
  if(!s_identical_rom||!cpu||cpu->waiting||cpu->stopped||
     cpu->resetWanted||cpu->intWanted)return false;
  uint32_t adr=((uint32_t)cpu->k<<16)|cpu->pc;
#if defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
  if(ta_r23Execute(cpu,adr))return true;
#endif
#if defined(TA_R32_NATIVE_DELAY_LOOP) && TA_R32_NATIVE_DELAY_LOOP
  if(adr>=0x83BF52u && adr<=0x83BF57u) {
    cls=adr<0x83BF56u ? 31u : (adr==0x83BF56u ? 0u : 14u);
  } else
#endif
  {
#if defined(TA_R15_NATIVE_LOOKUP) && TA_R15_NATIVE_LOOKUP
  /* Bank guard and high-half LoROM guard prevent aliasing into the table. */
  const TaNativeMapWord *row=s_nativeOpcodeBank[(uint8_t)(adr>>16)];
  if(!row || !(adr&0x8000u)) return false;
  TaNativeMapWord entry=row[adr&0x7fffu];
  if(!(entry&TA_MAP_CLASS_MASK)) return false;
  cls=(unsigned)(entry&TA_MAP_CLASS_MASK)-1u;
#else
  switch(adr){
#if defined(TA_R20_NATIVE_MEMORY_OPS) && TA_R20_NATIVE_MEMORY_OPS
    case 0x80942Bu: case 0x8094D5u: case 0x8094DAu: case 0x8094C4u: case 0x809368u: case 0x80936Du:
    case 0x80938Eu: case 0x809393u: case 0x8093B4u: case 0x8093B9u: case 0x8093DAu: case 0x8093DFu:
    case 0x809470u: case 0x80943Bu: case 0x8094C9u: case 0x80805Eu:
      cls=26u; break; /* R20 CMP absolute */
    case 0x809089u: case 0x8AC0ABu: case 0x8AC0C0u: case 0x8AC0D2u: case 0x8AC939u: case 0x80A0CDu:
    case 0x80A10Cu: case 0x8480E0u: case 0x80A082u: case 0x80A0BAu: case 0x8090F6u: case 0x86D93Au:
    case 0x86D9D8u: case 0x80AC3Au: case 0x80AEBBu: case 0x82A32Eu: case 0x82A334u: case 0x86D8FAu:
      cls=27u; break; /* R20 LDA absolute long */
#endif
    case 0x80807Bu: case 0x808120u: case 0x808121u: case 0x808131u: case 0x808132u:
    case 0x809F5Eu: case 0x809F5Fu: case 0x80A5A7u: case 0x80A5E8u: case 0x80BC47u:
    case 0x80BC48u: case 0x80BC49u: case 0x80BC4Au: case 0x80C1EDu: case 0x80C1EEu:
    case 0x80C1EFu: case 0x80C1F0u:
      cls=0u;break; /* INX */
    case 0x809494u: case 0x8094F3u: case 0x8094F4u: case 0x80A595u: case 0x80A5AEu:
    case 0x80A5CFu: case 0x80A5E7u: case 0x80BBC0u: case 0x80BBC1u: case 0x80BBE0u:
    case 0x80BBE1u: case 0x80BC42u: case 0x80BC43u: case 0x80BC44u: case 0x80BC45u:
    case 0x80BC46u: case 0x80C09Eu: case 0x80C09Fu: case 0x80C0BEu: case 0x80C0BFu:
    case 0x80C1E8u: case 0x80C1E9u: case 0x80C1EAu: case 0x80C1EBu: case 0x80C1ECu:
    case 0x8AD3C6u: case 0x8AD3C7u: case 0x8AD493u: case 0x8AD494u:
      cls=1u;break; /* INY */
    case 0x809438u: case 0x80AEB6u: case 0x84903Bu: case 0x84903Cu: case 0x8A8891u:
    case 0x8A88E9u: case 0x8A8AC3u: case 0x8AC0E7u:
      cls=2u;break; /* DEX */
    case 0x80AEB1u: case 0x80BBB6u: case 0x80BBBAu: case 0x80BBD6u: case 0x80BBEFu:
    case 0x80BC0Au: case 0x80BC36u: case 0x80C094u: case 0x80C098u: case 0x80C0B4u:
    case 0x80C0E0u: case 0x80C10Eu: case 0x80C16Au: case 0x80C170u: case 0x80C1B2u:
    case 0x80C1B8u: case 0x80C1CFu: case 0x8AC0D6u: case 0x8AC0E0u:
      cls=3u;break; /* CLC */
    case 0x809F60u: case 0x809F61u:
      cls=4u;break; /* DEY */
    case 0x80A5DBu: case 0x80C0D7u: case 0x80C105u: case 0x84918Fu:
      cls=5u;break; /* SEC */
    case 0x8094F1u: case 0x80BBBFu: case 0x80C09Du: case 0x8AC0AFu: case 0x8AC0C4u:
    case 0x8AC93Du: case 0x8AD31Eu: case 0x8AD3EBu:
      cls=6u;break; /* TAX */
    case 0x80A5DEu: case 0x80BBB9u: case 0x80C097u: case 0x849193u:
      cls=7u;break; /* TAY */
    case 0x80BBDFu: case 0x80C0BDu:
      cls=8u;break; /* TXY */
    case 0x809087u: case 0x809431u: case 0x80A5D6u: case 0x80BBB0u: case 0x80BBCAu:
    case 0x80C08Eu: case 0x80C0A8u: case 0x848163u: case 0x84C134u: case 0x89A367u:
    case 0x89A380u: case 0x89A393u: case 0x89A3A7u:
      cls=9u;break; /* REP */
    case 0x80908Du: case 0x809092u: case 0x80BBF5u: case 0x80BC10u: case 0x80C0CFu:
    case 0x80C0E6u: case 0x80C0FDu: case 0x80C114u: case 0x80C13Bu: case 0x80C15Du:
    case 0x80C183u: case 0x80C1A5u: case 0x84C13Du: case 0x84C169u: case 0x89A373u:
      cls=10u;break; /* BIT */
    case 0x80A5DAu:
      cls=11u;break; /* TXA */
    case 0x84918Eu:
      cls=12u;break; /* TYA */
    case 0x809436u: case 0x80A5DFu: case 0x80BBC2u: case 0x80C0A0u: case 0x89A378u:
    case 0x89A389u: case 0x89A39Cu:
      cls=13u;break; /* SEP */
    case 0x80807Fu: case 0x808122u: case 0x808133u: case 0x809090u: case 0x809095u:
    case 0x80942Eu: case 0x809439u: case 0x80947Cu: case 0x809495u: case 0x8094D8u:
    case 0x8094DDu: case 0x8094F5u: case 0x809CD3u: case 0x809F62u: case 0x80A596u:
    case 0x80A5AFu: case 0x80A5D0u: case 0x80AEB7u: case 0x80BC4Du: case 0x80C160u:
    case 0x80C1A8u: case 0x8481A6u: case 0x8481BAu: case 0x8A88EAu: case 0x8A8AC4u:
    case 0x8AC0E8u: case 0x8AD322u: case 0x8AD344u: case 0x8AD385u: case 0x8AD3EFu:
    case 0x8AD411u: case 0x8AD452u:
      cls=14u;break; /* BNE */
    case 0x80806Fu: case 0x808074u: case 0x808079u: case 0x8094ECu: case 0x809DADu:
    case 0x80A59Eu: case 0x80A5B7u: case 0x80A5F0u: case 0x80BBF8u: case 0x80BC13u:
    case 0x80C0D2u: case 0x80C0E9u: case 0x80C100u: case 0x80C117u: case 0x80C13Eu:
    case 0x80C186u: case 0x80C1F3u: case 0x848168u: case 0x84816Du: case 0x848179u:
    case 0x84C140u: case 0x84C16Cu: case 0x89A376u: case 0x89A37Eu: case 0x89A3A2u:
    case 0x8AC0B7u: case 0x8AC0CCu: case 0x8AC0F0u: case 0x8AD37Du: case 0x8AD3CDu:
    case 0x8AD44Au: case 0x8AD49Au:
      cls=15u;break; /* BEQ */
    case 0x809086u: case 0x8090F3u: case 0x8091B2u: case 0x809357u: case 0x809B1Fu:
    case 0x809BDEu: case 0x809C04u: case 0x80A07Fu: case 0x80A0CAu: case 0x80A6FBu:
    case 0x80A700u: case 0x80AE15u: case 0x80AE92u: case 0x80B636u: case 0x80BBAFu:
    case 0x80C08Du: case 0x80C6D6u: case 0x8480DCu: case 0x848162u: case 0x8481D6u:
    case 0x848C5Fu: case 0x848F1Eu: case 0x848FC7u: case 0x84908Bu: case 0x84C12Du:
    case 0x899E69u: case 0x89A325u: case 0x89A366u: case 0x89A395u: case 0x89A3F5u:
    case 0x89A431u: case 0x8ABE03u:
      cls=16u;break; /* PHP */
    case 0x809097u: case 0x8090FFu: case 0x8091C5u: case 0x8093FEu: case 0x809BDCu:
    case 0x809C02u: case 0x809C80u: case 0x80A0C8u: case 0x80A11Au: case 0x80A724u:
    case 0x80A725u: case 0x80AE25u: case 0x80AEC9u: case 0x80BC53u: case 0x80C1FCu:
    case 0x80CA30u: case 0x8480FBu: case 0x8481CBu: case 0x8481F9u: case 0x848CA8u:
    case 0x848F65u: case 0x849041u: case 0x84909Fu: case 0x84C193u: case 0x89A1B6u:
    case 0x89A364u: case 0x89A39Au: case 0x89A3ACu: case 0x89A3FAu: case 0x89A40Eu:
    case 0x89A436u: case 0x8ABE8Fu:
      cls=17u;break; /* PLP */
    case 0x808E2Du: case 0x808E9Fu: case 0x8091B5u: case 0x8091C4u: case 0x80935Au:
    case 0x8093FDu: case 0x80A6FDu: case 0x80A8FFu: case 0x80A976u: case 0x80AE95u:
    case 0x80AEC8u: case 0x80BBC9u: case 0x80BC52u: case 0x80C0A7u: case 0x80C1FBu:
    case 0x80C6D9u: case 0x80CA2Fu: case 0x8481DCu: case 0x8481F7u: case 0x84C132u:
    case 0x84C133u: case 0x84C192u: case 0x89A36Du: case 0x89A392u: case 0x89A39Bu:
    case 0x89A3ABu: case 0x8ABE05u: case 0x8AC1DDu: case 0x8AC21Cu: case 0x8AD309u:
    case 0x8AD30Au: case 0x8AD314u:
      cls=18u;break; /* PLB */
    case 0x809098u: case 0x809101u: case 0x8091C6u: case 0x8093FFu: case 0x8095AFu:
    case 0x8096AAu: case 0x809BDDu: case 0x809C03u: case 0x809C81u: case 0x809DB6u:
    case 0x80A0C9u: case 0x80A11Bu: case 0x80A144u: case 0x80A726u: case 0x80A7C2u:
    case 0x80BC54u: case 0x80C1FDu: case 0x80CA31u: case 0x82A353u: case 0x8480FCu:
    case 0x8481CCu: case 0x8481FAu: case 0x84C194u: case 0x86D972u: case 0x86D9CDu:
    case 0x86D9F0u: case 0x89A3ADu: case 0x8ABE90u: case 0x8AC0EBu: case 0x8AC21Du:
    case 0x8AC96Du: case 0x8AD315u:
      cls=19u;break; /* RTL */
    case 0x808E34u: case 0x809479u: case 0x809B9Au: case 0x809BA0u: case 0x809BA6u:
    case 0x809BACu: case 0x809BB2u: case 0x809BB8u: case 0x809BBEu: case 0x809BC4u:
    case 0x809BCAu: case 0x809BD0u: case 0x809BD6u: case 0x809C07u: case 0x809C10u:
    case 0x809C40u: case 0x809C50u: case 0x809D94u: case 0x80A138u: case 0x80A814u:
    case 0x848165u: case 0x8481DDu: case 0x84C13Au: case 0x84C166u:
      cls=20u;break; /* LDA_ABS */
    case 0x808069u: case 0x8094E0u: case 0x809C0Au: case 0x80A5CAu: case 0x80A5FCu:
    case 0x80A79Du: case 0x80BBCFu: case 0x80C0ADu: case 0x80C162u: case 0x80C1AAu:
    case 0x80C1CCu: case 0x80C1D2u: case 0x80C1DAu: case 0x8480E5u: case 0x8480F2u:
    case 0x848176u: case 0x8481E1u: case 0x8481F4u: case 0x849175u: case 0x8ABD9Eu:
    case 0x8AC0B4u: case 0x8AC0C9u: case 0x8AC954u: case 0x8AC95Du:
      cls=21u;break; /* AND_IMM */
    case 0x80806Cu: case 0x808071u: case 0x808076u: case 0x80A5F8u: case 0x80A7B1u:
    case 0x80A7EAu: case 0x80BBDAu: case 0x80C0B8u: case 0x80C174u: case 0x80C1BCu:
    case 0x82A33Du: case 0x84816Au: case 0x848E05u: case 0x89A1C1u: case 0x89A3C2u:
    case 0x8ABE13u: case 0x8AC14Du: case 0x8AC1A9u: case 0x8AD37Au: case 0x8AD382u:
    case 0x8AD447u: case 0x8AD44Fu: case 0x8AD514u: case 0x8AD51Cu:
      cls=22u;break; /* CMP_IMM */
    case 0x808E27u: case 0x8091B3u: case 0x809358u: case 0x80A8FDu: case 0x80AE93u:
    case 0x80BBB2u: case 0x80C090u: case 0x80C6D7u: case 0x8481DAu: case 0x848C62u:
    case 0x848F21u: case 0x848FCAu: case 0x84908Eu: case 0x84C12Eu: case 0x899E6Au:
    case 0x89A369u: case 0x89A382u: case 0x89A3EAu: case 0x8ABDA5u: case 0x8ABE65u:
    case 0x8ABE7Au: case 0x8AC087u: case 0x8AC1D5u: case 0x8AD305u:
      cls=23u;break; /* PHB */
    case 0x8095A4u: case 0x80A5BDu: case 0x80A612u: case 0x80A706u: case 0x80BBB5u:
    case 0x80BBD4u: case 0x80BBD5u: case 0x80C093u: case 0x80C0B2u: case 0x80C0B3u:
    case 0x848C98u: case 0x848CF9u: case 0x848D3Fu: case 0x849178u: case 0x89A1C9u:
    case 0x8ABD79u: case 0x8ABD7Au: case 0x8ABD7Bu: case 0x8ABE0Bu: case 0x8AC0F7u:
    case 0x8AC101u: case 0x8AD31Du: case 0x8AD3EAu: case 0x8AD4B7u:
      cls=24u;break; /* ASLA */
    case 0x8091D7u: case 0x8091F1u: case 0x80920Bu: case 0x809225u: case 0x80924Eu:
    case 0x80A61Au: case 0x80BBE5u: case 0x80C0C3u: case 0x80C0DBu: case 0x80C109u:
    case 0x80C137u: case 0x80C165u: case 0x80C17Fu: case 0x80C1ADu: case 0x80C1C7u:
    case 0x8480F5u: case 0x848F90u: case 0x899EF0u: case 0x89BEC1u: case 0x89BECEu:
    case 0x8ABDB0u: case 0x8ABE70u: case 0x8ABE85u: case 0x8AC16Au:
      cls=25u;break; /* BRA */
    default:return false;
  }
#endif
  }
  return ta_executeClass(cpu,cls);
#else
  (void)cpu;return false;
#endif
}

#if defined(TA_R40_DECODED_DISPATCH) && TA_R40_DECODED_DISPATCH && defined(TA_R15_NATIVE_LOOKUP) && TA_R15_NATIVE_LOOKUP
uint16_t ta_native_r40_descriptor(uint32_t pc) {
  if(!s_identical_rom)return 0;
#if defined(TA_R32_NATIVE_DELAY_LOOP) && TA_R32_NATIVE_DELAY_LOOP
  if(pc>=0x83BF52u && pc<=0x83BF57u) {
    unsigned cls=pc<0x83BF56u?32u:pc==0x83BF56u?1u:15u;
    return (uint16_t)(cls|(pc<0x83BF57u?64u:512u));
  }
#endif
  const TaNativeMapWord* row=s_nativeOpcodeBank[(uint8_t)(pc>>16)];
  return row && (pc&0x8000u)?row[pc&0x7fffu]:0u;
}
#include "ta_r108_mode2_records.h"
#include "ta_r126_prefix.h"
bool ta_native_run_decoded_block(Cpu* cpu,bool expectedVblank,uint32_t expectedFrame) {
#if defined(TA_NATIVE_ENABLED) && TA_NATIVE_ENABLED && defined(TA_R10_MICROBLOCKS) && TA_R10_MICROBLOCKS
  if(!cpu)return false;
  Snes *s=(Snes*)cpu->mem;
  if(cpu && s && !cpu->waiting && !cpu->stopped && !cpu->resetWanted && !cpu->intWanted) {
    unsigned run=0;
    uint32_t expectedNext=0;
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
    uint32_t prior=0;
#if defined(TA_R24_LOOP_BACKEDGE) && TA_R24_LOOP_BACKEDGE
    while(run<48) {
#else
    while(run<24) {
#endif
#else
    while(run<12) {
#endif
      if(s->inVblank!=expectedVblank || s->frames!=expectedFrame ||
         cpu->waiting || cpu->stopped || cpu->resetWanted || cpu->intWanted)break;
      uint32_t pc=((uint32_t)cpu->k<<16)|cpu->pc;
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
      bool dynamicLink=run && pc!=expectedNext;
      if(dynamicLink && !ta_native_r21_allow_branch_next(prior,pc))break;
#else
      if(run && pc!=expectedNext)break;
#endif
#if TA_R48_SERVICE_ON && !TA_R52_LOOP_ON
      if(pc==0x809086u && ta_native_r48_quiet_service(cpu,48u-run))
        return true;
#endif
      const uint16_t descriptor=ta_native_r40_descriptor(pc);
#if TA_R58_TASK_ON && !TA_R58_IDLE_ON
      if((descriptor&8192u) && ta_native_r58_task_prefix(cpu,48u-run))return true;
#endif
#if TA_R52_LOOP_ON
      /* One descriptor gate replaces R48's address comparison per opcode. */
#if TA_R62_SPRITE_ON
      if(descriptor&64512u) {
#elif TA_R58_IDLE_ON || TA_R58_COUNTER_ON
      if(descriptor&31744u) {
#else
      if(descriptor&7168u) {
#endif
#if TA_R62_SPRITE_ON
        if(descriptor&32768u) {
#if defined(TA_R108_MODE2_RECORDS) && TA_R108_MODE2_RECORDS
          if(cpu->k==0x88){
#if defined(TA_R114_R108_DIAG) && TA_R114_R108_DIAG
            ++s_r114_r108_attempts;
            const unsigned r114_r108_ops=ta_native_r108_records(cpu,48u-run);
            if(r114_r108_ops){++s_r114_r108_accepts;s_r114_r108_guest_ops+=r114_r108_ops;return true;}
#else
            if(ta_native_r108_records(cpu,48u-run))return true;
#if defined(TA_R126_CPU_PREFIX) && TA_R126_CPU_PREFIX
            if(ta_native_r126_prefix(cpu,48u-run))return true;
#endif
#endif
          }
          else
#endif
#if defined(TA_R92_FAST_MAP_ROUTING) && TA_R92_FAST_MAP_ROUTING && defined(TA_R91_TRANSFER_LOOP) && TA_R91_TRANSFER_LOOP
          if(pc==0x80a5e1u && TA_R103_ACCEPT(TA_R103_R91_TRANSFER,ta_native_r91_transfer_loop(cpu,48u-run)))return true;
#endif
#if TA_R81_ANIMATION_ON
          if(cpu->k==0x89){if(ta_r81_can_start(cpu,48u-run)){if(TA_R103_ACCEPT(TA_R103_R81_ANIMATION,ta_native_r81_animation(cpu,48u-run)))return true;}else TA_R103_PREFILTER_SKIP(TA_R103_R81_ANIMATION);}
          else
#endif
#if TA_R80_SPRITE_ON
          if(cpu->k==0x80 && cpu->pc>=0xc0c8 && cpu->pc<=0xc1e8){if(ta_r80_can_start(cpu,48u-run)){if(TA_R103_ACCEPT(TA_R103_R80_SPRITE,ta_native_r80_sprite(cpu,48u-run)))return true;}else TA_R103_PREFILTER_SKIP(TA_R103_R80_SPRITE);}
          else
#endif
#if TA_R78_QUIET_ON
          if(cpu->k==0x86){if(ta_r78_can_start(cpu,48u-run)){if(TA_R103_ACCEPT(TA_R103_R78_QUIET,ta_native_r78_quiet(cpu,48u-run)))return true;}else TA_R103_PREFILTER_SKIP(TA_R103_R78_QUIET);}
          else
#endif
#if TA_R74_FIELDS_ON
          if(cpu->k==0x82){if(ta_r74_can_start(cpu,48u-run)){if(TA_R103_ACCEPT(TA_R103_R74_FIELDS,ta_native_r74_fields(cpu,48u-run)))return true;}else TA_R103_PREFILTER_SKIP(TA_R103_R74_FIELDS);}
          else
#endif
#if TA_R73_CHAIN_ON
          if(cpu->pc>=0xbbea && cpu->pc<=
#if defined(TA_R101_SPRITE_RESUME) && TA_R101_SPRITE_RESUME
            0xbde0
#else
            0xbdd6
#endif
          ){if(ta_r73_can_start(cpu,48u-run)){if(TA_R103_ACCEPT(TA_R103_R73_SPRITE,ta_native_r73_chain(cpu,48u-run)))return true;}else TA_R103_PREFILTER_SKIP(TA_R103_R73_SPRITE);}
          else
#elif TA_R72_RECORD_ON
          if(cpu->pc==0xbc33){if(ta_native_r72_record(cpu,48u-run))return true;}
          else
#endif
          if(TA_R103_ACCEPT(TA_R103_R62_SPRITE,ta_native_r62_sprite(cpu,48u-run)))return true;
        } else
#endif
#if TA_R51_BLOCKS_ON
        if(descriptor&1024u) {
          #if defined(TA_R90_ACTIVE_PROLOGUE) && TA_R90_ACTIVE_PROLOGUE
          if(ta_native_r90_active_prologue(cpu,48u-run))return true;
#endif
#if defined(TA_R55_SERVICE_CHAIN) && TA_R55_SERVICE_CHAIN
          if(TA_R103_ACCEPT(TA_R103_R55_SERVICE,ta_native_r55_chained_game_blocks(cpu,48u-run)))return true;
#else
          if(ta_native_r51_game_block(cpu,48u-run))return true;
#endif
        } else
#endif
        if(descriptor&4096u) {
          if(TA_R103_ACCEPT(TA_R103_R48_QUIET,ta_native_r48_quiet_service(cpu,48u-run)))return true;
        }
#if TA_R58_IDLE_ON || TA_R58_COUNTER_ON
        else if(descriptor&2048u) {
          if(TA_R103_ACCEPT(TA_R103_R52_ACCUM,ta_native_r52_accumulate(cpu,48u-run)))return true;
        }
#if TA_R58_IDLE_ON
        else if(descriptor&8192u) {
#if TA_R58_TASK_ON
          if(pc==0x86b70cu) {if(TA_R103_ACCEPT(TA_R103_R58_TASK,ta_native_r58_task_prefix(cpu,48u-run)))return true;}
          else
#endif
          if(TA_R103_ACCEPT(TA_R103_R58_IDLE,ta_native_r58_idle_slots(cpu,48u-run)))return true;
        }
#endif
#if TA_R58_COUNTER_ON
        else if(descriptor&16384u) {
          if(TA_R103_ACCEPT(TA_R103_R58_COUNTER,ta_native_r58_counter_block(cpu,48u-run)))return true;
        }
#endif
#else
        else if(TA_R103_ACCEPT(TA_R103_R52_ACCUM,ta_native_r52_accumulate(cpu,48u-run)))return true;
#endif
      }
#elif TA_R51_BLOCKS_ON
      if((descriptor&1024u) && ta_native_r51_game_block(cpu,48u-run))return true;
#endif
      const unsigned delta=(descriptor>>6)&7u;
      uint32_t next=delta?pc+delta:0u;
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
      bool branchNext=(descriptor&512u)!=0;
#endif
      /* Only begin a microblock when there is a proven adjacent successor.
         The final instruction may have no successor and is still executed. */
#if defined(TA_R46_CPU_DISPATCH) && TA_R46_CPU_DISPATCH
      /* Single-site native work uses the same descriptor and exact body.
         A zero descriptor reaches the original interpreter exactly once. */
      if(!run && !(descriptor&63u))break;
#else
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
      if(!run && !next && !branchNext)break;
#else
      if(!run && !next)break;
#endif
#endif
#if defined(TA_VS_NATIVE_GAME) && TA_VS_NATIVE_GAME
      if(pc==0x829d51u||pc==0x829d59u||pc==0x829e01u||pc==0x829eb7u||pc==0x8481ddu||pc==0x8481f7u||pc==0x82e901u||pc==0x82e950u){extern void ta_vs_game_trace(Cpu*);ta_vs_game_trace(cpu);}
#endif
#if defined(TA_HOST_TRACE)
      extern void ta_trace_pc(uint8_t bank, uint16_t pc);
      ta_trace_pc(cpu->k,cpu->pc);
#endif
#if defined(TA_R19_HOST_DEEP) && TA_R19_HOST_DEEP
      ta_evidence_opcode(cpu);
#endif
#if defined(TA_R2_SAMPLE_PC) && TA_R2_SAMPLE_PC
      extern void ta_profile_sample(uint8_t bank, uint16_t pc);
      ta_profile_sample_state(cpu->k,cpu->pc,cpu->db,cpu->dp,
        (uint8_t)(cpu->mf | (cpu->xf<<1) | (cpu->e<<2)));
#endif
      const unsigned cls=descriptor&63u;
      bool executed=false;
      if(cls && s_identical_rom) {
#if defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
        if(cls>=33u)executed=ta_r23Execute(cpu,pc);
        else
#endif
          executed=ta_executeClass(cpu,cls-1u);
      }
      if(!executed) {
        /* Impossible for an exact-ROM verified site with a clean CPU state;
           never attempt to execute the rejected opcode a second time. */
        break;
      }
      ++run;
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
      if(dynamicLink){
        TA_DIAG(ta_native_r21_record_dynamic_link());
#if defined(TA_R24_LOOP_BACKEDGE) && TA_R24_LOOP_BACKEDGE && defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
        if(prior==0x80AEB7u && pc==0x80AEADu)TA_DIAG(ta_native_r24_record_loop_link());
#endif
      }
      if(!next && !branchNext)break;
      prior=pc;
#else
      if(!next)break;
#endif
      expectedNext=next;
    }
    if(run) {
      TA_DIAG(ta_native_record_microblock(run));
      return true;
    }
  }
#else
  (void)cpu;(void)expectedVblank;(void)expectedFrame;
#endif
  return false;
}
#endif

#if TA_R60_STORE_ON
#if defined(TA_R60_HOST_COVERAGE) && TA_R60_HOST_COVERAGE
uint64_t ta_r60HostStoreHits;
#endif
/* $85:87C1..87DE: TYX; four LDA #word / STA $7E:2000+$X,
 * $2002+$X, $2040+$X, $2042+$X; RTS. Full 16-bit widths only.
 * First observed through $858557 and $85858F. No guessed gameplay label.
 * All eight writes are disjoint from low-mirror return stack for every Y.
 * Preflight excludes all events/observers for the COMPLETE 10-op span.
 * Opcode fetches (30 bytes), WRAM writes/stack reads (10 x 8 clocks),
 * TYX/RTS idles (4 x 6 clocks) total 30*fetch+104 master clocks.
 * Exact write order retained, then stack reads; final open bus=return high.
 */
unsigned ta_native_r60_store(Cpu* c,unsigned budget){
 if(!c || !s_identical_rom || !s_r60ShaVerified || c->k!=0x85 || c->pc!=0x87c1 ||
    c->db!=0x85 || c->dp || c->e || c->mf || c->xf || budget<10 ||
    c->sp<0x100 || c->sp>0x1ffd || c->waiting || c->stopped || c->resetWanted ||
    c->intWanted || c->nmiWanted || (c->irqWanted&&!c->i))return 0;
 Snes* s=(Snes*)c->mem;
 if(!s || s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
    s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return 0;
 const unsigned h=s->hPos,clocks=30u*(s->fastMem?6u:8u)+104u,end=h+clocks;
 if((h&1u) || h>=1358 || end>ta_r41_busEndLimit[h] || !ta_r49_noTimerEdge(s,h,end))return 0;
 const unsigned y=c->y;
 s->ram[0x2000u+y]=0xa0;s->ram[0x2001u+y]=0x04;
 s->ram[0x2002u+y]=0xa1;s->ram[0x2003u+y]=0x04;
 s->ram[0x2040u+y]=0xa2;s->ram[0x2041u+y]=0x04;
 s->ram[0x2042u+y]=0xa3;s->ram[0x2043u+y]=0x04;
 const unsigned sp=c->sp;const uint16_t ret=(uint16_t)(s->ram[sp+1]|((uint16_t)s->ram[sp+2]<<8));
 c->x=c->y;c->a=0x04a3;c->n=false;c->z=false;c->sp=(uint16_t)(sp+2);c->pc=(uint16_t)(ret+1);
 s->openBus=(uint8_t)(ret>>8);s->hPos=(uint16_t)end;s->cycles+=clocks;s->apuMasterPending+=clocks;
 if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if defined(TA_R60_HOST_COVERAGE) && TA_R60_HOST_COVERAGE
 ++ta_r60HostStoreHits;
#endif
 return 10;
}
#endif

#if TA_R61_WORKERS_ON
#include "ta_r61_worker_values.h"
static unsigned r61_slot(uint16_t pc){return pc==0x87ac?48u:(pc-0x87c1u)/30u;}
unsigned ta_native_r61_worker_count(void){return 42;}
uint16_t ta_native_r61_worker_pc(unsigned i){return i<42?s_r61ObservedPcs[i]:0;}
#if TA_R61_WORKER_COVERAGE
uint32_t ta_native_r61_worker_hits(unsigned i){return i<42?s_r61WorkerHits[r61_slot(s_r61ObservedPcs[i])]:0;}
#endif
/* Host-reference execution establishes the allowlist. Original handler bytes
 * and P/E/DB/DP/stack evidence are in R61_NATIVE_MANIFEST.json. These are
 * fused native game operations, not a new 65816 opcode decoder.
 * Fixed writes are disjoint from every guarded return stack, even at Y=FFFF.
 * All unsupported PCs/states/events keep the original interpreter. */
unsigned ta_native_r61_worker(Cpu* c,unsigned budget){
 if(!c || !s_identical_rom || !s_r60ShaVerified || c->k!=0x85 || !ta_r61_maybe_worker(c->pc) ||
    c->db!=0x85 || c->dp || c->e || c->mf || c->xf || c->sp<0x100 || c->sp>0x1ffd ||
    c->waiting || c->stopped || c->resetWanted || c->intWanted || c->nmiWanted || (c->irqWanted&&!c->i))return 0;
 const unsigned slot=r61_slot(c->pc),ops=s_r61Workers[slot].opcodes;
 if(!ops || budget<ops)return 0;
 Snes* s=(Snes*)c->mem;
 if(!s || s->readHook || s->writeHook || s->inDma || s->dma->dmaState || s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return 0;
 const unsigned clocks=(ops==7?21u:30u)*(s->fastMem?6u:8u)+104u,h=s->hPos,end=h+clocks;
 if((h&1u)||h>=1358||end>ta_r41_busEndLimit[h]||!ta_r49_noTimerEdge(s,h,end))return 0;
 const unsigned y=c->y;const uint16_t* v=s_r61Workers[slot].values;
 s->ram[0x2000+y]=(uint8_t)v[0];s->ram[0x2001+y]=(uint8_t)(v[0]>>8);
 s->ram[0x2002+y]=(uint8_t)v[1];s->ram[0x2003+y]=(uint8_t)(v[1]>>8);
 s->ram[0x2040+y]=(uint8_t)v[2];s->ram[0x2041+y]=(uint8_t)(v[2]>>8);
 s->ram[0x2042+y]=(uint8_t)v[3];s->ram[0x2043+y]=(uint8_t)(v[3]>>8);
 const unsigned sp=c->sp;const uint16_t ret=(uint16_t)(s->ram[sp+1]|((uint16_t)s->ram[sp+2]<<8));
 c->x=c->y;c->a=v[3];c->n=(v[3]&0x8000u)!=0;c->z=v[3]==0;c->sp=(uint16_t)(sp+2);c->pc=(uint16_t)(ret+1);
 s->openBus=(uint8_t)(ret>>8);s->hPos=(uint16_t)end;s->cycles+=clocks;s->apuMasterPending+=clocks;
 if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if TA_R61_WORKER_COVERAGE
 if(s_r61WorkerHits[slot]!=0xffffffffu)++s_r61WorkerHits[slot];
#endif
 return ops;
}
#endif

#if TA_R62_SPRITE_ON
static uint16_t r62_word(const uint8_t* p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static void r62_store(uint8_t* p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
/* $80:BDB6..BDC9 (9 ops) computes vertical position and low tile bits.
 * $80:BDCA..BDE6 (15/16 ops) emits tile/attributes, advances the record,
 * decrements the remaining count and branches to the original loop/epilogue.
 * DB=$86 ROM sources and low mirrored WRAM destinations only; no IO aliases.
 * Arithmetic/writes below are explicit game operations, no opcode decoder.
 * Complete spans must fit the existing raster/refresh/IRQ fences. */
unsigned ta_native_r62_sprite(Cpu* c,unsigned budget){
 if(!c || c->k!=0x80 || (c->pc!=0xbdb6 && c->pc!=0xbdca))return 0;
 const unsigned block=c->pc==0xbdca;
#if TA_R62_COVERAGE
 if(s_r62Attempts[block]!=UINT32_MAX)++s_r62Attempts[block];
#endif
 if(!s_identical_rom || !s_r60ShaVerified || c->db!=0x86 || c->dp ||
    c->e || c->mf || c->xf || c->d || c->x>0x2fcu || c->y<0x8000 || c->y>0xfffb ||
    c->waiting || c->stopped || c->resetWanted || c->intWanted || c->nmiWanted ||
    (c->irqWanted&&!c->i))return 0;
 Snes* s=(Snes*)c->mem;
 if(!s || !s->cart || !s->cart->rom || s->cart->romSize!=1048576u ||
    s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
    s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return 0;
 const uint16_t count=(uint16_t)(r62_word(s->ram+0xe4)-1u);
 const unsigned ops=block?(count?16u:15u):9u;
 const unsigned fetch=s->fastMem?6u:8u;
 const unsigned clocks=block?(count?31u*fetch+156u:28u*fetch+162u):24u*fetch+94u;
 const unsigned h=s->hPos,end=h+clocks;
 if(budget<ops || (h&1u) || h>=1358 || end>ta_r41_busEndLimit[h] || !ta_r49_noTimerEdge(s,h,end))return 0;
 const uint8_t* src=s->cart->rom+0x30000u+(c->y&0x7fffu);
 const unsigned x=c->x;
 if(!block){
   r62_store(s->ram+0x1d01u+x,(uint16_t)(r62_word(src+2)+r62_word(s->ram+0xd1)));
   const uint16_t tile=r62_word(src+3),delta=r62_word(s->ram+0xdc);
   const uint32_t sum=(uint32_t)tile+delta;
   c->v=((~(tile^delta)&(tile^sum)&0x8000u)!=0);c->c=sum>0xffffu;
   c->a=(uint16_t)(sum&0x1ffu);c->n=false;c->z=c->a==0;
   r62_store(s->ram+0xe0,c->a);c->pc=0xbdca;s->openBus=(uint8_t)(c->a>>8);
 }else{
   const uint16_t attr=(uint16_t)((r62_word(src+3)&r62_word(s->ram+0xd8))|r62_word(s->ram+0xda)|r62_word(s->ram+0xe0));
   r62_store(s->ram+0x1d02u+x,attr);
   const unsigned y=c->y,sum=y+5u;
   c->a=c->y=(uint16_t)sum;c->x=(uint16_t)(x+4u);
   c->c=sum>0xffffu;c->v=((~(y^5u)&(y^sum)&0x8000u)!=0);
   /* DEC dp writes high byte first; preserve that ordering. */
   s->ram[0xe5]=(uint8_t)(count>>8);s->ram[0xe4]=(uint8_t)count;
   c->n=(count&0x8000u)!=0;c->z=count==0;c->pc=count?0xbd6d:0xbde7;
   s->openBus=count?0xbd:0x03;
 }
 s->hPos=(uint16_t)end;s->cycles+=clocks;s->apuMasterPending+=clocks;
 if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if TA_R62_COVERAGE
 if(s_r62Hits[block]!=UINT32_MAX)++s_r62Hits[block];
#endif
 return ops;
}
#endif

#if TA_R72_RECORD_ON
#if TA_R72_COVERAGE
static uint32_t s_r72Attempts,s_r72Hits;
void ta_native_r72_counts(uint32_t* a,uint32_t* h){if(a)*a=s_r72Attempts;if(h)*h=s_r72Hits;}
#endif
/* $80:BC33..BC4E: vertical coordinate, raw tile word, Y+=5, X+=4,
 * DEC remaining count, BNE $BBEA / continue $BC4F. 17 guest operations.
 * 28 opcode/operand bytes + four ROM data bytes + 80 WRAM clocks +
 * 15 six-clock idles; taken BNE adds one idle. No opcode decoding here.
 * DB=$86 ROM input and low mirrored WRAM output. Preserve overlap between
 * the two output words, ADC flags, DEC high-before-low write order and bus.
 */
unsigned ta_native_r72_record(Cpu* c,unsigned budget){
 if(!c || c->k!=0x80 || c->pc!=0xbc33)return 0;
#if TA_R72_COVERAGE
 if(s_r72Attempts!=UINT32_MAX)++s_r72Attempts;
#endif
 if(!s_identical_rom || !s_r60ShaVerified || c->db!=0x86 || c->dp ||
    c->e || c->mf || c->xf || c->d || c->x>0x2fcu || c->y<0x8000 || c->y>0xfffb ||
    c->waiting || c->stopped || c->resetWanted || c->intWanted || c->nmiWanted ||
    (c->irqWanted&&!c->i))return 0;
 Snes* s=(Snes*)c->mem;
 if(!s || !s->cart || !s->cart->rom || s->cart->romSize!=1048576u ||
    s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
    s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return 0;
 const uint16_t count=(uint16_t)(r62_word(s->ram+0xe4)-1u);
 const unsigned clocks=32u*(s->fastMem?6u:8u)+170u+(count?6u:0u);
 const unsigned h=s->hPos,end=h+clocks;
 if(budget<17u || (h&1u) || h>=1358 || end>ta_r41_busEndLimit[h] || !ta_r49_noTimerEdge(s,h,end))return 0;
 const uint8_t* src=s->cart->rom+0x30000u+(c->y&0x7fffu);
 const uint16_t ordinate=r62_word(src+2),delta=r62_word(s->ram+0xd1);
 const uint32_t sum=(uint32_t)ordinate+delta;
 r62_store(s->ram+0x1d01u+c->x,(uint16_t)sum);
 c->a=r62_word(src+3);r62_store(s->ram+0x1d02u+c->x,c->a);
 c->y=(uint16_t)(c->y+5u);c->x=(uint16_t)(c->x+4u);
 s->ram[0xe5]=(uint8_t)(count>>8);s->ram[0xe4]=(uint8_t)count;
 c->c=sum>0xffffu;c->v=((~(ordinate^delta)&(ordinate^sum)&0x8000u)!=0);
 c->n=(count&0x8000u)!=0;c->z=count==0;c->pc=count?0xbbea:0xbc4f;
 s->openBus=0x9b;s->hPos=(uint16_t)end;s->cycles+=clocks;s->apuMasterPending+=clocks;
 if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if TA_R72_COVERAGE
 if(s_r72Hits!=UINT32_MAX)++s_r72Hits;
#endif
 return 17u;
}
#endif

#if TA_R103_GUARD_AUDIT
static uint64_t s_r103R73RejectReasons[4];
void ta_native_r103_r73_reasons(uint64_t out[4]){
 if(!out)return;
 for(unsigned i=0;i<4u;++i)out[i]=s_r103R73RejectReasons[i];
}
#define TA_R103_R73_NO(reason) (++s_r103R73RejectReasons[(reason)],0u)
#else
#define TA_R103_R73_NO(reason) 0u
#endif
#if defined(TA_R104_GUARD_DETAIL) && TA_R104_GUARD_DETAIL
/* Audit only: first true reject predicate, matching R73 guard order exactly.
 * No guest state modified; compiled out of the clean PlayR104SpriteResume build.
 * Unknown ROM identity, non-bank register states and pending events remain guarded. */
enum {R104_IDENTITY=0,R104_DB80,R104_DB81,R104_DB_OTHER_ROM,R104_DB_OUTSIDE_ROM,
      R104_DP,R104_EMULATION,R104_M8,R104_X8,R104_DECIMAL,R104_XBOUNDS,
      R104_YBOUNDS,R104_CPU_PENDING,R104_OTHER,R104_NUM_REASONS};
static uint64_t s_r104R73CpuReject[R104_NUM_REASONS];
void ta_native_r104_r73_cpu_reasons(uint64_t out[R104_NUM_REASONS]){
 if(!out)return;
 for(unsigned i=0;i<R104_NUM_REASONS;++i)out[i]=s_r104R73CpuReject[i];
}
static void ta_r104_classify_r73_cpu_guard(const Cpu*c){
 unsigned reason=R104_OTHER;
 if(!c || c->k!=0x80 || !s_identical_rom || !s_r60ShaVerified)reason=R104_IDENTITY;
#if defined(TA_R105_DB81_R73) && TA_R105_DB81_R73
 else if(c->db!=0x86 && c->db!=0x81){
#else
 else if(c->db!=0x86){
#endif
  if(c->db==0x80)reason=R104_DB80;
  else if(c->db==0x81)reason=R104_DB81;
  else if(c->db>=0x80 && c->db<=0x9f)reason=R104_DB_OTHER_ROM;
  else reason=R104_DB_OUTSIDE_ROM;
 }else if(c->dp)reason=R104_DP;
 else if(c->e)reason=R104_EMULATION;
 else if(c->mf)reason=R104_M8;
 else if(c->xf)reason=R104_X8;
 else if(c->d)reason=R104_DECIMAL;
 else if((c->x&3u) || c->x>0x1fcu)reason=R104_XBOUNDS;
 else if(c->y<0x8000 || c->y>0xfffb)reason=R104_YBOUNDS;
 else if(c->waiting || c->stopped || c->resetWanted || c->intWanted || c->nmiWanted || (c->irqWanted&&!c->i)) reason=R104_CPU_PENDING;
 ++s_r104R73CpuReject[reason];
}
#endif
#if TA_R73_CHAIN_ON
#if TA_R73_COVERAGE
static uint32_t s_r73Stages[8],s_r73Calls,s_r73Rejects;
static uint64_t s_r73Ops;
void ta_native_r73_counts(uint32_t stages[8],uint32_t* calls,uint32_t* rejects,uint64_t* ops){
 for(unsigned i=0;i<8;++i)if(stages)stages[i]=s_r73Stages[i];
 if(calls)*calls=s_r73Calls;if(rejects)*rejects=s_r73Rejects;if(ops)*ops=s_r73Ops;
}
#endif
/* Native game operations, no guest opcode dispatch. Eight segment types:
 * H: coordinate + signed/bit branches (7 ops), B: bitmap (5/6 ops),
 * V: ordinate/tile output (6 ops), T: index/count/loop tail (11 ops).
 * The BD sibling splits ordinate, tile, attributes and tail separately.
 * Resolve the original event/IRQ limit once; stop BEFORE a segment which
 * cannot fit, leaving its original continuation PC for fallback or reentry.
 * Fourteen entries reuse the existing bit-15 map; no new dispatch map.
 */
unsigned ta_native_r73_chain(Cpu* c,unsigned budget){
 if(!c || c->k!=0x80 || !s_identical_rom || !s_r60ShaVerified || 
#if defined(TA_R105_DB81_R73) && TA_R105_DB81_R73
    (c->db!=0x81 && c->db!=0x86) ||
#elif defined(TA_R101_SPRITE_RESUME) && TA_R101_SPRITE_RESUME
    (c->db<0x80 || c->db>0x9f) ||
#else
    c->db!=0x86 ||
#endif
    c->dp ||
    c->e || c->mf || c->xf || c->d || (c->x&3u) || c->x>0x1fcu || c->y<0x8000 || c->y>0xfffb ||
    c->waiting || c->stopped || c->resetWanted || c->intWanted || c->nmiWanted ||
    (c->irqWanted&&!c->i)){
#if defined(TA_R104_GUARD_DETAIL) && TA_R104_GUARD_DETAIL
  ta_r104_classify_r73_cpu_guard(c);
#endif
  return TA_R103_R73_NO(0);
 }
 Snes* s=(Snes*)c->mem;
 if(!s || !s->cart || !s->cart->rom || s->cart->romSize!=1048576u ||
    s->readHook || s->writeHook || s->inDma || s->dma->dmaState ||
    s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return TA_R103_R73_NO(1);
 const unsigned initial=s->hPos,fetch=s->fastMem?6u:8u;
 if((initial&1u) || initial>=1358 || s->irqCondition)return TA_R103_R73_NO(2);
 unsigned limit=ta_r41_busEndLimit[initial];
 if(s->hIrqEnabled || s->vIrqEnabled){
  if(s->autoJoyTimer&1u)return TA_R103_R73_NO(2);
  if(!s->vIrqEnabled || s->vPos==s->vTimer){
   if(!s->hIrqEnabled)return TA_R103_R73_NO(2);
   const unsigned edge=(unsigned)s->hTimer*4u;
   if(edge>=initial && edge<limit)limit=edge;
  }
 }
 unsigned h=initial,run=0;
 if(limit<h)return TA_R103_R73_NO(2);
 while(c->x<=0x1fcu && c->y>=0x8000 && c->y<=0xfffb){
  const unsigned entry=c->pc,x=c->x;unsigned stage,ops,clocks;
  const uint8_t* src=s->cart->rom+
#if (defined(TA_R105_DB81_R73) && TA_R105_DB81_R73) || (defined(TA_R101_SPRITE_RESUME) && TA_R101_SPRITE_RESUME)
      ((unsigned)(c->db&31u)<<15)+
#else
      0x30000u+
#endif
      (c->y&0x7fffu);
  if(entry==0xbbea || entry==0xbd6d){
   const uint16_t ordinate=r62_word(src),delta=r62_word(s->ram+0xcf);
   const uint32_t sum=(uint32_t)ordinate+delta;
   const bool negative=(ordinate&0x8000u)!=0,bit=(sum&0x100u)!=0;
   stage=0;ops=7;clocks=18u*fetch+50u+(negative?6u:0u)+(!bit?6u:0u);
   if(ops>budget-run || clocks>limit-h)break;
   c->a=(uint16_t)sum;r62_store(s->ram+0x1d00u+x,c->a);
   c->c=sum>0xffffu;c->v=((~(ordinate^delta)&(ordinate^sum)&0x8000u)!=0);
   c->n=(c->a&0x8000u)!=0;c->z=!bit;
   c->pc=entry==0xbd6d?(negative?(bit?0xbd98:0xbda8):(bit?0xbd7d:0xbdb6)):(negative?(bit?0xbc15:0xbc25):(bit?0xbbfa:0xbc33));
   s->openBus=negative?0x10:0x39;
  }else if(entry==0xbbfa || entry==0xbc15 || entry==0xbc25 || entry==0xbd7d || entry==0xbd98 || entry==0xbda8){
   const bool clear=entry==0xbc25 || entry==0xbda8;const bool positive=entry==0xbbfa || entry==0xbd7d;stage=1;ops=clear?5u:6u;
   clocks=(clear?18u:20u)*fetch+80u+(clear?0u:6u);
   if(ops>budget-run || clocks>limit-h)break;
   const uint16_t pointer=r62_word(s->cart->rom+0x41feu+x);
   /* Verified x-aligned table domain: 1F00..1F1E, disjoint from records. */
   if(pointer<0x1f00 || pointer>0x1f1e)break;
   const unsigned mask=clear?0x43feu:positive?0x4200u:0x4400u;
   r62_store(s->ram+0xe0,pointer);
   c->a=(uint16_t)(r62_word(s->ram+pointer)|r62_word(s->cart->rom+mask+x));
   r62_store(s->ram+pointer,c->a);c->n=(c->a&0x8000u)!=0;c->z=c->a==0;
   c->pc=entry>=0xbd6d?0xbdb6:0xbc33;s->openBus=clear?(uint8_t)(c->a>>8):positive?0x29:0x0e;

#if defined(TA_R101_SPRITE_RESUME) && TA_R101_SPRITE_RESUME
  }else if(entry==0xbc37 || entry==0xbc39 || entry==0xbc3c || entry==0xbc3f){
   stage=2;
   ops=entry==0xbc37?4u:entry==0xbc39?3u:entry==0xbc3c?2u:1u;
   clocks=entry==0xbc37?13u*fetch+66u:entry==0xbc39?11u*fetch+50u:entry==0xbc3c?8u*fetch+28u:3u*fetch+22u;
   if(ops>budget-run || clocks>limit-h)break;
   if(entry==0xbc37){const unsigned prior=c->a,delta=r62_word(s->ram+0xd1),sum=prior+delta+(unsigned)c->c;c->a=(uint16_t)sum;c->c=sum>65535u;c->v=((~(prior^delta)&(prior^sum)&0x8000u)!=0);}
   if(entry<=0xbc39)r62_store(s->ram+0x1d01u+x,c->a);
   if(entry<=0xbc3c){c->a=r62_word(src+3);c->n=(c->a&0x8000u)!=0;c->z=c->a==0;}
   r62_store(s->ram+0x1d02u+x,c->a);c->pc=0xbc42;s->openBus=(uint8_t)(c->a>>8);
  }else if(entry==0xbc4b || entry==0xbde0){
   const uint16_t count=(uint16_t)(r62_word(s->ram+0xe4)-1u);stage=entry==0xbc4b?3u:7u;
   ops=entry==0xbc4b?2u:count?3u:2u;
   clocks=entry==0xbc4b?4u*fetch+38u+(count?6u:0u):count?7u*fetch+38u:4u*fetch+44u;
   if(ops>budget-run || clocks>limit-h)break;
   s->ram[0xe5]=(uint8_t)(count>>8);s->ram[0xe4]=(uint8_t)count;c->n=(count&0x8000u)!=0;c->z=count==0;
   c->pc=entry==0xbc4b?(count?0xbbea:0xbc4f):(count?0xbd6d:0xbde7);s->openBus=entry==0xbc4b?0x9b:count?0xbd:0x03;
  }else if(entry==0xbdba || entry==0xbdbc){
   stage=4;ops=entry==0xbdba?2u:1u;clocks=entry==0xbdba?5u*fetch+38u:3u*fetch+22u;
   if(ops>budget-run || clocks>limit-h)break;
   if(entry==0xbdba){const unsigned prior=c->a,delta=r62_word(s->ram+0xd1),sum=prior+delta+(unsigned)c->c;c->a=(uint16_t)sum;c->c=sum>65535u;c->v=((~(prior^delta)&(prior^sum)&0x8000u)!=0);c->n=(c->a&0x8000u)!=0;c->z=c->a==0;}
   r62_store(s->ram+0x1d01u+x,c->a);c->pc=0xbdbf;s->openBus=(uint8_t)(c->a>>8);
  }else if(entry==0xbdc3 || entry==0xbdc5 || entry==0xbdc8){
   stage=5;ops=entry==0xbdc3?3u:entry==0xbdc5?2u:1u;clocks=entry==0xbdc3?7u*fetch+32u:entry==0xbdc5?5u*fetch+16u:2u*fetch+16u;
   if(ops>budget-run || clocks>limit-h)break;
   if(entry==0xbdc3){const unsigned prior=c->a,delta=r62_word(s->ram+0xdc),sum=prior+delta+(unsigned)c->c;c->a=(uint16_t)sum;c->c=sum>65535u;c->v=((~(prior^delta)&(prior^sum)&0x8000u)!=0);}
   if(entry<=0xbdc5){c->a&=0x1ffu;c->n=false;c->z=c->a==0;}
   r62_store(s->ram+0xe0,c->a);c->pc=0xbdca;s->openBus=(uint8_t)(c->a>>8);
  }else if(entry==0xbdcd || entry==0xbdcf || entry==0xbdd1 || entry==0xbdd3){
   stage=6;ops=(0xbdd3u-entry)/2u+1u;clocks=(2u*ops+1u)*fetch+16u*(ops-1u)+22u;
   if(ops>budget-run || clocks>limit-h)break;
   if(entry==0xbdcd)c->a&=r62_word(s->ram+0xd8);
   if(entry<=0xbdcf)c->a|=r62_word(s->ram+0xda);
   if(entry<=0xbdd1){c->a|=r62_word(s->ram+0xe0);c->n=(c->a&0x8000u)!=0;c->z=c->a==0;}
   r62_store(s->ram+0x1d02u+x,c->a);c->pc=0xbdd6;s->openBus=(uint8_t)(c->a>>8);
#endif
  }else if(entry==0xbc33){
   stage=2;ops=6;clocks=19u*fetch+78u;
   if(ops>budget-run || clocks>limit-h)break;
   const uint16_t ordinate=r62_word(src+2),delta=r62_word(s->ram+0xd1);
   const uint32_t sum=(uint32_t)ordinate+delta;
   r62_store(s->ram+0x1d01u+x,(uint16_t)sum);
   c->a=r62_word(src+3);r62_store(s->ram+0x1d02u+x,c->a);
   c->c=sum>0xffffu;c->v=((~(ordinate^delta)&(ordinate^sum)&0x8000u)!=0);
   c->n=(c->a&0x8000u)!=0;c->z=c->a==0;c->pc=0xbc42;s->openBus=(uint8_t)(c->a>>8);
  }else if(entry==0xbc42){
   const uint16_t count=(uint16_t)(r62_word(s->ram+0xe4)-1u);
   stage=3;ops=11;clocks=13u*fetch+92u+(count?6u:0u);
   if(ops>budget-run || clocks>limit-h)break;
   c->y=(uint16_t)(c->y+5u);c->x=(uint16_t)(x+4u);
   s->ram[0xe5]=(uint8_t)(count>>8);s->ram[0xe4]=(uint8_t)count;
   c->n=(count&0x8000u)!=0;c->z=count==0;c->pc=count?0xbbea:0xbc4f;s->openBus=0x9b;
  }else if(entry==0xbdb6){
   stage=4;ops=4;clocks=11u*fetch+50u;
   if(ops>budget-run || clocks>limit-h)break;
   const uint16_t ordinate=r62_word(src+2),delta=r62_word(s->ram+0xd1);
   const uint32_t sum=(uint32_t)ordinate+delta;
   c->a=(uint16_t)sum;r62_store(s->ram+0x1d01u+x,c->a);
   c->c=sum>0xffffu;c->v=((~(ordinate^delta)&(ordinate^sum)&0x8000u)!=0);
   c->n=(c->a&0x8000u)!=0;c->z=c->a==0;c->pc=0xbdbf;s->openBus=(uint8_t)(c->a>>8);
  }else if(entry==0xbdbf){
   stage=5;ops=5;clocks=13u*fetch+44u;
   if(ops>budget-run || clocks>limit-h)break;
   const uint16_t tile=r62_word(src+3),delta=r62_word(s->ram+0xdc);
   const uint32_t sum=(uint32_t)tile+delta;
   c->a=(uint16_t)(sum&0x1ffu);r62_store(s->ram+0xe0,c->a);
   c->c=sum>0xffffu;c->v=((~(tile^delta)&(tile^sum)&0x8000u)!=0);
   c->n=false;c->z=c->a==0;c->pc=0xbdca;s->openBus=(uint8_t)(c->a>>8);
  }else if(entry==0xbdca){
   stage=6;ops=5;clocks=14u*fetch+76u;
   if(ops>budget-run || clocks>limit-h)break;
   c->a=(uint16_t)((r62_word(src+3)&r62_word(s->ram+0xd8))|r62_word(s->ram+0xda)|r62_word(s->ram+0xe0));
   r62_store(s->ram+0x1d02u+x,c->a);c->n=(c->a&0x8000u)!=0;c->z=c->a==0;
   c->pc=0xbdd6;s->openBus=(uint8_t)(c->a>>8);
  }else if(entry==0xbdd6){
   const uint16_t count=(uint16_t)(r62_word(s->ram+0xe4)-1u);
   stage=7;ops=count?11u:10u;clocks=count?17u*fetch+80u:14u*fetch+86u;
   if(ops>budget-run || clocks>limit-h)break;
   const unsigned y=c->y,sum=y+5u;
   c->a=c->y=(uint16_t)sum;c->x=(uint16_t)(x+4u);
   c->c=sum>0xffffu;c->v=((~(y^5u)&(y^sum)&0x8000u)!=0);
   s->ram[0xe5]=(uint8_t)(count>>8);s->ram[0xe4]=(uint8_t)count;
   c->n=(count&0x8000u)!=0;c->z=count==0;c->pc=count?0xbd6d:0xbde7;
   s->openBus=count?0xbd:0x03;
  }else break;
  h+=clocks;run+=ops;
#if TA_R73_COVERAGE
  if(s_r73Stages[stage]!=UINT32_MAX)++s_r73Stages[stage];s_r73Ops+=ops;
#endif
 }
 const unsigned clocks=h-initial;
 if(!run){
#if TA_R73_COVERAGE
  if(s_r73Rejects!=UINT32_MAX)++s_r73Rejects;
#endif
  return TA_R103_R73_NO(3);
 }
 s->hPos=(uint16_t)h;s->cycles+=clocks;s->apuMasterPending+=clocks;
 if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if TA_R73_COVERAGE
 if(s_r73Calls!=UINT32_MAX)++s_r73Calls;
#endif
 return run;
}
#endif

#if TA_R74_FIELDS_ON
#if TA_R74_COVERAGE
static uint32_t s_r74Paths[4],s_r74Calls;static uint64_t s_r74Ops;
void ta_native_r74_counts(uint32_t paths[4],uint32_t* calls,uint64_t* ops){for(unsigned i=0;i<4;++i)if(paths)paths[i]=s_r74Paths[i];if(calls)*calls=s_r74Calls;if(ops)*ops=s_r74Ops;}
#endif
/* $82:D9D4..DACF: LDY row index; six flags/auxiliary-field predicates;
 * DEC row count and optional row-index +=16 / backedge. RTS stays reference.
 * Each complete field or loop segment must fit the original event fence.
 * Fixed C game operations, no opcode decoder or new whole-ROM map.
 */
unsigned ta_native_r74_fields(Cpu* c,unsigned budget){
 if(!c || !s_identical_rom || !s_r60ShaVerified || c->k!=0x82 || c->db!=0x82 || c->dp || c->e || c->mf || c->xf || c->d || c->x>0x800 ||
    c->waiting || c->stopped || c->resetWanted || c->intWanted || c->nmiWanted || (c->irqWanted&&!c->i))return 0;
 unsigned state;
 if(c->pc==0xd9d4)state=6;else if(c->pc==0xdac0)state=7;
 else{if(c->pc<0xd9d6 || c->pc>0xda99)return 0;state=(c->pc-0xd9d6u)/39u;if(state>=6 || c->pc!=0xd9d6u+39u*state)return 0;}
 Snes* s=(Snes*)c->mem;
 if(!s || !s->cart || !s->cart->rom || s->cart->romSize!=1048576u || s->readHook || s->writeHook || s->inDma || s->dma->dmaState || s->dma->hdmaInitRequested || s->dma->hdmaRunRequested)return 0;
 const unsigned initial=s->hPos,fetch=s->fastMem?6u:8u;
 if((initial&1u) || initial>=1358 || s->irqCondition)return 0;
 unsigned limit=ta_r41_busEndLimit[initial];
 if(s->hIrqEnabled || s->vIrqEnabled){if(s->autoJoyTimer&1u)return 0;if(!s->vIrqEnabled || s->vPos==s->vTimer){if(!s->hIrqEnabled)return 0;const unsigned edge=(unsigned)s->hTimer*4u;if(edge>=initial && edge<limit)limit=edge;}}
 if(limit<initial)return 0;unsigned h=initial,run=0;
 while(state<8){
  unsigned ops,clocks;
  if(state==6){
   ops=1;clocks=2u*fetch+16u;if(ops>budget-run || clocks>limit-h)break;
   c->y=r62_word(s->ram);c->n=(c->y&0x8000u)!=0;c->z=c->y==0;c->pc=0xd9d6;s->openBus=(uint8_t)(c->y>>8);state=0;
  }else if(state<6){
   if(c->y>0x1000u)break;
   const unsigned address=0xd7cu+2u*state+c->y,aux=0xf8cu+2u*state+c->y;
   const uint16_t field=r62_word(s->ram+address),masked=field&0x0f40u;
   const unsigned path=!(field&0x0640u)?0u:masked!=0x0200u?1u:(r62_word(s->ram+aux)&0x0100u)?2u:3u;
   ops=path==0?3u:path==1?8u:path==2?11u:14u;
   clocks=path==0?8u*fetch+28u:path==1?22u*fetch+50u:path==2?30u*fetch+72u:39u*fetch+110u;
   if(ops>budget-run || clocks>limit-h)break;
   if(!path){c->a=field;c->n=(field&0x8000u)!=0;c->z=true;s->openBus=0x1f;}
   else{c->c=masked>=0x0200u;if(path==3)r62_store(s->ram+address,(uint16_t)(field&0xfdffu));c->a=1;c->n=c->z=false;r62_store(s->ram+0x4f4u+c->x,1);s->openBus=0;}
   c->pc=(uint16_t)(0xd9d6u+39u*(state+1u));state=state==5?7:state+1;
#if TA_R74_COVERAGE
   if(s_r74Paths[path]!=UINT32_MAX)++s_r74Paths[path];
#endif
  }else{
   const uint16_t count=(uint16_t)(r62_word(s->ram+0x3fc)-1u);
   ops=count?7u:2u;clocks=count?16u*fetch+76u:5u*fetch+44u;
   if(ops>budget-run || clocks>limit-h)break;
   s->ram[0x3fd]=(uint8_t)(count>>8);s->ram[0x3fc]=(uint8_t)count;
   if(!count){c->n=false;c->z=true;c->pc=0xdad0;s->openBus=0x0b;state=8;}
   else{const uint16_t prior=r62_word(s->ram);const uint32_t sum=(uint32_t)prior+16u;c->a=(uint16_t)sum;r62_store(s->ram,c->a);c->c=sum>0xffffu;c->v=((~(prior^16u)&(prior^sum)&0x8000u)!=0);c->n=(c->a&0x8000u)!=0;c->z=c->a==0;c->pc=0xd9d4;s->openBus=0xd9;state=6;}
  }
  h+=clocks;run+=ops;
 }
 if(!run)return 0;const unsigned clocks=h-initial;s->hPos=(uint16_t)h;s->cycles+=clocks;s->apuMasterPending+=clocks;
 if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if TA_R74_COVERAGE
 if(s_r74Calls!=UINT32_MAX)++s_r74Calls;s_r74Ops+=run;
#endif
 return run;
}
#endif

#if TA_R78_QUIET_ON
#if TA_R78_COVERAGE
static uint32_t s_r78Stages[5],s_r78Calls;static uint64_t s_r78Ops;
void ta_native_r78_counts(uint32_t stages[5],uint32_t* calls,uint64_t* ops){for(unsigned i=0;i<5;++i)if(stages)stages[i]=s_r78Stages[i];if(calls)*calls=s_r78Calls;if(ops)*ops=s_r78Ops;}
#endif
/* Observed $86:9B2A quiet path: four flag checks, Y advances and row tail.
 * Active fields leave their original continuation intact for reference code.
 * No indirect handler, RTL, opcode decoder or new ROM-wide map is introduced. */
unsigned ta_native_r78_quiet(Cpu* c,unsigned budget){
 if(!c||!s_identical_rom||!s_r60ShaVerified||c->k!=0x86||c->db!=0x82||c->dp||c->e||c->mf||c->xf||c->d||c->y>0x1000u||c->waiting||c->stopped||c->resetWanted||c->intWanted||c->nmiWanted||(c->irqWanted&&!c->i))return 0;
 unsigned stage=c->pc==0x9b2a?0u:c->pc==0x9b59?1u:c->pc==0x9b75?2u:c->pc==0x9b91?3u:c->pc==0x9be8?4u:5u;if(stage==5)return 0;
 Snes* s=c->mem;if(!s||!s->cart||!s->cart->rom||s->cart->romSize!=1048576u||s->readHook||s->writeHook||s->inDma||s->dma->dmaState||s->dma->hdmaInitRequested||s->dma->hdmaRunRequested)return 0;
 const unsigned initial=s->hPos,fetch=s->fastMem?6u:8u;if((initial&1u)||initial>=1358u||s->irqCondition)return 0;
 unsigned limit=ta_r41_busEndLimit[initial];if(s->hIrqEnabled||s->vIrqEnabled){if(s->autoJoyTimer&1u)return 0;if(!s->vIrqEnabled||s->vPos==s->vTimer){if(!s->hIrqEnabled)return 0;unsigned edge=(unsigned)s->hTimer*4u;if(edge>=initial&&edge<limit)limit=edge;}}
 if(limit<initial)return 0;unsigned h=initial,run=0;
 while(stage<5&&c->y<=0x1000u){unsigned ops,clocks,done=stage;
  if(stage<4){
   const unsigned y=c->y+(stage?2u:0u);const uint16_t field=r62_word(s->ram+0xf7cu+y);if(field&0x0c00u)break;
   ops=stage==0?3u:stage==3?6u:5u;clocks=stage==0?8u*fetch+28u:stage==3?13u*fetch+34u:10u*fetch+40u;
   if(ops>budget-run||clocks>limit-h)break;
   c->y=(uint16_t)y;c->a=field;c->n=(field&0x8000u)!=0;c->z=true;
   c->pc=stage==0?0x9b59:stage==1?0x9b75:stage==2?0x9b91:0x9be8;s->openBus=stage==0?0x27:stage==3?0x9b:0x12;++stage;
  }else{
   const uint16_t count=(uint16_t)(r62_word(s->ram+0x3fc)-1u);ops=count?9u:2u;clocks=count?22u*fetch+120u:5u*fetch+44u;
   if(ops>budget-run||clocks>limit-h)break;
   s->ram[0x3fd]=(uint8_t)(count>>8);s->ram[0x3fc]=(uint8_t)count;
   if(!count){c->n=false;c->z=true;c->pc=0x9bfe;s->openBus=0x11;stage=5;}
   else{const uint16_t prior=r62_word(s->ram+0x394),row=(uint16_t)(prior-16u),aux=(uint16_t)(r62_word(s->ram+0x3fa)-1u);
    c->a=c->y=row;r62_store(s->ram+0x394,row);c->c=prior>=16u;c->v=((prior^16u)&(prior^row)&0x8000u)!=0;
    s->ram[0x3fb]=(uint8_t)(aux>>8);s->ram[0x3fa]=(uint8_t)aux;c->n=(aux&0x8000u)!=0;c->z=aux==0;c->pc=0x9b2a;s->openBus=0x9b;stage=0;
   }
  }
  h+=clocks;run+=ops;
#if TA_R78_COVERAGE
  if(s_r78Stages[done]!=UINT32_MAX)++s_r78Stages[done];s_r78Ops+=ops;
#endif
 }
 if(!run)return 0;unsigned clocks=h-initial;s->hPos=(uint16_t)h;s->cycles+=clocks;s->apuMasterPending+=clocks;if(s->autoJoyTimer)s->autoJoyTimer=s->autoJoyTimer<=clocks?0:(uint16_t)(s->autoJoyTimer-clocks);
#if TA_R78_COVERAGE
 if(s_r78Calls!=UINT32_MAX)++s_r78Calls;
#endif
 return run;
}
#endif

#include "ta_r80_sprite_impl.h"

#include "ta_r81_animation_impl.h"
