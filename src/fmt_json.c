#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "fmt_json.h"
#include "md4c.h"

// JSON formatter
//
// -- WORK IN PROGRESS --
//

#ifdef _WIN32
  #define snprintf _snprintf
#endif

// dlog
#ifndef DEBUG
#define DEBUG 1
#endif
#if DEBUG
#include <stdio.h>
#  define dlog(...)  printf(__VA_ARGS__)
#else
#  define dlog(...)
#endif /* DEBUG > 0 */


typedef struct JsonFormatter_st {
  WBuf* outbuf;
  u32   bnest; // block nesting level
} JsonFormatter;


#define ISDIGIT(ch)  ('0' <= (ch) && (ch) <= '9')
#define ISLOWER(ch)  ('a' <= (ch) && (ch) <= 'z')
#define ISUPPER(ch)  ('A' <= (ch) && (ch) <= 'Z')
#define ISALNUM(ch)  (ISLOWER(ch) || ISUPPER(ch) || ISDIGIT(ch))

#define SIZEOF_ARRAY(a)     (sizeof(a) / sizeof(a[0]))


#define render_text(f, textptr, textlen) \
  WBufAppendBytes((r)->outbuf, (textptr), (textlen))


#define JSON_SUB_LEN 2
static const char* jsonEscapeMap[256];

static void __attribute__((constructor)) init() {
  // important: Values must all be exactly JSON_SUB_LEN bytes long
  jsonEscapeMap[(unsigned char)'"']  = "\\\"";
  jsonEscapeMap[(unsigned char)'\n'] = "\\n";
  jsonEscapeMap[(unsigned char)'\r'] = "\\r";
  jsonEscapeMap[(unsigned char)'\t'] = "\\t";
}

// #define JSON_BYTE_NEED_ESCAPE(ch)  (jsonEscapeMap[(unsigned char)(ch)] != 0)
#define JSON_ESCAPE_MAP(ch)  jsonEscapeMap[(unsigned char)(ch)]


static void writeJsonEscaped(JsonFormatter* r, const MD_CHAR* data, MD_SIZE size) {
  MD_OFFSET beg = 0;
  MD_OFFSET off = 0;

  while (1) {
    const char* sub = NULL;
    while (off < size) {
      sub = JSON_ESCAPE_MAP(data[off]);
      if (sub != NULL) {
        break;
      }
      off++;
    }

    if (off > beg) {
      // in-between
      WBufAppendBytes(r->outbuf, data + beg, off - beg);
    }

    if (sub) {
      WBufAppendBytes(r->outbuf, sub, JSON_SUB_LEN);
      off++;
    } else {
      break;
    }

    beg = off;
  }
}


static void render_url_escaped(JsonFormatter* r, const MD_CHAR* data, MD_SIZE size) {
  static const MD_CHAR hex_chars[] = "0123456789ABCDEF";
  MD_OFFSET beg = 0;
  MD_OFFSET off = 0;

  #define URL_NEED_ESCAPE(ch)                                             \
      (!ISALNUM(ch)  &&  strchr("-_.+!*'(),%#@?=;:/,+$", ch) == NULL)

  while(1) {
    while(off < size  &&  !URL_NEED_ESCAPE(data[off]))
      off++;
    if(off > beg)
      render_text(r, data + beg, off - beg);

    if(off < size) {
      char hex[3];

      switch(data[off]) {
        case '&':   WBufAppendCStr(r->outbuf, "&amp;"); break;
        case '\'':  WBufAppendCStr(r->outbuf, "&#x27;"); break;
        default:
          hex[0] = '%';
          hex[1] = hex_chars[((unsigned)data[off] >> 4) & 0xf];
          hex[2] = hex_chars[((unsigned)data[off] >> 0) & 0xf];
          render_text(r, hex, 3);
          break;
      }
      off++;
    } else {
      break;
    }

    beg = off;
  }
}

