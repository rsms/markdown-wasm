#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "fmt_json.h"
#include "md4c.h"
// #include "md4c_render_html.h"
// #include "entity.h"

//
//
// --------------   WORK IN PROGRESS
//
//

#ifdef _WIN32
  #define snprintf _snprintf
#endif


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
} JsonFormatter;


#define ISDIGIT(ch)  ('0' <= (ch) && (ch) <= '9')
#define ISLOWER(ch)  ('a' <= (ch) && (ch) <= 'z')
#define ISUPPER(ch)  ('A' <= (ch) && (ch) <= 'Z')
#define ISALNUM(ch)  (ISLOWER(ch) || ISUPPER(ch) || ISDIGIT(ch))


// static inline void render_text(JsonFormatter* r, const MD_CHAR* text, MD_SIZE size) {
//   // r->process_output(text, size, r->userdata);
//   WBufAppendBytes(r->outbuf, text, size);
// }

#define render_text(f, textptr, textlen) \
  WBufAppendBytes((r)->outbuf, (textptr), (textlen))

// #define RENDER_LITERAL(r, literal) \
//   WBufAppendBytes((r)->outbuf, (literal), (MD_SIZE)strlen(literal))


static char jsonEscapeMap[256];

static void __attribute__((constructor)) init() {
  jsonEscapeMap[(unsigned char)'"'] = 1;
  jsonEscapeMap[(unsigned char)'\n'] = 1;
  jsonEscapeMap[(unsigned char)'\r'] = 1;
  jsonEscapeMap[(unsigned char)'\t'] = 1;
}


static void writeJsonEscaped(JsonFormatter* r, const MD_CHAR* data, MD_SIZE size) {
  MD_OFFSET beg = 0;
  MD_OFFSET off = 0;

  #define NEED_ESCAPE(ch)  (jsonEscapeMap[(unsigned char)(ch)] != 0)

  while(1) {
    /* Optimization: Use some loop unrolling. */
    while (
      off + 3 < size &&
      !NEED_ESCAPE(data[off+0]) &&
      !NEED_ESCAPE(data[off+1]) &&
      !NEED_ESCAPE(data[off+2]) &&
      !NEED_ESCAPE(data[off+3])
    ) {
      off += 4;
    }
    while (off < size && !NEED_ESCAPE(data[off])) {
      off++;
    }

    if (off > beg) {
      WBufAppendBytes(r->outbuf, data + beg, off - beg);
    }

    if (off < size) {
      switch (data[off]) {
        case '"':   WBufAppendCStr(r->outbuf, "\\\""); break;
        case '\n':  WBufAppendCStr(r->outbuf, "\\n");  break;
        case '\r':  WBufAppendCStr(r->outbuf, "\\r");  break;
        case '\t':  WBufAppendCStr(r->outbuf, "\\t");  break;
      }
      off++;
    } else {
      break;
    }

    beg = off;
  }

  #undef NEED_ESCAPE
}


static void
render_url_escaped(JsonFormatter* r, const MD_CHAR* data, MD_SIZE size)
{
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

static unsigned
hex_val(char ch)
{
  if('0' <= ch && ch <= '9')
    return ch - '0';
  if('A' <= ch && ch <= 'Z')
    return ch - 'A' + 10;
  else
    return ch - 'a' + 10;
}

static void WBufAppendUTF8Codepoint(WBuf* b, u32 codepoint) {
  if (codepoint <= 0x7f) {
    WBufAppendc(b, (char)codepoint);
    return;
  }

  unsigned char utf8[4];
  size_t n;
  if (codepoint <= 0x7ff) {
    n = 2;
    utf8[0] = 0xc0 | ((codepoint >>  6) & 0x1f);
    utf8[1] = 0x80 + ((codepoint >>  0) & 0x3f);
  } else if (codepoint <= 0xffff) {
    n = 3;
    utf8[0] = 0xe0 | ((codepoint >> 12) & 0xf);
    utf8[1] = 0x80 + ((codepoint >>  6) & 0x3f);
    utf8[2] = 0x80 + ((codepoint >>  0) & 0x3f);
  } else {
    n = 4;
    utf8[0] = 0xf0 | ((codepoint >> 18) & 0x7);
    utf8[1] = 0x80 + ((codepoint >> 12) & 0x3f);
    utf8[2] = 0x80 + ((codepoint >>  6) & 0x3f);
    utf8[3] = 0x80 + ((codepoint >>  0) & 0x3f);
  }

  if (0 < codepoint && codepoint <= 0x10ffff) {
    WBufAppendBytes(b, (const char*)utf8, n);
  } else {
    static const MD_CHAR utf8_replacement_char[] = { 0xef, 0xbf, 0xbd };
    WBufAppendBytes(b, utf8_replacement_char, sizeof(utf8_replacement_char));
  }
}

/* Translate entity to its UTF-8 equivalent, or output the verbatim one
 * if such entity is unknown (or if the translation is disabled). */
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

    WBufAppendUTF8Codepoint(r->outbuf, codepoint);
  } else {
    WBufAppendBytes(r->outbuf, text, size);
  }
}

