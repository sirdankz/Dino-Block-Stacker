#pragma once
#include <stdint.h>
struct PcFrameClock {
 uint64_t deadline=0;
 uint64_t wait(uint64_t now){
  const uint64_t step=1000000000ull/60;
  if(!deadline){deadline=now+step;return step;}
  deadline+=step;
  if(now>=deadline){if(now-deadline>=step)deadline=now;return 0;}
  return deadline-now;
 }
};
