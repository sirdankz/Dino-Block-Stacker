/* R19 optional host-only execution-evidence collector. Not compiled for Xbox. */
#ifndef TA_EVIDENCE_H
#define TA_EVIDENCE_H
#include <stdint.h>
#include <stddef.h>
#include "cpu.h"
#include "snes.h"
void ta_evidence_init(const uint8_t *rom, size_t bytes);
void ta_evidence_opcode(const Cpu *cpu);
void ta_evidence_write(Snes *s, uint32_t adr, uint8_t val, uint64_t cpuClock, void *ctx);
int ta_evidence_export(const char *stem);
#endif