static unsigned hex_val(char ch) {
  if ('0' <= ch && ch <= '9') {
    return ch - '0';
  }
  if ('A' <= ch && ch <= 'Z') {
    return ch - 'A' + 10;
  }
  return ch - 'a' + 10;
}

// Translate entity to its UTF-8 equivalent, or output the verbatim one
// if such entity is unknown (or if the translation is disabled).
static void writeDecodeXmlEntity(JsonFormatter* r, const MD_CHAR* text, MD_SIZE size) {
  if (size > 3 && text[1] == '#') {
    unsigned codepoint = 0;
    if(text[2] == 'x' || text[2] == 'X') {
      // Hexadecimal entity (e.g. "&#x1234abcd;")).
      for (MD_SIZE i = 3; i < size-1; i++) {
        codepoint = 16 * codepoint + hex_val(text[i]);
      }
    } else {
      // Decimal entity (e.g. "&1234;")
      for (MD_SIZE i = 2; i < size-1; i++) {
        codepoint = 10 * codepoint + (text[i] - '0');
      }
    }
    if (codepoint <= 0xFF) {
      const char* sub = JSON_ESCAPE_MAP(codepoint);
      if (sub) {
        // predefined escape code, e.g. "\n"
        WBufAppendBytes(r->outbuf, sub, JSON_SUB_LEN);
      } else {
        // verbatim
        WBufAppendUTF8Codepoint(r->outbuf, codepoint);
      }
    } else {
      // e.g. \uD87E
      WBufAppendCStr(r->outbuf, "\\u");
      if (codepoint <= 0xF) {
        WBufAppendCStr(r->outbuf, "000");
      } else if (codepoint <= 0xFF) {
        WBufAppendCStr(r->outbuf, "00");
      } else if (codepoint <= 0xFFF) {
        WBufAppendCStr(r->outbuf, "0");
      }
      WBufAppendU32(r->outbuf, codepoint, 16);
    }
  } else {
    // named entity
    // We could do a lookup here but it would increase the WASM module binary size by
    // at least 20kB, so for now, let's keep it simple and just include it verbatim until we
    // can do something fancy like a compressed b-tree.
    writeJsonEscaped(r, text, size);
  }
}

static void render_attribute(
  JsonFormatter* r,
  const MD_ATTRIBUTE* attr,
  void (*fn_append)(JsonFormatter*, const MD_CHAR*, MD_SIZE)
) {
  int i;

  for(i = 0; attr->substr_offsets[i] < attr->size; i++) {
    MD_TEXTTYPE type = attr->substr_types[i];
    MD_OFFSET off = attr->substr_offsets[i];
    MD_SIZE size = attr->substr_offsets[i+1] - off;
    const MD_CHAR* text = attr->text + off;

    switch(type) {
      case MD_TEXT_NULLCHAR:  WBufAppendc(r->outbuf, 0); break;
      case MD_TEXT_ENTITY:    writeDecodeXmlEntity(r, text, size); break;
      default:                WBufAppendBytes(r->outbuf, text, size); break;
    }
  }
}


static void render_open_a_span(JsonFormatter* r, const MD_SPAN_A_DETAIL* det) {
  WBufAppendCStr(r->outbuf, "<a href=\"");
  render_attribute(r, &det->href, render_url_escaped);

  if(det->title.text != NULL) {
    WBufAppendCStr(r->outbuf, "\" title=\"");
    render_attribute(r, &det->title, writeJsonEscaped);
  }

  WBufAppendCStr(r->outbuf, "\">");
}

static void render_open_img_span(JsonFormatter* r, const MD_SPAN_IMG_DETAIL* det) {
  WBufAppendCStr(r->outbuf, "<img src=\"");
  render_attribute(r, &det->src, render_url_escaped);

  WBufAppendCStr(r->outbuf, "\" alt=\"");
}

