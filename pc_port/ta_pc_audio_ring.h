#pragma once
#include <atomic>
#include <stdint.h>
#include <string.h>
/* One frame producer and one device consumer. Neither waits on the other. */
struct PcPcmRing {
 enum {Capacity=16,Frames=800,Samples=1600};
 struct Block{uint32_t command;int16_t pcm[Samples];}blocks[Capacity];
 alignas(64) std::atomic<uint32_t> written{0};
 alignas(64) std::atomic<uint32_t> read{0};
 void reset(){written.store(0);read.store(0);}
 bool push(uint32_t command,const int16_t*pcm){uint32_t w=written.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);if(w-r>=Capacity)return false;auto&b=blocks[w%Capacity];b.command=command;memcpy(b.pcm,pcm,sizeof b.pcm);written.store(w+1,std::memory_order_release);return true;}
 void discardBefore(uint32_t command){uint32_t r=read.load(std::memory_order_relaxed),w=written.load(std::memory_order_acquire);while(r!=w&&(int32_t)(blocks[r%Capacity].command-command)<0)++r;read.store(r,std::memory_order_release);}
 unsigned queued(uint32_t command){discardBefore(command);uint32_t r=read.load(std::memory_order_relaxed),w=written.load(std::memory_order_acquire),n=0;while(r+n!=w&&blocks[(r+n)%Capacity].command==command)++n;return n;}
 bool pop(uint32_t command,int16_t*out){discardBefore(command);uint32_t r=read.load(std::memory_order_relaxed),w=written.load(std::memory_order_acquire);if(r==w||blocks[r%Capacity].command!=command)return false;memcpy(out,blocks[r%Capacity].pcm,sizeof blocks[0].pcm);read.store(r+1,std::memory_order_release);return true;}
};
