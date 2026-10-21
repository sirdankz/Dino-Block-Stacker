
#ifndef SPC_H
#define SPC_H

#include <stdint.h>
#include <stdbool.h>

#include "statehandler.h"

typedef uint8_t (*SpcReadHandler)(void* mem, uint16_t adr);
typedef void (*SpcWriteHandler)(void* mem, uint16_t adr, uint8_t val);
typedef void (*SpcIdleHandler)(void* mem, bool waiting);

typedef struct Spc Spc;

struct Spc {
  // reference to memory handler, pointers to read/write/idle handlers
  void* mem;
  SpcReadHandler read;
  SpcWriteHandler write;
  SpcIdleHandler idle;
  // registers
  uint8_t a;
  uint8_t x;
  uint8_t y;
  uint8_t sp;
  uint16_t pc;
  // flags
  bool c;
  bool z;
  bool v;
  bool n;
  bool i;
  bool h;
  bool p;
  bool b;
  // stopping
  bool stopped;
  // reset
  bool resetWanted;
};

Spc* spc_init(void* mem, SpcReadHandler read, SpcWriteHandler write, SpcIdleHandler idle);
#if defined(XBOX_PORT) && defined(TA_R59_SPC_BLOCKS) && TA_R59_SPC_BLOCKS && \
    defined(TA_R59_SPC_RUN) && TA_R59_SPC_RUN && \
    defined(TA_R47_FAST_REJECT) && TA_R47_FAST_REJECT && !TA_R32_DIAGNOSTICS
#define TA_R59_SPC_BLOCKS_ON 1
bool spc_r59_nativeBlock(Spc* spc,unsigned remaining);
bool spc_r59_echoOverlap(uint16_t echo,uint16_t begin,unsigned length);
#if defined(TA_R59_HOST_COVERAGE) && TA_R59_HOST_COVERAGE
extern uint64_t ta_r59HostSpcBlocks[2];
#endif
#else
#define TA_R59_SPC_BLOCKS_ON 0
#endif
void spc_free(Spc* spc);
void spc_reset(Spc* spc, bool hard);
void spc_handleState(Spc* spc, StateHandler* sh);
void spc_runOpcode(Spc* spc);
#if defined(XBOX_PORT) && defined(TA_R59_SPC_RUN) && TA_R59_SPC_RUN && !TA_R32_DIAGNOSTICS
#define TA_R59_SPC_RUN_ON 1
int spc_runCycles(Spc* spc,int wantedCycles);
#else
#define TA_R59_SPC_RUN_ON 0
#endif

#endif
