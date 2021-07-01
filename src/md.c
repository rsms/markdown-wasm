#include <ctype.h>
#include "common.h"
#include "wlib.h"
#include "fmt_html.h"
// #include "fmt_json.h"

// these should be in sync with "OutputFlags" in md.js
typedef enum OutputFlags {
  OutputFlagHTML  = 1 << 0,
  OutputFlagXHTML = 1 << 1,
} OutputFlags;

typedef enum ErrorCode {
  ERR_NONE,
  ERR_MD_PARSE,
  ERR_OUTFLAGS,
} ErrorCode;


#if DEBUG
void __attribute__((constructor)) init() {
  dlog("WASM INIT");
}
#endif


// Shared, reusable output buffer.
// Must make sure to never use this across calls from WASM host.
static WBuf outbuf;


export size_t parseUTF8(
  const char* inbufptr,
  u32 inbuflen,
  u32 parser_flags,
  OutputFlags outflags,
  const char** outptr,
  JSTextFilterFun onCodeBlock
) {
  dlog("parseUTF8 called with inbufptr=%p  inbuflen=%u", inbufptr, inbuflen);

  WBufReset(&outbuf);

  if (outflags & OutputFlagHTML) {
    WBufReserve(&outbuf, inbuflen * 2);  // approximate output size to minimize reallocations

    FmtHTML fmt = {
      .flags = 0,
      .parserFlags = parser_flags,
      .outbuf = &outbuf,
      .onCodeBlock = onCodeBlock,
    };

    if (outflags & OutputFlagXHTML)
      fmt.flags |= MD_HTML_FLAG_XHTML;

    if (fmt_html(inbufptr, inbuflen, &fmt) != 0) {
      // fmt_html returns status of md_parse which only fails in extreme cases
      // like when out of memory. md4c does not provide error codes or error messages.
      WErrSet(ERR_MD_PARSE, "md parser error");
      *outptr = 0;
      return 0;
    }

    *outptr = outbuf.start;
    // dlog("outbuf =>\n%.*s\n", WBufLen(&outbuf), outbuf.start);
    return WBufLen(&outbuf);
  }

  WErrSet(ERR_OUTFLAGS, "no output format set in output flags");
  *outptr = 0;
  return 0;
}
