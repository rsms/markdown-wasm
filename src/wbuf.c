#include "common.h"

void WBufInit(WBuf* b) {
  b->start = 0;
  b->end = 0;
  b->ptr = 0;
}

void WBufFree(WBuf* b) {
  free(b->start);
}

void WBufReset(WBuf* b) {
  b->end = b->start;
  b->ptr = b->start;
}

inline size_t WBufCap(WBuf* b) { return b->end - b->start; } // total capacity (size)
inline size_t WBufLen(WBuf* b) { return b->ptr - b->start; } // valid bytes at start
inline size_t WBufAvail(WBuf* b) { return b->end - b->ptr; } // bytes available

// grows buffer so that there is at least minspace available space
static void WBufGrow(WBuf* b, size_t minspace) {
  // size_t avail = b->end - b->ptr;
  size_t len = WBufLen(b); // store len before changing b
  size_t cap = WBufCap(b);
  do {
    if (cap == 0) {
      cap = 512;
    } else {
      cap *= 2;
    }
  } while (cap - len < minspace);
  b->start = realloc(b->start, cap);
  b->end = b->start + cap;
  b->ptr = b->start + len;
}

void WBufReserve(WBuf* b, size_t minspace) {
  if (WBufAvail(b) < minspace) {
    WBufGrow(b, minspace);
  }
}

void WBufAppendc(WBuf* b, char c) {
  if (WBufAvail(b) < 1) {
    WBufGrow(b, 1);
  }
  *(b->ptr++) = c;
}

void WBufAppendBytes(WBuf* b, const void* bytes, size_t len) {
  if (WBufAvail(b) < len) {
    WBufGrow(b, len);
  }
  memcpy(b->ptr, bytes, len);
  b->ptr += len;
}

void WBufAppendStr(WBuf* b, const char* pch) {
  WBufAppendBytes(b, pch, strlen(pch));
}

void _WBufAppendHtml(WBuf* b, const char* pch, bool isattr) {
  u32 len = strlen(pch);
  if (WBufAvail(b) < len) {
    WBufGrow(b, len);
  }

  while_loop:
  while (*pch) {
    u32 slen = 0;
    const char* s = NULL;
    #define S(cstr) s = cstr; slen = strlen(cstr); break;
    switch (*pch) {
      case '&': S("&amp;")
      case '<': S("&lt;")
      case '>': S("&gt;")
      case '"': if (isattr) S("&quot;")  // must be last since fallthrough
      default:
        *(b->ptr++) = *pch;
        pch++;
        goto while_loop;
    }
    #undef S
    WBufAppendBytes(b, s, slen);
    pch++;
  }
}


static inline size_t fmtu32(u32 n, u32 radix, char* buf) {
  if (n == 0) {
    *buf = '0';
    return 1;
  }

  // longest possible string is 32 bytes
  // (0xFFFFFFFF, 4294967295, 11111111111111111111111111111111)
  const u32 Z = 32;
  char* p = buf + Z;

  if (radix < 11) {
    if (radix > 1) {
      // 0-9
      for (; n; n /= radix) {
        *--p = '0' + n % radix;
      }
    }
  } else if (radix < 37) {
    // 0-9, A-Z
    u32 c = 0;
    for (; n; n /= radix) {
      c = n % radix;
      if (c > 9) {
        c += 7;
      }
      *--p = '0' + c;
    }
  }

  size_t len = Z - (p - buf);

  // Shift buffer contents to the beginning
  //
  // len=4  Z=7
  //   ___ABCD  i=3 'A' o=0  A__ABCD
  //   A__ABCD  i=4 'B' o=1  AB_ABCD
  //   AB_ABCD  i=5 'C' o=2  ABCABCD
  //   ABCABCD  i=6 'D' o=3  ABCDBCD
  //
  // len=5  Z=7
  //   _ABCDEF  i=1 'A' o=0  AABCDEF
  //   AABCDEF  i=2 'B' o=1  ABBCDEF
  //   ABBCDEF  i=3 'C' o=2  ABCCDEF
  //   ABCCDEF  i=4 'D' o=3  ABCDDEF
  //   ABCDDEF  i=5 'E' o=4  ABCDEEF
  //   ABCDEEF  i=6 'F' o=5  ABCDEFF
  //
  if (len <= Z/2) {
    // move short strings in 4-byte chunks
    u32* inp = (u32*)p;
    u32* out = (u32*)buf;
    const u32* end = (u32*)(buf + Z);
    while (inp < end) {
      *out++ = *inp++;
    }
  } else if (len < Z) {
    const char* end = buf + Z;
    while (p < end) {
      *buf++ = *p++;
    }
  }

  return len;
}


void WBufAppendU32(WBuf* b, u32 n, u32 radix) {
  WBufReserve(b, 32);
  b->ptr += fmtu32(n, radix, b->ptr);
}


// static void WBufAppendSlug(WBuf* b, const char* text) {
//   size_t len = strlen(text);
//   WBufReserve(b, len);
//   while (*text) {
//     char c = *text++;
//     if ((c >= '0' && c <= '9') ||
//         (c >= 'a' && c <= 'z') ||
//         c == '.' ||
//         c == '-'
//     ) {
//       *(b->ptr++) = c;
//     } else if ((c >= '0' && c <= '9') ||
//         (c >= 'a' && c <= 'z') ||
//         (c >= 'A' && c <= 'Z') ||
//         c == '.' ||
//         c == '-'
//     ) {
//       *(b->ptr++) = tolower(c);
//     } else if (c == ' ') {
//       *(b->ptr++) = '-';
//     } // else: ignore
//   }
// }
