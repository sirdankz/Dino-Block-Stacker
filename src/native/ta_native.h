/* Tetris Attack exact-ROM-native cutover proof (R1). */
#ifndef TA_NATIVE_H
#define TA_NATIVE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cpu.h"
#ifdef __cplusplus
extern "C" {
#endif
bool ta_native_init(const uint8_t *rom, size_t length);
#if defined(TA_R112_NATIVE_9AC4) && TA_R112_NATIVE_9AC4
bool ta_native_r112_sha_verified(void);
#endif
bool ta_native_try_execute(Cpu *cpu);
#if defined(TA_R91_TRANSFER_LOOP) && TA_R91_TRANSFER_LOOP
unsigned ta_native_r91_transfer_loop(Cpu* cpu,unsigned budget);
#endif
#if defined(TA_R40_DECODED_DISPATCH) && TA_R40_DECODED_DISPATCH
uint16_t ta_native_r40_descriptor(uint32_t pc);
bool ta_native_run_decoded_block(Cpu* cpu,bool expectedVblank,uint32_t expectedFrame);
#endif
/* R103: diagnostic-only dispatch coverage. Counters are cumulative and never
 * modify CPU, bus, graphics or guest-time semantics. Compiled out normally.
 * A dispatch attempt is NOT a game-function call: it means descriptor routing
 * reached the instrumented native candidate on this host CPU execution. */
#if defined(TA_R115_R108_REJECT_DETAIL) && TA_R115_R108_REJECT_DETAIL
/* One exclusive entry-rejection cause per unsuccessful R108 attempt. */
void ta_native_r115_r108_rejection_counts(uint64_t out[12]);
#endif
#if defined(TA_R114_R108_DIAG) && TA_R114_R108_DIAG
/* Exact same R108 native branch as the normal build; counters only. */
void ta_native_r114_r108_counts(uint64_t out[3]);
#endif
#ifndef TA_R103_GUARD_AUDIT
#define TA_R103_GUARD_AUDIT 0
#endif
#if TA_R103_GUARD_AUDIT
enum {TA_R103_R55_SERVICE=0,TA_R103_R52_ACCUM,TA_R103_R58_COUNTER,
      TA_R103_R58_IDLE,TA_R103_R48_QUIET,TA_R103_R62_SPRITE,
      TA_R103_R73_SPRITE,TA_R103_R81_ANIMATION,TA_R103_R80_SPRITE,
      TA_R103_R74_FIELDS,TA_R103_R78_QUIET,TA_R103_R91_TRANSFER,
      TA_R103_R58_TASK,TA_R103_FAMILY_COUNT};
void ta_native_r103_get(unsigned family,uint64_t counts[5]);
void ta_native_r103_r73_reasons(uint64_t counts[4]);
#if defined(TA_R104_GUARD_DETAIL) && TA_R104_GUARD_DETAIL
void ta_native_r104_r73_cpu_reasons(uint64_t counts[14]);
#endif
const char* ta_native_r103_name(unsigned family);
#endif
uint32_t ta_native_site_hits(void);
uint32_t ta_native_enabled_sites(void);
/* R54: optional profiling only. No counter overhead in ordinary builds. */
#if defined(TA_R54_XBOX_PROFILE) && TA_R54_XBOX_PROFILE
void ta_native_r54_hot_counts(uint32_t counts[4]); /* service, C12D, 8162, C13A */
#endif
/* R10: adjacency is ROM-verified, but execution checks actual PC and frame/IRQ
   boundaries after every instruction. Native site total includes microblock
   instructions (do not add these counters to native_site_hits). */
uint32_t ta_native_next_in_microblock(uint32_t pc);
void ta_native_record_microblock(uint32_t n);
uint32_t ta_native_microblock_calls(void);
uint32_t ta_native_microblock_instructions(void);
uint32_t ta_native_microblock_max_length(void);
#if defined(TA_R21_BRANCH_CHAINS) && TA_R21_BRANCH_CHAINS
bool ta_native_r21_has_branch_next(uint32_t pc);
bool ta_native_r21_allow_branch_next(uint32_t from,uint32_t to);
void ta_native_r21_record_dynamic_link(void);
uint32_t ta_native_r21_dynamic_links(void);
#if defined(TA_R24_LOOP_BACKEDGE) && TA_R24_LOOP_BACKEDGE && defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
void ta_native_r24_record_loop_link(void);
uint32_t ta_native_r24_loop_links(void);
#endif
#endif
/* R22: cls 28 JSL long, 29 JMP absolute, 30 PEA immediate.
   cls 26 CMP absolute, cls 27 LDA absolute-long (R20 optional); cls 0..25: INX INY DEX CLC DEY SEC TAX TAY TXY REP BIT TXA TYA SEP BNE BEQ PHP PLP PLB RTL LDA_ABS AND_IMM CMP_IMM PHB ASLA BRA. */
#if defined(TA_R23_AE_LOOP) && TA_R23_AE_LOOP
uint32_t ta_native_r23_lda_longx_hits(void);
uint32_t ta_native_r23_adc_dp_hits(void);
uint32_t ta_native_r23_sta_dp_hits(void);
#endif
uint32_t ta_native_opcode_hits(unsigned cls);
#ifdef __cplusplus
}
#endif
#endif
