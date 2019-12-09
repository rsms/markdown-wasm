#pragma once
#include "wbuf.h"

int fmt_json(const char* input, u32 inputlen, WBuf* outbuf, u32 parserFlags);