static void render_close_img_span(JsonFormatter* r, const MD_SPAN_IMG_DETAIL* det) {
  if(det->title.text != NULL) {
    WBufAppendCStr(r->outbuf, "\" title=\"");
    render_attribute(r, &det->title, writeJsonEscaped);
  }

  WBufAppendCStr(r->outbuf, "\">");
}

static void render_open_wikilink_span(JsonFormatter* r, const MD_SPAN_WIKILINK_DETAIL* det) {
  WBufAppendCStr(r->outbuf, "<x-wikilink data-target=\"");
  render_attribute(r, &det->target, writeJsonEscaped);
  WBufAppendCStr(r->outbuf, "\">");
}


// ------------------------------------------------------------------------------------------------


static void writeNewline(JsonFormatter* r) {
  WBufAppendc(r->outbuf, '\n');

  static const char indent_chunk_str[] = "                ";
  static const u32  indent_chunk_size = (u32)(SIZEOF_ARRAY(indent_chunk_str) - 1);
  u32 indent = r->bnest * 4;
  while (indent > indent_chunk_size) {
    WBufAppendBytes(r->outbuf, indent_chunk_str, indent_chunk_size);
    indent -= indent_chunk_size;
  }
  if (indent > 0) {
    WBufAppendBytes(r->outbuf, indent_chunk_str, indent);
  }
}

static void writeAttribute(JsonFormatter* r, const MD_ATTRIBUTE* attr) {
  for (u32 i = 0; attr->substr_offsets[i] < attr->size; i++) {
    MD_TEXTTYPE type = attr->substr_types[i];
    MD_OFFSET   off  = attr->substr_offsets[i];
    MD_SIZE     size = attr->substr_offsets[i+1] - off;
    const MD_CHAR* text = attr->text + off;
    switch (type) {
      case MD_TEXT_NULLCHAR:  WBufAppendCStr(r->outbuf, "\\0"); break;
      case MD_TEXT_ENTITY:    writeDecodeXmlEntity(r, text, size); break;
      default:                WBufAppendBytes(r->outbuf, text, size); break;
    }
  }
}

static void writeKey(JsonFormatter* r, const char* rawkey, size_t rawkeyLen) {
  WBufAppendc(r->outbuf, ',');
  writeNewline(r);
  WBufAppendc(r->outbuf, '"');
  WBufAppendBytes(r->outbuf, rawkey, rawkeyLen);
  WBufAppendCStr(r->outbuf, "\":");
}

static void writeTypeStart(JsonFormatter* r, const char* typename, size_t typenamelen) {
  WBufAppendCStr(r->outbuf, "{ \"_\": \"");
  WBufAppendBytes(r->outbuf, typename, typenamelen);
  WBufAppendc(r->outbuf, '"');
}


