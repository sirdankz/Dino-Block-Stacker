/* R82 diagnostic-only nested host cost and whole game-operation census. */
#ifndef TA_R82_COST_H
#define TA_R82_COST_H
#include <stdint.h>
#include <stdbool.h>
#ifndef TA_R82_COST_PROFILE
#define TA_R82_COST_PROFILE 0
#endif
#define TA_R82_COST_ON TA_R82_COST_PROFILE
#ifndef TA_R83_RESIDUAL_PROFILE
#define TA_R83_RESIDUAL_PROFILE 0
#endif
#ifndef TA_R84_COMPOSE_PROFILE
#define TA_R84_COMPOSE_PROFILE 0
#endif
#ifndef TA_R87_CACHE_MAP
#define TA_R87_CACHE_MAP 0
#endif
#if TA_R87_CACHE_MAP && (!TA_R84_COMPOSE_PROFILE || !TA_R83_RESIDUAL_PROFILE)
#error R87 cache map requires R82/R83/R84 nested timing and CPU dispatch observer
#endif
#if TA_R84_COMPOSE_PROFILE && !TA_R83_RESIDUAL_PROFILE
#error R84 composition profiling needs the R83 residual observer
#endif
#if TA_R83_RESIDUAL_PROFILE && !TA_R82_COST_ON
#error R83 residual profiling needs the R82 nested cost observer
#endif
#if TA_R82_COST_ON
#ifdef __cplusplus
extern "C" {
#endif
struct Snes;struct Cpu;
enum {TA82_OTHER,TA82_CPU,TA82_PPU,TA82_BG,TA82_SPRITE_EVAL,TA82_SPRITE_COMP,TA82_APU,TA82_DSP,TA82_DMA,TA82_OBSERVER,
#if TA_R83_RESIDUAL_PROFILE
TA82_NATIVE_COMPOSE,TA82_GENERIC_PIXELS,
#endif
#if TA_R84_COMPOSE_PROFILE
TA84_SETUP,TA84_CACHE,TA84_DIRECT,TA84_FIXED,TA84_SUB,TA84_WINDOW,TA84_SCALAR,TA84_BLANK,
#endif
#if TA_R87_CACHE_MAP
TA87_KEY,TA87_DEP,TA87_RESTORE,TA87_MISS,TA87_STORE,
#endif
TA82_KINDS};
enum {TA82_UNITS=14,TA82_CALLBACKS=128,TA82_PAGES=1024};
typedef struct {uint64_t start,closed;unsigned kind;bool active;} TaR82Token;
typedef struct {uint64_t ticks[TA82_KINDS],calls[TA82_KINDS],frames,rootTicks;} TaR82ModeCost;
extern bool ta_r82_active;
TaR82Token ta_r82_scope_begin(unsigned kind);
uint64_t ta_r82_scope_finish(TaR82Token* token);
static inline void ta_r82_scope_cleanup(TaR82Token* token){(void)ta_r82_scope_finish(token);}
#define TA_R82_SCOPE(kind) TaR82Token ta82_scope_token __attribute__((cleanup(ta_r82_scope_cleanup)))=ta_r82_scope_begin(kind)
#if TA_R84_COMPOSE_PROFILE
#define TA_R84_SCOPE(name,kind) TaR82Token name __attribute__((cleanup(ta_r82_scope_cleanup)))=ta_r82_scope_begin(kind)
#define TA_R84_FINISH(name) ((void)ta_r82_scope_finish(&(name)))
#else
#define TA_R84_SCOPE(name,kind)
#define TA_R84_FINISH(name) ((void)0)
#endif
#if TA_R87_CACHE_MAP
enum { TA87_EVENT_KEY_HIT,TA87_EVENT_KEY_MISS,TA87_EVENT_STAMP_BYPASS,
       TA87_EVENT_GENERATION_SCAN,TA87_EVENT_GENERATION_FAIL,
       TA87_EVENT_BG_RESTORE,TA87_EVENT_BG_STORE,TA87_EVENTS };
void ta_r87_cache_event(unsigned event);
#define TA_R87_SCOPE(name,kind) TaR82Token name __attribute__((cleanup(ta_r82_scope_cleanup)))=ta_r82_scope_begin(kind)
#define TA_R87_FINISH(name) ((void)ta_r82_scope_finish(&(name)))
#else
#define TA_R87_SCOPE(name,kind)
#define TA_R87_FINISH(name) ((void)0)
#define ta_r87_cache_event(event) ((void)0)
#endif
void ta_r82_reset(void);
void ta_r82_frame_begin(struct Snes* snes);
void ta_r82_frame_end(struct Snes* snes);
void ta_r82_cpu_step(struct Cpu* cpu,bool expectedVblank,uint32_t expectedFrame);
void ta_r82_get_mode(unsigned mode,TaR82ModeCost* result);
void ta_r82_report(void (*emit)(const char*),uint32_t endFrame);
#if TA_R83_RESIDUAL_PROFILE
/* Sampled-only host diagnostics, never part of guest CPU/PPU state. */
void ta_r83_cpu_native_done(void);
void ta_r83_service_attempt(bool accepted);
#endif
#ifdef __cplusplus
}
#endif
#else
#define TA_R82_SCOPE(kind)
#define TA_R84_SCOPE(name,kind)
#define TA_R84_FINISH(name) ((void)0)
#define TA_R87_SCOPE(name,kind)
#define TA_R87_FINISH(name) ((void)0)
#define ta_r87_cache_event(event) ((void)0)
#endif
#endif
