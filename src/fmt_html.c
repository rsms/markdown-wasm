/*
 * md4c modified for mdjs.
 * Original source code is licensed as follows:
 *
 * Copyright (c) 2016-2019 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <string.h>
#include <ctype.h>
#include <strings.h>

#include "common.h"
#include "fmt_html.h"
#include "md4c.h"

// typedef struct FmtHTML_st {
//   WBuf* outbuf;
//   int   imgnest;
//   int   addanchor;
//   u32   flags;
// } FmtHTML;


static char htmlEscapeMap[256] = {
        /* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
/* 0x00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // <CTRL> ...
/* 0x10 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // <CTRL> ...
/* 0x20 */ 0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,  //   ! " # $ % & ' ( ) * + , - . /
/* 0x30 */ 0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,  // 0 1 2 3 4 5 6 7 8 9 : ; < = > ?
/* 0x40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // @ A B C D E F G H I J K L M N O
/* 0x50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // P Q R S T U V W X Y Z [ \ ] ^ _
/* 0x60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // ` a b c d e f g h i j k l m n o
/* 0x70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // p q r s t u v w x y z { | } ~ <DEL>
/* 0x80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // <CTRL> ...
/* 0x90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // <CTRL> ...
/* 0xA0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // <NBSP> ¡ ¢ £ ¤ ¥ ¦ § ¨ © ª « ¬ <SOFTHYPEN> ® ¯
/* 0xB0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // ° ± ² ³ ´ µ ¶ · ¸ ¹ º » ¼ ½ ¾ ¿
/* 0xC0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // À Á Â Ã Ä Å Æ Ç È É Ê Ë Ì Í Î Ï
/* 0xD0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // Ð Ñ Ò Ó Ô Õ Ö × Ø Ù Ú Û Ü Ý Þ ß
/* 0xE0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // à á â ã ä å æ ç è é ê ë ì í î ï
/* 0xF0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // ð ñ ò ó ô õ ö ÷ ø ù ú û ü ý þ ÿ
};

static const char ucReplacementUTF8[] = { 0xef, 0xbf, 0xbd };


static inline void render_text(FmtHTML* r, const char* pch, size_t len) {
  WBufAppendBytes(r->outbuf, pch, len);
}

static inline void render_literal(FmtHTML* r, const char* cs) {
  WBufAppendBytes(r->outbuf, cs, strlen(cs));
}

static inline void render_char(FmtHTML* r, char c) {
  WBufAppendc(r->outbuf, c);
}


static void render_html_escaped(FmtHTML* r, const char* data, size_t size) {
  MD_OFFSET beg = 0;
  MD_OFFSET off = 0;

  /* Some characters need to be escaped in normal HTML text. */
  #define HTML_NEED_ESCAPE(ch)  (htmlEscapeMap[(unsigned char)(ch)] != 0)

  while (1) {
    while (
      off + 3 < size &&
      !HTML_NEED_ESCAPE(data[off+0]) &&
      !HTML_NEED_ESCAPE(data[off+1]) &&
      !HTML_NEED_ESCAPE(data[off+2]) &&
      !HTML_NEED_ESCAPE(data[off+3])
    ) {
      off += 4;
    }

    while (off < size && !HTML_NEED_ESCAPE(data[off])) {
      off++;
    }

    if (off > beg) {
      render_text(r, data + beg, off - beg);
    }

    if (off < size) {
      switch (data[off]) {
        case '&': render_literal(r, "&amp;");  break;
        case '<': render_literal(r, "&lt;");   break;
        case '>': render_literal(r, "&gt;");   break;
        case '"': render_literal(r, "&quot;"); break;
      }
      off++;
    } else {
      break;
    }
    beg = off;
  }
}


