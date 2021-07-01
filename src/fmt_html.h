#pragma once

#define MD_HTML_FLAG_XHTML 0x0008 // instead of e.g. <br>, generate <br/>

typedef struct FmtHTML {
  u32   flags;       // MD_HTML_FLAG_*
  u32   parserFlags; // passed along to md_parse
  WBuf* outbuf;

  // optional callbacks
  JSTextFilterFun onCodeBlock;

  // internal state
  int  imgnest;
  int  addanchor;
  int  codeBlockNest;
  WBuf tmpbuf;
} FmtHTML;

int fmt_html(const char* input, u32 inputlen, FmtHTML* fmt);
