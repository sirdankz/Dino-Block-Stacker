#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <ppcintrinsics.h>
#ifndef __attribute__
#define __attribute__(x)
#endif
#ifndef __restrict__
#define __restrict__ __restrict
#endif
#ifndef restrict
#define restrict __restrict
#endif
#ifndef __func__
#define __func__ __FUNCTION__
#endif
#define __builtin_expect(value,expected) (value)
static unsigned ta360_ctz(unsigned value){if(!value)return 32;unsigned n=0;while(!(value&1u)){value>>=1;++n;}return n;}
#define __builtin_ctz(value) ta360_ctz((unsigned)(value))
#define __builtin_readcyclecounter() __mftb()
#define __builtin_bswap32(value) _byteswap_ulong(value)
#define __builtin_bswap64(value) _byteswap_uint64(value)
static int ta360_snprintf(char*out,size_t cap,const char*fmt,...){
 va_list args;va_start(args,fmt);int n=_vsnprintf(out,cap,fmt,args);va_end(args);if(cap)out[cap-1]=0;return n;
}
#define snprintf ta360_snprintf
/* C sources compile as C++98 on the installed PowerPC compiler. These typed
   conversion results preserve C allocation semantics without editing the core. */
struct Ta360Allocation {
 void*value;explicit Ta360Allocation(void*p):value(p){}
 template<class T> operator T*() const{return static_cast<T*>(value);}
};
static Ta360Allocation ta360_malloc(size_t n){return Ta360Allocation(malloc(n));}
static Ta360Allocation ta360_calloc(size_t n,size_t size){return Ta360Allocation(calloc(n,size));}
static Ta360Allocation ta360_realloc(void*p,size_t n){return Ta360Allocation(realloc(p,n));}
#ifdef TA_360_C_TRANSLATION
#define malloc(n) ta360_malloc(n)
#define calloc(n,s) ta360_calloc(n,s)
#define realloc(p,n) ta360_realloc(p,n)
#endif