static int enter_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata) {
  JsonFormatter* r = (JsonFormatter*) userdata;

  writeNewline(r);
  r->bnest++;

  #define WRITE_TYPE_START(name) \
    writeTypeStart(r, (name), strlen((name)))

  switch (type) {
    case MD_BLOCK_DOC:   WRITE_TYPE_START("doc"); break;
    case MD_BLOCK_QUOTE: WRITE_TYPE_START("quote"); break;
    case MD_BLOCK_UL:    WRITE_TYPE_START("ul"); break;
    case MD_BLOCK_HTML:  WRITE_TYPE_START("html"); break;
    case MD_BLOCK_P:     WRITE_TYPE_START("p"); break;
    case MD_BLOCK_TABLE: WRITE_TYPE_START("table"); break;
    case MD_BLOCK_THEAD: WRITE_TYPE_START("thead"); break;
    case MD_BLOCK_TBODY: WRITE_TYPE_START("tbody"); break;
    case MD_BLOCK_TR:    WRITE_TYPE_START("tr"); break;
    case MD_BLOCK_HR:    WRITE_TYPE_START("hr"); break;

    case MD_BLOCK_H: {
      WRITE_TYPE_START("h");
      WBufAppendCStr(r->outbuf, ", \"level\": ");
      WBufAppendU32(r->outbuf, ((MD_BLOCK_H_DETAIL*)detail)->level, 10);
      break;
    }

    case MD_BLOCK_OL: {
      WRITE_TYPE_START("ol");
      const MD_BLOCK_OL_DETAIL* d = (const MD_BLOCK_OL_DETAIL*)detail;
      if (d->start != 1) {
        char buf[24];
        snprintf(buf, sizeof(buf), ", \"start\":%u", d->start);
        WBufAppendCStr(r->outbuf, buf);
      }
      if (d->is_tight) {
        WBufAppendCStr(r->outbuf, ", \"tight\":true");
      }
      WBufAppendCStr(r->outbuf, ", \"delimiter\":\"");
      WBufAppendc(r->outbuf, d->mark_delimiter);
      WBufAppendc(r->outbuf, '"');
      break;
    }

    case MD_BLOCK_LI: {
      const MD_BLOCK_LI_DETAIL* d = (const MD_BLOCK_LI_DETAIL*)detail;
      if (d->is_task) {
        WBufAppendCStr(r->outbuf, "{\"_\":\"task\"");
        if (d->task_mark == 'x' || d->task_mark == 'X') {
          WBufAppendCStr(r->outbuf, ", \"complete\":true");
        }
      } else {
        WRITE_TYPE_START("li");
      }
      break;
    }

    case MD_BLOCK_CODE: {
      WRITE_TYPE_START("code");
      const MD_BLOCK_CODE_DETAIL* d = (const MD_BLOCK_CODE_DETAIL*)detail;
      if (d->lang.text != NULL) {
        WBufAppendCStr(r->outbuf, ", \"lang\":\"");
        writeAttribute(r, &d->lang);
        WBufAppendc(r->outbuf, '"');
      }
      if (d->info.text != NULL) {
        WBufAppendCStr(r->outbuf, ", \"info\":\"");
        writeAttribute(r, &d->info);
        WBufAppendc(r->outbuf, '"');
      }
      break;
    }

    case MD_BLOCK_TH:
    case MD_BLOCK_TD: {
      writeTypeStart(r, type == MD_BLOCK_TH ? "th" : "td", 2);
      const MD_BLOCK_TD_DETAIL* d = (const MD_BLOCK_TD_DETAIL*)detail;
      switch (d->align) {
          case MD_ALIGN_LEFT:   WBufAppendCStr(r->outbuf, ", \"align\":\"left\""); break;
          case MD_ALIGN_CENTER: WBufAppendCStr(r->outbuf, ", \"align\":\"center\""); break;
          case MD_ALIGN_RIGHT:  WBufAppendCStr(r->outbuf, ", \"align\":\"right\""); break;
          default: break;  // unspecified alignment
      }
      break;
    }

  }

  WBufAppendCStr(r->outbuf, ", \"children\": [");

  return 0;
}


static int leave_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata) {
  JsonFormatter* r = (JsonFormatter*)userdata;
  r->bnest--;
  if (*(r->outbuf->ptr-1) == ',') {
    // undo trailing comma
    // e.g.
    //
    //  "1,2,3,"
    //        ^
    //  "1,2,3"
    //       ^
    //
    r->outbuf->ptr--;
  }
  writeNewline(r);
  WBufAppendCStr(r->outbuf, "]},");
  return 0;
}


