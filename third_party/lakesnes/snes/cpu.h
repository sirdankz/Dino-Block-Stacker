
#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>

#include "statehandler.h"

typedef uint8_t (*CpuReadHandler)(void* mem, uint32_t adr);
typedef void (*CpuWriteHandler)(void* mem, uint32_t adr, uint8_t val);
typedef void (*CpuIdleHandler)(void* mem, bool waiting);

typedef struct Cpu Cpu;

struct Cpu {
  // reference to memory handler, pointers to read/write/idle handlers
  void* mem;
  CpuReadHandler read;
  CpuWriteHandler write;
  CpuIdleHandler idle;
  // registers
  uint16_t a;
  uint16_t x;
  uint16_t y;
  uint16_t sp;
  uint16_t pc;
  uint16_t dp; // direct page (D)
  uint8_t k; // program bank (PB)
  uint8_t db; // data bank (B)
  // flags
  bool c;
  bool z;
  bool v;
  bool n;
  bool i;
  bool d;
  bool xf;
  bool mf;
  bool e;
  // power state (WAI/STP)
  bool waiting;
  bool stopped;
  // interrupts
  bool irqWanted;
  bool nmiWanted;
  bool intWanted;
  bool resetWanted;
};

Cpu* cpu_init(void* mem, CpuReadHandler read, CpuWriteHandler write, CpuIdleHandler idle);
void cpu_free(Cpu* cpu);
void cpu_reset(Cpu* cpu, bool hard);
void cpu_handleState(Cpu* cpu, StateHandler* sh);
void cpu_runOpcode(Cpu* cpu);
void cpu_runFrameOpcode(Cpu* cpu, bool expectedVblank, uint32_t expectedFrame);
#if defined(XBOX_PORT) && defined(TA_R59_CPU_FRAME) && TA_R59_CPU_FRAME && \
    defined(TA_R46_CPU_DISPATCH) && TA_R46_CPU_DISPATCH && \
    defined(TA_NATIVE_ENABLED) && TA_NATIVE_ENABLED && \
    defined(TA_R10_MICROBLOCKS) && TA_R10_MICROBLOCKS && \
    defined(TA_R40_DECODED_DISPATCH) && TA_R40_DECODED_DISPATCH && \
    defined(TA_R15_NATIVE_LOOKUP) && TA_R15_NATIVE_LOOKUP
#define TA_R59_CPU_FRAME_ON 1
void cpu_runFrameBody(Cpu* cpu);
#else
#define TA_R59_CPU_FRAME_ON 0
#endif
void cpu_nmi(Cpu* cpu);
void cpu_setIrq(Cpu* cpu, bool state);

#endif
