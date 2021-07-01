#pragma once

typedef struct WBuf_s {
  char* start; // pointer to start of data
  char* end;   // pointer to end of data
  char* ptr;   // pointer to end of valid bytes (cursor)
} WBuf;

void WBufInit(WBuf*);
void WBufFree(WBuf*);
void WBufReset(WBuf*);

size_t WBufCap(WBuf*);   // total capacity (size)
size_t WBufLen(WBuf*);   // valid bytes at start
size_t WBufAvail(WBuf*); // bytes available

void WBufReserve(WBuf*, size_t minspace);

void WBufAppendc(WBuf*, char c);
void WBufAppendBytes(WBuf*, const void* bytes, size_t len);
void WBufAppendStr(WBuf*, const char* pch);
#define WBufAppendCStr(b, cstr) WBufAppendBytes((b), (cstr), strlen(cstr))
#define WBufAppendHtml(b, pch) _WBufAppendHtml(b, pch, false)
#define WBufAppendHtmlAttr(b, pch) _WBufAppendHtml(b, pch, true)
void _WBufAppendHtml(WBuf*, const char* pch, bool isattr);

// append u32 integer n. radix must be in range [2-36]
void WBufAppendU32(WBuf*, u32 n, u32 radix);