static void
render_attribute(JsonFormatter* r, const MD_ATTRIBUTE* attr,
         void (*fn_append)(JsonFormatter*, const MD_CHAR*, MD_SIZE))
{
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


static void
render_open_ol_block(JsonFormatter* r, const MD_BLOCK_OL_DETAIL* det)
{
  char buf[64];

  if(det->start == 1) {
    WBufAppendCStr(r->outbuf, "<ol>\n");
    return;
  }

  snprintf(buf, sizeof(buf), "<ol start=\"%u\">\n", det->start);
  WBufAppendCStr(r->outbuf, buf);
}

static void
render_open_li_block(JsonFormatter* r, const MD_BLOCK_LI_DETAIL* det)
{
  if(det->is_task) {
    WBufAppendCStr(r->outbuf,
      "<li class=\"task-list-item\">"
      "<input type=\"checkbox\" class=\"task-list-item-checkbox\" disabled");
    if (det->task_mark == 'x' || det->task_mark == 'X') {
      WBufAppendCStr(r->outbuf, " checked");
    }
    WBufAppendc(r->outbuf, '>');
  } else {
    WBufAppendCStr(r->outbuf, "<li>");
  }
}

static void
render_open_code_block(JsonFormatter* r, const MD_BLOCK_CODE_DETAIL* det)
{
  WBufAppendCStr(r->outbuf, "<pre><code");

  /* If known, output the HTML 5 attribute class="language-LANGNAME". */
  if(det->lang.text != NULL) {
    WBufAppendCStr(r->outbuf, " class=\"language-");
    render_attribute(r, &det->lang, writeJsonEscaped);
    WBufAppendc(r->outbuf, '"');
  }

  WBufAppendc(r->outbuf, '>');
}

static void
render_open_td_block(JsonFormatter* r, const MD_CHAR* cell_type, const MD_BLOCK_TD_DETAIL* det)
{
  WBufAppendc(r->outbuf, '<');
  WBufAppendCStr(r->outbuf, cell_type);

  switch (det->align) {
    case MD_ALIGN_LEFT:     WBufAppendCStr(r->outbuf, " align=\"left\">"); break;
    case MD_ALIGN_CENTER:   WBufAppendCStr(r->outbuf, " align=\"center\">"); break;
    case MD_ALIGN_RIGHT:    WBufAppendCStr(r->outbuf, " align=\"right\">"); break;
    default:                WBufAppendCStr(r->outbuf, ">"); break;
  }
}

static void
render_open_a_span(JsonFormatter* r, const MD_SPAN_A_DETAIL* det)
{
  WBufAppendCStr(r->outbuf, "<a href=\"");
  render_attribute(r, &det->href, render_url_escaped);

  if(det->title.text != NULL) {
    WBufAppendCStr(r->outbuf, "\" title=\"");
    render_attribute(r, &det->title, writeJsonEscaped);
  }

  WBufAppendCStr(r->outbuf, "\">");
}

static void
render_open_img_span(JsonFormatter* r, const MD_SPAN_IMG_DETAIL* det)
{
  WBufAppendCStr(r->outbuf, "<img src=\"");
  render_attribute(r, &det->src, render_url_escaped);

  WBufAppendCStr(r->outbuf, "\" alt=\"");
}

static void
render_close_img_span(JsonFormatter* r, const MD_SPAN_IMG_DETAIL* det)
{
  if(det->title.text != NULL) {
    WBufAppendCStr(r->outbuf, "\" title=\"");
    render_attribute(r, &det->title, writeJsonEscaped);
  }

  WBufAppendCStr(r->outbuf, "\">");
}

static void
render_open_wikilink_span(JsonFormatter* r, const MD_SPAN_WIKILINK_DETAIL* det)
{
  WBufAppendCStr(r->outbuf, "<x-wikilink data-target=\"");
  render_attribute(r, &det->target, writeJsonEscaped);
  WBufAppendCStr(r->outbuf, "\">");
}


// ------------------------------------------------------------------------------------------------


static void writeTypeStart(JsonFormatter* r, const char* typename, size_t typenamelen) {
  WBufAppendCStr(r->outbuf, "{\"_\":\"");
  WBufAppendBytes(r->outbuf, typename, typenamelen);
  WBufAppendc(r->outbuf, '"');
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


static int enter_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata) {
  static const MD_CHAR* head[6] = { "h1", "h2", "h3", "h4", "h5", "h6" };
  JsonFormatter* r = (JsonFormatter*) userdata;
  const char* typename = "";
  size_t typenamelen = 0;
  #define WRITE_TYPE_START(name) writeTypeStart(r, (name), strlen((name)))

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
      WRITE_TYPE_START(head[((MD_BLOCK_H_DETAIL*)detail)->level - 1]);
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

// static void
// render_open_td_block(MD_RENDER_HTML* r, const MD_CHAR* cell_type, const MD_BLOCK_TD_DETAIL* det)
// {
//     RENDER_LITERAL(r, "<");
//     RENDER_LITERAL(r, cell_type);

//     switch(det->align) {
//         case MD_ALIGN_LEFT:     RENDER_LITERAL(r, " align=\"left\">"); break;
//         case MD_ALIGN_CENTER:   RENDER_LITERAL(r, " align=\"center\">"); break;
//         case MD_ALIGN_RIGHT:    RENDER_LITERAL(r, " align=\"right\">"); break;
//         default:                RENDER_LITERAL(r, ">"); break;
//     }
  }

  WBufAppendCStr(r->outbuf, ", \"children\":[\n  ");
  return 0;
}