static char slugMap[256] = {
/*          0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F    */
/* 0x00 */ '-','-','-','-','-','-','-','-','-','-','-','-','-','-','-','-',  // <CTRL> ...
/* 0x10 */ '-','-','-','-','-','-','-','-','-','-','-','-','-','-','-','-',  // <CTRL> ...
/* 0x20 */ '-','-','-','-','-','-','-','-','-','-','-','-','-','-','.','-',  //   ! " # $ % & ' ( ) * + , - . /
/* 0x30 */ '0','1','2','3','4','5','6','7','8','9','-','-','-','-','-','-',  // 0 1 2 3 4 5 6 7 8 9 : ; < = > ?
/* 0x40 */ '-','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',  // @ A B C D E F G H I J K L M N O
/* 0x50 */ 'p','q','r','s','t','u','v','w','x','y','z','-','-','-','-','_',  // P Q R S T U V W X Y Z [ \ ] ^ _
/* 0x60 */ '-','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',  // ` a b c d e f g h i j k l m n o
/* 0x70 */ 'p','q','r','s','t','u','v','w','x','y','z','-','-','-','-','-',  // p q r s t u v w x y z { | } ~ <DEL>
/* 0x80 */ '-','-','-','-','-','-','-','-','-','-','-','-','-','-','-','-',  // <CTRL> ...
/* 0x90 */ '-','-','-','-','-','-','-','-','-','-','-','-','-','-','-','-',  // <CTRL> ...
/* 0xA0 */ '-','-','-','-','-','-','-','-','-','-','-','-','-','-','-','-',  // <NBSP> ¡ ¢ £ ¤ ¥ ¦ § ¨ © ª « ¬ <SOFTHYPEN> ® ¯
/* 0xB0 */ '-','-','-','-','-','-','-','-','-','-','-','-','-','-','-','-',  // ° ± ² ³ ´ µ ¶ · ¸ ¹ º » ¼ ½ ¾ ¿
/* 0xC0 */ 'a','a','a','a','a','a','a','c','e','e','e','e','i','i','i','i',  // À Á Â Ã Ä Å Æ Ç È É Ê Ë Ì Í Î Ï
/* 0xD0 */ 'd','n','o','o','o','o','o','x','o','u','u','u','u','y','-','s',  // Ð Ñ Ò Ó Ô Õ Ö × Ø Ù Ú Û Ü Ý Þ ß
/* 0xE0 */ 'a','a','a','a','a','a','a','c','e','e','e','e','i','i','i','i',  // à á â ã ä å æ ç è é ê ë ì í î ï
/* 0xF0 */ 'd','n','o','o','o','o','o','-','o','u','u','u','u','y','-','y',  // ð ñ ò ó ô õ ö ÷ ø ù ú û ü ý þ ÿ
};


static size_t WBufAppendSlug(WBuf* b, const char* pch, size_t len) {
  WBufReserve(b, len);
  const char* start = b->ptr;
  char c = 0, pc = 0;
  for (size_t i = 0; i < len; i++) {
    u8 x = (u8)pch[i];
    if (x >= 0x80) {
      // decode UTF8-encoded character as Latin-1
      if ((x >> 5) == 0x6 && i+1 < len) {
        u32 cp = ((x << 6) & 0x7ff) + ((pch[++i]) & 0x3f);
        x = cp <= 0xFF ? cp : 0;
      } else {
        x = 0;
      }
    }
    c = slugMap[x];
    if (c != '-' || (pc != '-' && pc)) {
      // note: check "pc" to trim leading '-'
      *(b->ptr++) = c;
      pc = c;
    }
  }
  if (pc == '-') {
    // trim trailing '-'
    b->ptr--;
  }
  return b->ptr - start;
}


static void render_attribute(FmtHTML* r, const MD_ATTRIBUTE* attr) {
  int i;
  for (i = 0; attr->substr_offsets[i] < attr->size; i++) {
    MD_TEXTTYPE type = attr->substr_types[i];
    MD_OFFSET   off  = attr->substr_offsets[i];
    MD_SIZE     size = attr->substr_offsets[i+1] - off;
    const MD_CHAR* text = attr->text + off;
    switch (type) {
      case MD_TEXT_NULLCHAR: render_text(r, ucReplacementUTF8, sizeof(ucReplacementUTF8)); break;
      case MD_TEXT_ENTITY:   render_text(r, text, size); break;
      default:               render_html_escaped(r, text, size); break;
    }
  }
}


static void render_open_ol_block(FmtHTML* r, const MD_BLOCK_OL_DETAIL* det) {
  if (det->start == 1) {
    render_literal(r, "<ol>\n");
  } else {
    render_literal(r, "<ol start=\"");
    WBufAppendU32(r->outbuf, det->start, 10);
    render_literal(r, "\">\n");
  }
}

static void render_open_li_block(FmtHTML* r, const MD_BLOCK_LI_DETAIL* det) {
  if (det->is_task) {
    render_literal(r, "<li class=\"task-list-item\"><input type=\"checkbox\" disabled");
    if (det->task_mark == 'x' || det->task_mark == 'X') {
      render_literal(r, " checked");
    }
    render_char(r, '>');
  } else {
    render_literal(r, "<li>");
  }
}

