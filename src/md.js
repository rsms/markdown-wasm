import {
  utf8,
  withTmpBytePtr,
  withOutPtr,
  werrCheck,
  mallocbuf,
} from "./wlib"

export const ready = Module.ready

// console.time('wasm load')
// Module.postRun.push(() => {
//   console.timeEnd('wasm load')
// })

export const ParseFlags = {
  COLLAPSE_WHITESPACE:         0x0001, // In TEXT, collapse non-trivial whitespace into single ' '
  PERMISSIVE_ATX_HEADERS:      0x0002, // Do not require space in ATX headers ( ###header )
  PERMISSIVE_URL_AUTO_LINKS:   0x0004, // Recognize URLs as links even without <...>
  PERMISSIVE_EMAIL_AUTO_LINKS: 0x0008, // Recognize e-mails as links even without <...>
  NO_INDENTED_CODE_BLOCKS:     0x0010, // Disable indented code blocks. (Only fenced code works)
  NO_HTML_BLOCKS:              0x0020, // Disable raw HTML blocks.
  NO_HTML_SPANS:               0x0040, // Disable raw HTML (inline).
  TABLES:                      0x0100, // Enable tables extension.
  STRIKETHROUGH:               0x0200, // Enable strikethrough extension.
  PERMISSIVE_WWW_AUTOLINKS:    0x0400, // Enable WWW autolinks (without proto; just 'www.')
  TASK_LISTS:                  0x0800, // Enable task list extension.
  LATEX_MATH_SPANS:            0x1000, // Enable $ and $$ containing LaTeX equations.
  WIKI_LINKS:                  0x2000, // Enable wiki links extension.
  UNDERLINE:                   0x4000, // Enable underline extension (disables '_' for emphasis)

  // Github style default flags
  DEFAULT: 0x0001 | 0x0002 | 0x0004 | 0x0200 | 0x0100 | 0x0800,
    // COLLAPSE_WHITESPACE
    // PERMISSIVE_ATX_HEADERS
    // PERMISSIVE_URL_AUTO_LINKS
    // STRIKETHROUGH
    // TABLES
    // TASK_LISTS

  NO_HTML: 0x0020 | 0x0040, // NO_HTML_BLOCKS | NO_HTML_SPANS
}

// these should be in sync with "OutputFlags" in common.h
const OutputFlags = {
  HTML:       1 << 0, // Output HTML
  XHTML:      1 << 1, // Output XHTML (only has effect with HTML flag set)
  AllowJSURI: 1 << 2, // Allow "javascript:" URIs
}


export function parse(source, options) {
  options = options || {}

  let parseFlags = (
    options.parseFlags === undefined ? ParseFlags.DEFAULT :
    options.parseFlags
  )

  let outputFlags = options.allowJSURIs ? OutputFlags.AllowJSURI : 0

  switch (options.format) {
    case "xhtml":
      outputFlags |= OutputFlags.HTML | OutputFlags.XHTML
      break

    case "html":
    case undefined:
    case null:
    case "":
      outputFlags |= OutputFlags.HTML
      break

    default:
      throw new Error(`invalid format "${options.format}"`)
  }

  let onCodeBlockPtr = options.onCodeBlock ? create_onCodeBlock_fn(options.onCodeBlock) : 0

  let buf = as_byte_array(source)
  let outbuf = withOutPtr(outptr => withTmpBytePtr(buf, (inptr, inlen) =>
    _parseUTF8(inptr, inlen, parseFlags, outputFlags, outptr, onCodeBlockPtr)
  ))

  if (options.onCodeBlock)
    removeFunction(onCodeBlockPtr)

  // check for error and throw if needed
  werrCheck()

  // DEBUG
  // if (outbuf) {
  //   console.log(utf8.decode(outbuf))
  // }

  if (options.bytes || options.asMemoryView)
    return outbuf

  return utf8.decode(outbuf)
}


function create_onCodeBlock_fn(onCodeBlock) {
  // See https://emscripten.org/docs/porting/connecting_cpp_and_javascript/
  //   Interacting-with-code.html#calling-javascript-functions-as-function-pointers-from-c
  //
  // Function's C type: JSTextFilterFun
  // (metaptr ptr, metalen ptr, inptr ptr, inlen ptr, outptr ptr) -> outlen int
  const fnptr = addFunction(function(metaptr, metalen, inptr, inlen, outptr) {
    try {
      // lang is the "language" tag, if any, provided with the code block
      const lang = metalen > 0 ? utf8.decode(HEAPU8.subarray(metaptr, metaptr + metalen)) : ""

      // body is a view into heap memory of the segment of source (UTF8 bytes)
      const body = HEAPU8.subarray(inptr, inptr + inlen)
      let bodystr = undefined
      body.toString = () => (bodystr || (bodystr = utf8.decode(body)))

      // result is the result from the onCodeBlock function
      let result = null
      result = onCodeBlock(lang, body)

      if (result === null || result === undefined) {
        // Callback indicates that it does not wish to filter.
        // The md.c implementation will html-encode the body.
        return -1
      }

      let resbuf = as_byte_array(result)
      if (resbuf.length > 0) {
        // copy resbuf to WASM heap memory
        const resptr = mallocbuf(resbuf, resbuf.length)
        // write pointer value
        HEAPU32[outptr >> 2 /* == outptr / 4 */] = resptr
        // Note: fmt_html.c calls free(resptr)
      }

      return resbuf.length
    } catch (err) {
      console.error(`error in markdown onCodeBlock callback: ${err.stack||err}`)
      return -1
    }
  }, "iiiiii")
  return fnptr
}


function as_byte_array(something) {
  if (typeof something == "string")
    return utf8.encode(something)
  if (something instanceof Uint8Array)
    return something
  return new Uint8Array(something)
}
