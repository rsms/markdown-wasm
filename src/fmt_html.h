#pragma once

typedef struct FmtHTML {
  OutputFlags flags;
  u32         parserFlags; // passed along to md_parse
  WBuf*       outbuf;

  // optional callbacks
  JSTextFilterFun onCodeBlock;

  // internal state
  int  imgnest;
  int  addanchor;
  int  codeBlockNest;
  WBuf tmpbuf;
} FmtHTML;

int fmt_html(const char* input, u32 inputlen, FmtHTML* fmt);