static void render_open_code_block(FmtHTML* r, const MD_BLOCK_CODE_DETAIL* det) {
  render_literal(r, "<pre><code");
  if (det->lang.text != NULL) {
    render_literal(r, " class=\"language-");
    render_attribute(r, &det->lang);
    render_char(r, '"');
  }
  render_char(r, '>');
  r->codeBlockNest++;
}

static void render_close_code_block(FmtHTML* r, const MD_BLOCK_CODE_DETAIL* det) {
  dlog("end code block (lang \"%.*s\")", (int)det->lang.size, det->lang.text);

  r->codeBlockNest--;

  if (r->onCodeBlock) {
    const char* text = r->tmpbuf.start;
    size_t len = WBufLen(&r->tmpbuf);

    int outlen = -1;

    if (len <= 0x7FFFFFFF) {
      const char* outptr = NULL;
      outlen = r->onCodeBlock(det->lang.text, (u32)det->lang.size, text, (u32)len, &outptr);
      if (outlen > 0 && outptr != NULL)
        WBufAppendBytes(r->outbuf, outptr, (size_t)outlen);
      if (outptr != NULL)
        free((void*)outptr);
    }

    if (outlen < 0) {
      // The function failed or opted out of taking care of formatting
      render_html_escaped(r, text, len);
    }

    WBufReset(&r->tmpbuf);
  }

  render_literal(r, "</code></pre>\n");
}

static void render_open_td_block(FmtHTML* r, bool isTH, const MD_BLOCK_TD_DETAIL* det) {
  render_text(r, isTH ? "<th" : "<td", 3);
  switch (det->align) {
    case MD_ALIGN_LEFT:   render_literal(r, " align=\"left\">"); break;
    case MD_ALIGN_CENTER: render_literal(r, " align=\"center\">"); break;
    case MD_ALIGN_RIGHT:  render_literal(r, " align=\"right\">"); break;
    default:              render_char(r, '>'); break;
  }
}

static bool is_javascript_uri(const MD_CHAR* text, size_t len) {
  return (
    len >= strlen("javascript:") &&
    strncasecmp(text, "javascript:", strlen("javascript:")) == 0
  );
}

static void render_open_a_span(FmtHTML* r, const MD_SPAN_A_DETAIL* det) {
  render_literal(r, "<a href=\"");
  // skip "javascript:" URIs unless explicitly allowed
  if ((r->flags & OutputFlagAllowJSURI) != 0 ||
      !is_javascript_uri(det->href.text, det->href.size))
  {
    render_attribute(r, &det->href);
  }
  if (det->title.text != NULL) {
    render_literal(r, "\" title=\"");
    render_attribute(r, &det->title);
  }
  render_literal(r, "\">");
}

static void render_open_img_span(FmtHTML* r, const MD_SPAN_IMG_DETAIL* det) {
  render_literal(r, "<img src=\"");
  render_attribute(r, &det->src);
  render_literal(r, "\" alt=\"");
  r->imgnest++;
}

static void render_close_img_span(FmtHTML* r, const MD_SPAN_IMG_DETAIL* det) {
  if(det->title.text != NULL) {
    render_literal(r, "\" title=\"");
    render_attribute(r, &det->title);
  }
  render_literal(r, (r->flags & OutputFlagXHTML) ? "\"/>" : "\">");
  r->imgnest--;
}

static void render_open_wikilink_span(FmtHTML* r, const MD_SPAN_WIKILINK_DETAIL* det) {
  render_literal(r, "<x-wikilink data-target=\"");
  render_attribute(r, &det->target);
  render_literal(r, "\">");
}


/**************************************
 ***  HTML renderer implementation  ***
 **************************************/



