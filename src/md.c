#include <ctype.h>
#include "common.h"
#include "wlib.h"
#include "wbuf.h"
#include "fmt_html.h"
// #include "fmt_json.h"

// #include "md4c.h"
/* If set, debug output from md_parse() is sent to stderr. */
#define MD_RENDER_FLAG_DEBUG                0x0001
#define MD_RENDER_FLAG_VERBATIM_ENTITIES    0x0002

typedef enum OutputFlags {
  OutputFlagsHTML = 1 << 0,
} OutputFlags;

typedef enum ErrorCode {
  ERR_NONE,
  ERR_MD_PARSE,
  ERR_OUTFLAGS,
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
  const char* inbufptr,
  u32 inbuflen,
  u32 parser_flags,
  OutputFlags outflags,
  const char** outptr
) {
  dlog("parseUTF8 called with inbufptr=%p  inbuflen=%u\n", inbufptr, inbuflen);

  WBufReset(&outbuf);

  if (outflags & OutputFlagsHTML) {
    WBufReserve(&outbuf, inbuflen * 2);  // approximate output size to minimize reallocations

    if (fmt_html(inbufptr, inbuflen, &outbuf, parser_flags) != 0) {
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