static int leave_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata) {
  JsonFormatter* r = (JsonFormatter*)userdata;
  WBufAppendCStr(r->outbuf, "]},\n");
  return 0;
}


static int enter_span_callback(MD_SPANTYPE type, void* detail, void* userdata) {
  JsonFormatter* r = (JsonFormatter*) userdata;

  switch(type) {
    case MD_SPAN_EM:                WBufAppendCStr(r->outbuf, "<em>"); break;
    case MD_SPAN_STRONG:            WBufAppendCStr(r->outbuf, "<b>"); break;
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

  WBufAppendCStr(r->outbuf, ", \"");

  switch (type) {
    case MD_TEXT_NULLCHAR:  WBufAppendCStr(r->outbuf, "\\0"); break;
    case MD_TEXT_BR:        WBufAppendCStr(r->outbuf, "<br>"); break;
    case MD_TEXT_SOFTBR:    WBufAppendc(r->outbuf, '\n'); break;
    case MD_TEXT_HTML:      render_text(r, text, size); break;
    case MD_TEXT_ENTITY:    writeDecodeXmlEntity(r, text, size); break;
    default:                writeJsonEscaped(r, text, size); break;
  }

  WBufAppendc(r->outbuf, '"');

  return 0;
}

int fmt_json(const MD_CHAR* input, MD_SIZE input_size, WBuf* outbuf, u32 parser_flags) {
  JsonFormatter render = { outbuf };
  MD_PARSER parser = {
    0,
    parser_flags,
    enter_block_callback,
    leave_block_callback,
    enter_span_callback,
    leave_span_callback,
    text_callback,
    NULL, //debug_log_callback,
    NULL
  };

  return md_parse(input, input_size, &parser, (void*) &render);
}