static int enter_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata) {
  static const MD_CHAR* head[6] = { "<h1>", "<h2>", "<h3>", "<h4>", "<h5>", "<h6>" };
  FmtHTML* r = (FmtHTML*) userdata;

  switch(type) {
    case MD_BLOCK_DOC:   /* noop */ break;
    case MD_BLOCK_QUOTE: render_literal(r, "<blockquote>\n"); break;
    case MD_BLOCK_UL:    render_literal(r, "<ul>\n"); break;
    case MD_BLOCK_OL:    render_open_ol_block(r, (const MD_BLOCK_OL_DETAIL*)detail); break;
    case MD_BLOCK_LI:    render_open_li_block(r, (const MD_BLOCK_LI_DETAIL*)detail); break;
    case MD_BLOCK_HR:    render_literal(r, (r->flags & OutputFlagXHTML) ? "<hr/>\n" : "<hr>\n"); break;
    case MD_BLOCK_H:
    {
      render_literal(r, head[((MD_BLOCK_H_DETAIL*)detail)->level - 1]);
      r->addanchor = !(r->flags & OutputFlagDisableHeadlineAnchors);
      break;
    }
    case MD_BLOCK_CODE:  render_open_code_block(r, (const MD_BLOCK_CODE_DETAIL*) detail); break;
    case MD_BLOCK_HTML:  /* noop */ break;
    case MD_BLOCK_P:     render_literal(r, "<p>"); break;
    case MD_BLOCK_TABLE: render_literal(r, "<table>\n"); break;
    case MD_BLOCK_THEAD: render_literal(r, "<thead>\n"); break;
    case MD_BLOCK_TBODY: render_literal(r, "<tbody>\n"); break;
    case MD_BLOCK_TR:    render_literal(r, "<tr>\n"); break;
    case MD_BLOCK_TH:    render_open_td_block(r, true, (MD_BLOCK_TD_DETAIL*)detail); break;
    case MD_BLOCK_TD:    render_open_td_block(r, false, (MD_BLOCK_TD_DETAIL*)detail); break;
  }

  return 0;
}

static int leave_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata) {
  static const MD_CHAR* head[6] = { "</h1>\n", "</h2>\n", "</h3>\n", "</h4>\n", "</h5>\n", "</h6>\n" };
  FmtHTML* r = (FmtHTML*) userdata;

  switch(type) {
    case MD_BLOCK_DOC:   /*noop*/ break;
    case MD_BLOCK_QUOTE: render_literal(r, "</blockquote>\n"); break;
    case MD_BLOCK_UL:    render_literal(r, "</ul>\n"); break;
    case MD_BLOCK_OL:    render_literal(r, "</ol>\n"); break;
    case MD_BLOCK_LI:    render_literal(r, "</li>\n"); break;
    case MD_BLOCK_HR:    /*noop*/ break;
    case MD_BLOCK_H:     render_literal(r, head[((MD_BLOCK_H_DETAIL*)detail)->level - 1]); break;
    case MD_BLOCK_CODE:  render_close_code_block(r, (const MD_BLOCK_CODE_DETAIL*)detail); break;
    case MD_BLOCK_HTML:  /* noop */ break;
    case MD_BLOCK_P:     render_literal(r, "</p>\n"); break;
    case MD_BLOCK_TABLE: render_literal(r, "</table>\n"); break;
    case MD_BLOCK_THEAD: render_literal(r, "</thead>\n"); break;
    case MD_BLOCK_TBODY: render_literal(r, "</tbody>\n"); break;
    case MD_BLOCK_TR:    render_literal(r, "</tr>\n"); break;
    case MD_BLOCK_TH:    render_literal(r, "</th>\n"); break;
    case MD_BLOCK_TD:    render_literal(r, "</td>\n"); break;
  }

  return 0;
}

static int enter_span_callback(MD_SPANTYPE type, void* detail, void* userdata) {
  FmtHTML* r = (FmtHTML*) userdata;

  if(r->imgnest > 0) {
    /* We are inside a Markdown image label. Markdown allows to use any
     * emphasis and other rich contents in that context similarly as in
     * any link label.
     *
     * However, unlike in the case of links (where that contents becomes
     * contents of the <a>...</a> tag), in the case of images the contents
     * is supposed to fall into the attribute alt: <img alt="...">.
     *
     * In that context we naturally cannot output nested HTML tags. So lets
     * suppress them and only output the plain text (i.e. what falls into
     * text() callback).
     *
     * This make-it-a-plain-text approach is the recommended practice by
     * CommonMark specification (for HTML output).
     */
    return 0;
  }

  switch(type) {
    case MD_SPAN_EM:                render_literal(r, "<em>"); break;
    case MD_SPAN_STRONG:            render_literal(r, "<b>"); break;
    case MD_SPAN_U:                 render_literal(r, "<u>"); break;
    case MD_SPAN_A:                 render_open_a_span(r, (MD_SPAN_A_DETAIL*)detail); break;
    case MD_SPAN_IMG:               render_open_img_span(r, (MD_SPAN_IMG_DETAIL*)detail); break;
    case MD_SPAN_CODE:              render_literal(r, "<code>"); break;
    case MD_SPAN_DEL:               render_literal(r, "<del>"); break;
    case MD_SPAN_LATEXMATH:         render_literal(r, "<x-equation>"); break;
    case MD_SPAN_LATEXMATH_DISPLAY: render_literal(r, "<x-equation type=\"display\">"); break;
    case MD_SPAN_WIKILINK:          render_open_wikilink_span(r, (MD_SPAN_WIKILINK_DETAIL*) detail); break;
  }

  return 0;
}

