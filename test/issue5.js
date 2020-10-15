// https://github.com/rsms/markdown-wasm/issues/5
const libdir = process.argv.includes("-debug") ? "build/debug" : "dist"
const md = require(`../${libdir}/markdown.node.js`)

// this triggers the bug
const input1 = Buffer.from(
  "```\n" +
  "                 |\n" +
  "```"
, "utf8")
const expected1 = Buffer.from(
  "<pre><code>" +
  "                 |\n" +
  "</code></pre>\n"
, "utf8")

// this does not (indent is 1 less space)
const input2 = Buffer.from(
  "```\n" +
  "                |\n" +
  "```"
, "utf8")
const expected2 = Buffer.from(
  "<pre><code>" +
  "                |\n" +
  "</code></pre>\n"
, "utf8")

// this does not (indent is 1 more space)
const input3 = Buffer.from(
  "```\n" +
  "                  |\n" +
  "```"
, "utf8")
const expected3 = Buffer.from(
  "<pre><code>" +
  "                  |\n" +
  "</code></pre>\n"
, "utf8")

let fails = 0
check("test1", expected1, input1)
check("test2", expected2, input2)
check("test3", expected3, input3)


function check(name, expected, inputData) {
  const actual = Buffer.from(md.parse(inputData, { asMemoryView: true }))

  if (expected.compare(actual) == 0) {
    console.log(`${name} OK`)
    return
  }
  fails++
  const line = "——————————————————————————————————————————————————"
  const wave = "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
  console.error(`${name} FAIL.`)

  console.error(`\n\nExpected output:\n${line}`)
  inspectBuf(expected, actual)
  console.error(`${line}\n\nActual output:\n${line}`)
  inspectBuf(actual, expected)
  console.error(line)

  function inspectBuf(buf, otherbuf) {
    process.stderr.write(buf)
    if (buf[buf.length-1] != 0x0a) {
      process.stderr.write("<no-ending-line-break>\n")
    }
    console.error(wave)
    const styleReset = "\x1b[22;39m"
    const styleNone = s=>s
    const styleDiff = process.stderr.isTTY ? s => "\x1b[1;33m"+s+styleReset : styleNone
    const styleErr  = process.stderr.isTTY ? s => "\x1b[1;31m"+s+styleReset : styleNone

    for (let i = 0; i < buf.length; i++) {
      let b = buf[i]

      let style = styleNone
      if (b < 0x20 && b != 0x09 && b != 0x0A && b != 0x0D) {
        // byte is unexpected control character (except TAB, CR, LF)
        style = styleErr
      } else if (otherbuf && otherbuf[i] != b) {
        style = styleDiff
      }

      process.stderr.write(style(b.toString(16).padStart(2, '0')) + " ")

      if (b == 0x0a) {
        process.stderr.write("\n")
      }
    }
  }
}

process.exit(fails > 0 ? 1 : 0)
