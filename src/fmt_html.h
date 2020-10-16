#pragma once
#include "wbuf.h"

#define MD_HTML_FLAG_XHTML 0x0008 // instead of e.g. <br>, generate <br/>

int fmt_html(const char* input, u32 inputlen, WBuf* outbuf, u32 parserFlags, u32 renderFlags);
