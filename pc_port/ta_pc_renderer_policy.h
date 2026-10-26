#pragma once
#include <stdint.h>
/* Only graphics costs count. Scene loading and frame-limiter waits do not. */
struct PcRendererPolicy {
 uint64_t graceUntil=0,firstStall=0;
 unsigned stalls=0;bool attempted=false;
 void settle(uint64_t now){graceUntil=now+2000000000ull;firstStall=0;stalls=0;}
 bool sample(uint64_t now,uint64_t graphics,bool eligible){
  if(attempted)return false;
  if(!eligible){settle(now);return false;}
  if(now<graceUntil)return false;
  if(stalls&&now-firstStall>60000000000ull){firstStall=0;stalls=0;}
  if(graphics<50000000ull)return false;
  if(!stalls)firstStall=now;
  if(++stalls<3)return false;
  attempted=true;return true;
 }
};
