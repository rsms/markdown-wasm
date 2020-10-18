#pragma once
#include "wbuf.h"

#define MD_JSON_FLAG_NONE 0

int fmt_json(const char* input, u32 inputlen, WBuf* outbuf, u32 parseFlags, u32 fmtFlags);
