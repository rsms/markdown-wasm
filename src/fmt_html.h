#pragma once
#include "wbuf.h"

#define MD_HTML_FLAG_XHTML (1 << 0) // instead of e.g. <br>, generate <br/>

int fmt_html(const char* input, u32 inputlen, WBuf* outbuf, u32 parseFlags, u32 fmtFlags);