static int enter_span_callback(MD_SPANTYPE type, void* detail, void* userdata) {
  JsonFormatter* r = (JsonFormatter*) userdata;

  switch(type) {
    case MD_SPAN_EM:                WBufAppendCStr(r->outbuf, "<em>"); break;
    case MD_SPAN_STRONG:            WBufAppendCStr(r->outbuf, "<b>"); break;
    case MD_SPAN_U:                 WBufAppendCStr(r->outbuf, "<u>"); break;
    case MD_SPAN_A:                 render_open_a_span(r, (MD_SPAN_A_DETAIL*) detail); break;
    case MD_SPAN_IMG:               render_open_img_span(r, (MD_SPAN_IMG_DETAIL*) detail); break;
    case MD_SPAN_CODE:              WBufAppendCStr(r->outbuf, "<code>"); break;
    case MD_SPAN_DEL:               WBufAppendCStr(r->outbuf, "<del>"); break;
    case MD_SPAN_LATEXMATH:         WBufAppendCStr(r->outbuf, "<x-equation>"); break;
    case MD_SPAN_LATEXMATH_DISPLAY: WBufAppendCStr(r->outbuf, "<x-equation type=\"display\">"); break;
    case MD_SPAN_WIKILINK:          render_open_wikilink_span(r, (MD_SPAN_WIKILINK_DETAIL*) detail); break;
  }

  return 0;
}

static int leave_span_callback(MD_SPANTYPE type, void* detail, void* userdata) {
  JsonFormatter* r = (JsonFormatter*) userdata;

  switch(type) {
    case MD_SPAN_EM:                WBufAppendCStr(r->outbuf, "</em>"); break;
    case MD_SPAN_STRONG:            WBufAppendCStr(r->outbuf, "</b>"); break;
    case MD_SPAN_U:                 WBufAppendCStr(r->outbuf, "</u>"); break;
    case MD_SPAN_A:                 WBufAppendCStr(r->outbuf, "</a>"); break;
    case MD_SPAN_IMG:               /*noop, handled above*/ break;
    case MD_SPAN_CODE:              WBufAppendCStr(r->outbuf, "</code>"); break;
    case MD_SPAN_DEL:               WBufAppendCStr(r->outbuf, "</del>"); break;
    case MD_SPAN_LATEXMATH:         /*fall through*/
    case MD_SPAN_LATEXMATH_DISPLAY: WBufAppendCStr(r->outbuf, "</x-equation>"); break;
    case MD_SPAN_WIKILINK:          WBufAppendCStr(r->outbuf, "</x-wikilink>"); break;
  }

  return 0;
}

static int text_callback(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata) {
  JsonFormatter* r = (JsonFormatter*)userdata;

  if (type == MD_TEXT_SOFTBR) {
    // ignore soft break, i.e.
    //
    // Markdown:
    //    line1
    //    line2
    //
    // md4c emits: (line1, MD_TEXT_SOFTBR, line2)
    //
    return 0;
  }

  writeNewline(r);

  if (type == MD_TEXT_HTML) {
    WBufAppendCStr(r->outbuf, "{\"_\":\"html\",\"content\":\"");
    writeJsonEscaped(r, text, size);
    WBufAppendCStr(r->outbuf, "\"}");
  } else {
    WBufAppendc(r->outbuf, '"');
    switch (type) {
    case MD_TEXT_NULLCHAR:
      WBufAppendCStr(r->outbuf, "\\0");
      break;

    case MD_TEXT_BR:
      WBufAppendCStr(r->outbuf, "\\n");
      break;

    case MD_TEXT_ENTITY:
      writeDecodeXmlEntity(r, text, size);
      break;

    default:
      writeJsonEscaped(r, text, size);
      break;
    }
    WBufAppendc(r->outbuf, '"');
  }

  WBufAppendc(r->outbuf, ',');

  return 0;
}

int fmt_json(
  const MD_CHAR* input,
  MD_SIZE inputlen,
  WBuf* outbuf,
  u32 parseFlags,
  u32 _fmtFlags
) {
  JsonFormatter render = {
    .outbuf = outbuf,
    .bnest = 0,
  };
  MD_PARSER parser = {
    0,
    parseFlags,
    enter_block_callback,
    leave_block_callback,
    enter_span_callback,
    leave_span_callback,
    text_callback,
    NULL, //debug_log_callback,
    NULL
  };

  return md_parse(input, inputlen, &parser, (void*)&render);
}
