#pragma once
#include "wbuf.h"

int fmt_html(const char* input, u32 inputlen, WBuf* outbuf, u32 parserFlags);