static int leave_span_callback(MD_SPANTYPE type, void* detail, void* userdata) {
  FmtHTML* r = (FmtHTML*) userdata;

  if(r->imgnest > 0) {
    /* Ditto as in enter_span_callback(), except we have to allow the
     * end of the <img> tag. */
    if(r->imgnest == 1  &&  type == MD_SPAN_IMG)
      render_close_img_span(r, (MD_SPAN_IMG_DETAIL*) detail);
    return 0;
  }

  switch(type) {
    case MD_SPAN_EM:                render_literal(r, "</em>"); break;
    case MD_SPAN_STRONG:            render_literal(r, "</b>"); break;
    case MD_SPAN_U:                 render_literal(r, "</u>"); break;
    case MD_SPAN_A:                 render_literal(r, "</a>"); break;
    case MD_SPAN_IMG:               /*noop, handled above*/ break;
    case MD_SPAN_CODE:              render_literal(r, "</code>"); break;
    case MD_SPAN_DEL:               render_literal(r, "</del>"); break;
    case MD_SPAN_LATEXMATH:         /*fall through*/
    case MD_SPAN_LATEXMATH_DISPLAY: render_literal(r, "</x-equation>"); break;
    case MD_SPAN_WIKILINK:          render_literal(r, "</x-wikilink>"); break;
  }

  return 0;
}

static int text_callback(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata) {
  FmtHTML* r = (FmtHTML*) userdata;

  if (r->codeBlockNest && r->onCodeBlock) {
    WBufAppendBytes(&r->tmpbuf, text, size);
    return 0;
  }

  if (r->addanchor) {
    r->addanchor = 0;
    if (type != MD_TEXT_NULLCHAR && type != MD_TEXT_BR && type != MD_TEXT_SOFTBR) {
      render_literal(r, "<a id=\"");

      const char* slugptr = r->outbuf->ptr;
      size_t sluglen = WBufAppendSlug(r->outbuf, text, size);

      render_literal(r, "\" class=\"anchor\" aria-hidden=\"true\" href=\"#");

      if (sluglen > 0) {
        WBufReserve(r->outbuf, sluglen);
        memcpy(r->outbuf->ptr, slugptr, sluglen);
        r->outbuf->ptr += sluglen;
      }

      render_literal(r, "\"></a>");
    }
  }

  switch (type) {
    case MD_TEXT_NULLCHAR:  render_text(r, ucReplacementUTF8, sizeof(ucReplacementUTF8)); break;
    case MD_TEXT_BR:
      render_literal(
        r,
        r->imgnest == 0 ?
          ((r->flags & OutputFlagXHTML) ? "<br/>\n" : "<br>\n") :
          " "
      );
      break;

    render_literal(r, (r->flags & OutputFlagXHTML) ? "<hr/>\n" : "<hr>\n"); break;

    case MD_TEXT_SOFTBR:    render_literal(r, (r->imgnest == 0 ? "\n" : " ")); break;
    case MD_TEXT_HTML:      render_text(r, text, size); break;
    case MD_TEXT_ENTITY:    render_text(r, text, size); break;
    default:                render_html_escaped(r, text, size); break;
  }

  return 0;
}

// static void debug_log_callback(const char* msg, void* userdata) {
//   dlog("MD4C: %s\n", msg);
// }

int fmt_html(const MD_CHAR* input, MD_SIZE input_size, FmtHTML* fmt) {
  fmt->imgnest = 0;
  fmt->addanchor = 0;
  fmt->codeBlockNest = 0;
  fmt->tmpbuf = (WBuf){0};

  MD_PARSER parser = {
    0,
    fmt->parserFlags,
    enter_block_callback,
    leave_block_callback,
    enter_span_callback,
    leave_span_callback,
    text_callback,
    NULL, // debug_log_callback,
    NULL
  };

  WBufInit(&fmt->tmpbuf);

  int res = md_parse(input, input_size, &parser, (void*)fmt);

  WBufFree(&fmt->tmpbuf);

  return res;
}
