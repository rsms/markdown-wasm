import { utf8, withTmpBytePtr, withOutPtr, werrCheck } from "./wlib"

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

// these should be in sync with "OutputFlags" in md.c
const OutputFlags = {
  HTML:  1 << 0, // Output HTML
  XHTML: 1 << 1, // Output XHTML (only has effect with HTML flag set)
}

export function parse(source, options) {
  options = options || {}

  let parseFlags = (
    options.parseFlags === undefined ? ParseFlags.DEFAULT :
    options.parseFlags
  )

  let outputFlags = 0
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

  let buf = typeof source == "string" ? utf8.encode(source) : source
  let outbuf = withOutPtr(outptr => withTmpBytePtr(buf, (inptr, inlen) =>
    _parseUTF8(inptr, inlen, parseFlags, outputFlags, outptr)
  ))

  // check for error and throw if needed
  werrCheck()

  // DEBUG
  // if (outbuf) {
  //   console.log(utf8.decode(outbuf))
  // }

  if (options.asMemoryView) {
    return outbuf
  }
  return utf8.decode(outbuf)
}
