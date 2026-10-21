/* REA-method-inspired (NOT dependent on REA/MCP) bounded host evidence.
 * Observed CPU edges, NOT statically-proven routine boundaries. A CPU
 * writeHook observes CPU writes only, NOT DMA writes. No ROM bytes emitted.
 * This file is absent from RXDK sources and only linked by the host script.
 */
#include "ta_evidence.h"
#include <stdio.h>
#include <string.h>

#define EDGE_CAP 16384u
typedef struct { uint32_t from, to, hits; uint8_t kind, pad[3]; } Edge;
static Edge edges[EDGE_CAP];
static uint32_t edgeOverflows, cpuEvents;
static uint32_t wramWrites[512]; /* canonical 7E:0000–7F:FFFF / 256 */
static uint32_t ioPpuWrites[256], ioCpuWrites[256], ioDmaWrites[256];
static uint32_t lastPc;
static uint8_t lastOpcode;
static int haveLast;
static const uint8_t *romPtr;
static size_t romSize;

static uint8_t rom_opcode(uint32_t adr) {
  if (!romPtr || romSize != 1048576u || (adr & 0x8000u)==0) return 0xff;
  uint32_t offset = ((((adr >> 16) & 0x7fu) << 15) | (adr & 0x7fffu)) & 0xfffffu;
  return romPtr[offset];
}
/* Calls/returns and taken-or-not-taken branch successors as *observed*.
   Does not guess the purpose of a function or invent its owner. */
static uint8_t edge_kind(uint8_t op) {
  if(op==0x20 || op==0x22 || op==0xfc)return 1; /* JSR/JSL */
  if(op==0x60 || op==0x6b || op==0x40)return 2; /* RTS/RTL/RTI */
  if(op==0x4c || op==0x5c || op==0x6c || op==0x7c || op==0xdc)return 3; /* JMP/JML */
  switch(op){case 0x80:case 0x82:case 0xd0:case 0xf0:case 0x90:case 0xb0:
    case 0x30:case 0x10:case 0x50:case 0x70:return 4;}
  return 0;
}
void ta_evidence_init(const uint8_t *rom, size_t bytes) {
  memset(edges,0,sizeof(edges));memset(wramWrites,0,sizeof(wramWrites));
  memset(ioPpuWrites,0,sizeof(ioPpuWrites));memset(ioCpuWrites,0,sizeof(ioCpuWrites));
  memset(ioDmaWrites,0,sizeof(ioDmaWrites));
  romPtr=rom;romSize=bytes;haveLast=0;edgeOverflows=cpuEvents=0;
}
void ta_evidence_opcode(const Cpu *cpu) {
  const uint32_t now=((uint32_t)cpu->k<<16)|cpu->pc;
  if(haveLast){
    uint8_t kind=edge_kind(lastOpcode);
    if(kind){
      uint32_t slot=(lastPc*16777619u ^ now*2654435761u ^ kind) & (EDGE_CAP-1);
      for(unsigned i=0;i<16;i++){
        Edge *e=&edges[(slot+i)&(EDGE_CAP-1)];
        if(!e->hits || (e->from==lastPc && e->to==now && e->kind==kind)){
          if(!e->hits){e->from=lastPc;e->to=now;e->kind=kind;}
          ++e->hits;break;
        }
        if(i==15)++edgeOverflows;
      }
    }
  }
  lastOpcode=rom_opcode(now);lastPc=now;haveLast=1;++cpuEvents;
}
void ta_evidence_write(Snes *s, uint32_t a, uint8_t v, uint64_t cycle, void *ctx){
  (void)s;(void)v;(void)cycle;(void)ctx;
  const uint8_t bank=(uint8_t)(a>>16);
  const uint16_t off=(uint16_t)a;
  if(bank==0x7e || bank==0x7f)++wramWrites[(((uint32_t)bank-0x7eu)<<8)|(off>>8)];
  else if((bank<0x40 || (bank>=0x80 && bank<0xc0)) && off<0x2000)++wramWrites[off>>8];
  else if((bank<0x40 || (bank>=0x80 && bank<0xc0))){
    if(off>=0x2100 && off<0x2200)++ioPpuWrites[off&255u];
    else if(off>=0x4200 && off<0x4300)++ioCpuWrites[off&255u];
    else if(off>=0x4300 && off<0x4400)++ioDmaWrites[off&255u];
  }
}
static const char *edge_name(uint8_t kind){
  switch(kind){case 1:return "call";case 2:return "return";case 3:return "jump";case 4:return "branch";default:return "unknown";}
}
int ta_evidence_export(const char *stem){
  char path[512];
  snprintf(path,sizeof(path),"%s_edges.csv",stem);FILE *f=fopen(path,"wb");if(!f)return 0;
  fputs("from_bank,from_pc,to_bank,to_pc,kind,observations\n",f);
  for(unsigned i=0;i<EDGE_CAP;++i){Edge *e=&edges[i];if(e->hits)
    fprintf(f,"%02X,%04X,%02X,%04X,%s,%u\n",(unsigned)(e->from>>16),(unsigned)(e->from&0xffffu),
      (unsigned)(e->to>>16),(unsigned)(e->to&0xffffu),edge_name(e->kind),e->hits);}
  fclose(f);
  snprintf(path,sizeof(path),"%s_wram.csv",stem);f=fopen(path,"wb");if(!f)return 0;
  fputs("wram_bank,page_start,cpu_write_observations\n",f);
  for(unsigned i=0;i<512;i++)if(wramWrites[i])fprintf(f,"%02X,%04X,%u\n",0x7e + (i>>8),(i&255u)<<8,wramWrites[i]);
  fclose(f);
  snprintf(path,sizeof(path),"%s_io.csv",stem);f=fopen(path,"wb");if(!f)return 0;
  fputs("register,classification,cpu_write_observations\n",f);
  for(unsigned i=0;i<256;i++){
    if(ioPpuWrites[i])fprintf(f,"%04X,ppu,%u\n",0x2100+i,ioPpuWrites[i]);
    if(ioCpuWrites[i])fprintf(f,"%04X,cpu_control,%u\n",0x4200+i,ioCpuWrites[i]);
    if(ioDmaWrites[i])fprintf(f,"%04X,dma_register,%u\n",0x4300+i,ioDmaWrites[i]);}
  fclose(f);
  snprintf(path,sizeof(path),"%s_summary.txt",stem);f=fopen(path,"wb");if(!f)return 0;
  fprintf(f,"method=REA-inspired offline source instrumentation; no REA/MCP execution\nrom_bytes=%lu\nopcode_observations=%u\nedge_hash_overflows=%u\n",
      (unsigned long)romSize,cpuEvents,edgeOverflows);
  fputs("limitations=observed edge successors only; ambiguous labels; hooks omit DMA transfers; opcode map ignores addresses outside LoROM high halves; results cover only replayed inputs\n",f);
  fclose(f);return 1;
}
