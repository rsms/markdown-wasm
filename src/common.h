#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
// #include <emscripten/emscripten.h>

#ifdef _MSC_VER
  #ifndef __cplusplus
    #define inline __inline
  #endif
#endif

typedef uint8_t  bool;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef int16_t  i16;
typedef uint32_t u32;
typedef int32_t  i32;

#ifndef TRUE
  #define TRUE 1
#endif
#ifndef FALSE
  #define FALSE 0
#endif

#define true TRUE
#define false FALSE

#ifndef static_assert
  #if __has_feature(c_static_assert)
    #define static_assert _Static_assert
  #else
    #define static_assert(cond, msg) ((void*)0)
  #endif
#endif

#define max(a,b) \
  ({__typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

#define min(a,b) \
  ({__typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define countof(x) \
  ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#ifndef DEBUG
  #define DEBUG 0
#endif
#if DEBUG
#include <stdio.h>
  #define dlog(fmt, ...)  printf(fmt "\n", ##__VA_ARGS__)
#else
  #define dlog(...)
#endif /* DEBUG > 0 */

#include "wbuf.h"

typedef int(*JSTextFilterFun)(
  const char*  metaptr, u32 metalen,
  const char*  inptr,   u32 inlen,
  const char** outptrp); // return outlen
