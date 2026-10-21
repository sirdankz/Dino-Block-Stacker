#ifndef TA_R126_GPU_VECTORS_H
#define TA_R126_GPU_VECTORS_H
#include <stdint.h>
/* First half covers every pair in every channel, with independent mixed
 * channels. Second half covers all 32768 five-bit RGB colors without math. */
static inline void ta_r126_gpu_vector(unsigned i,uint32_t*main,uint32_t*sub){
 unsigned n=i&32767u,r=n&31u,g=(n>>5)&31u,b=(n>>10)&31u;
 *main=(r<<19)|(g<<11)|(b<<3);
 *sub=i<32768u?((g<<19)|(b<<11)|(r<<3)):0u;
}
static inline uint32_t ta_r126_gpu_reference(unsigned i){
 uint32_t a,b,result=0;ta_r126_gpu_vector(i,&a,&b);
 for(unsigned shift=0;shift<=16;shift+=8){unsigned x=(a>>(shift+3))&31u,y=(b>>(shift+3))&31u,v=x>y?x-y:0u;result|=((v<<3)|(v>>2))<<shift;}
 return result;
}
#endif
