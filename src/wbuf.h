#pragma once
#include "common.h"

typedef struct WBuf_s {
  char* start; // pointer to start of data
  char* end;   // pointer to end of data
  char* ptr;   // pointer to end of valid bytes (cursor)
} WBuf;

void WBufInit(WBuf*);
void WBufFree(WBuf*);
void WBufReset(WBuf*);

inline static size_t WBufCap(WBuf* b) { return b->end - b->start; } // total capacity (size)
inline static size_t WBufLen(WBuf* b) { return b->ptr - b->start; } // valid bytes at start
inline static size_t WBufAvail(WBuf* b) { return b->end - b->ptr; } // bytes available

void WBufReserve(WBuf*, size_t minspace);

static void WBufAppendc(WBuf*, char c);
void WBufAppendBytes(WBuf*, const void* bytes, size_t len);
void WBufAppendStr(WBuf*, const char* pch);
#define WBufAppendCStr(b, cstr) WBufAppendBytes((b), (cstr), strlen(cstr))
#define WBufAppendHtml(b, pch) _WBufAppendHtml(b, pch, false)
#define WBufAppendHtmlAttr(b, pch) _WBufAppendHtml(b, pch, true)
void _WBufAppendHtml(WBuf*, const char* pch, bool isattr);

// append u32 integer n. radix must be in range [2-36]
void WBufAppendU32(WBuf*, u32 n, u32 radix);

static void WBufAppendUTF8Codepoint(WBuf* b, u32 codepoint);

// grows buffer so that there is at least minspace available space
void WBufGrow(WBuf* b, size_t minspace);



// implementation of WBufAppendUTF8Codepoint
void _WBufAppendUTF8Codepoint2(WBuf* b, u32 codepoint);
inline static void WBufAppendUTF8Codepoint(WBuf* b, u32 codepoint) {
  if (codepoint > 0x7f) {
    return _WBufAppendUTF8Codepoint2(b, codepoint);
  }
  WBufAppendc(b, (char)codepoint);
}

inline static void WBufAppendc(WBuf* b, char c) {
  if (WBufAvail(b) < 1) {
    WBufGrow(b, 1);
  }
  *(b->ptr++) = c;
}
