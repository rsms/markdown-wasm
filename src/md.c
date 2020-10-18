#include <ctype.h>
#include "common.h"
#include "wlib.h"
#include "wbuf.h"
#include "fmt_html.h"

#if MD_WITH_JSON
#include "fmt_json.h"
#endif

// these should be in sync with "OutputFlags" in md.js
typedef enum Formatter {
  FormatterNONE,
  FormatterHTML,
  FormatterJSON,
} Formatter;

typedef enum FormatFlags {
  FormatFlagHTML  = 1 << 0,
  FormatFlagXHTML = 1 << 1,
  FormatFlagJSON  = 1 << 2,
} FormatFlags;

typedef enum ErrorCode {
  ERR_NONE,
  ERR_MD_PARSE,
  ERR_FORMAT,
} ErrorCode;


#if DEBUG
void __attribute__((constructor)) init() {
  dlog("WASM INIT\n");
}
#endif


// Shared, reusable output buffer.
// Must make sure to never use this across calls from WASM host.
static WBuf outbuf;


export size_t parseUTF8(
  const char*  inbufptr,
  u32          inbuflen,
  u32          parseFlags,
  Formatter    formatter,
  u32          fmtflags,
  const char** outptr
) {
  dlog("parseUTF8 called with inbufptr=%p  inbuflen=%u\n", inbufptr, inbuflen);

  WBufReset(&outbuf);
  WBufReserve(&outbuf, inbuflen * 2);  // approximate output size to minimize reallocations

  int result = 0x7ffff;

  switch (formatter) {

  case FormatterHTML:
    result = fmt_html(inbufptr, inbuflen, &outbuf, parseFlags, fmtflags);
    break;

  case FormatterJSON:
    #if MD_WITH_JSON
    result = fmt_json(inbufptr, inbuflen, &outbuf, parseFlags, fmtflags);
    #endif
    break;

  case FormatterNONE:
    break;

  } // switch

  if (result == 0x7ffff) {
    WErrSet(ERR_FORMAT, "invalid formatter");
  } else if (result != 0) {
    // fmt_html returns status of md_parse which only fails in extreme cases
    // like when out of memory. md4c does not provide error codes or error messages.
    WErrSet(ERR_MD_PARSE, "parser error");
  }

  if (result == 0) {
    *outptr = outbuf.start;
    // dlog("outbuf =>\n%.*s\n", WBufLen(&outbuf), outbuf.start);
    return WBufLen(&outbuf);
  }

  *outptr = 0;
  return 0;
}
