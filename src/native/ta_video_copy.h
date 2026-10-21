#ifndef TA_VIDEO_COPY_H
#define TA_VIDEO_COPY_H
#include <stdint.h>
#include <string.h>
/* SSE1 only. Copy full 64-byte groups to aligned linear texture memory.
   Caller must fence once after all streaming rows, before GPU submission. */
static inline int ta_video_copy_row(uint8_t* dst,const uint8_t* src,unsigned bytes) {
#if defined(TA_R37_STREAM_UPLOAD) && TA_R37_STREAM_UPLOAD
  if(bytes && !(bytes&63u) && !((uintptr_t)dst&15u)) {
    __asm__ volatile(
      "1:\n\t"
      "movups 0(%[src]), %%xmm0\n\t"
      "movups 16(%[src]), %%xmm1\n\t"
      "movups 32(%[src]), %%xmm2\n\t"
      "movups 48(%[src]), %%xmm3\n\t"
      "movntps %%xmm0, 0(%[dst])\n\t"
      "movntps %%xmm1, 16(%[dst])\n\t"
      "movntps %%xmm2, 32(%[dst])\n\t"
      "movntps %%xmm3, 48(%[dst])\n\t"
      "add $64, %[src]\n\t"
      "add $64, %[dst]\n\t"
      "sub $64, %[bytes]\n\t"
      "jnz 1b\n\t"
      : [src] "+r"(src),[dst] "+r"(dst),[bytes] "+r"(bytes)
      : : "xmm0","xmm1","xmm2","xmm3","cc","memory");
    return 1;
  }
#endif
  memcpy(dst,src,bytes);
  return 0;
}
static inline void ta_video_copy_fence(int streamed) {
#if defined(TA_R37_STREAM_UPLOAD) && TA_R37_STREAM_UPLOAD
  if(streamed)__asm__ volatile("sfence" : : : "memory");
#else
  (void)streamed;
#endif
}
#endif
