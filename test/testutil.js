const Path = require("path")

const libdir = process.argv.includes("-debug") ? "build/debug" : "dist"
const md = require(`../${libdir}/markdown.node.js`)

const line = "——————————————————————————————————————————————————"
const wave = "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"


exports.numFailures = 0


exports.checkHTMLResult = function check(name, inputData, expectedOutputData) {
  if (typeof inputData == "string") {
    inputData = Buffer.from(inputData, "utf8")
  }
  if (typeof expectedOutputData == "string") {
    expectedOutputData = Buffer.from(expectedOutputData, "utf8")
  }
  const actual = Buffer.from(md.parse(inputData, { asMemoryView: true }))
  if (expectedOutputData.compare(actual) == 0) {
    log(`${name} OK`)
    return true
  }
  exports.numFailures++
  logerr(`${name} FAIL`)
  console.error(`\n\nExpected output:\n${line}`)
  inspectBuf(expectedOutputData, actual)
  console.error(`${line}\n\nActual output:\n${line}`)
  inspectBuf(actual, expectedOutputData)
  console.error(line)
}


exports.exit = function() {
  process.exit(exports.numFailures > 0 ? 1 : 0)
}


exports.log = log
exports.logerr = logerr

const logprefix = Path.basename(process.argv[1])+":"

function log() {
  console.log.apply(console, [logprefix].concat([].slice.call(arguments)))
}
function logerr() {
  console.error.apply(console, [logprefix].concat([].slice.call(arguments)))
}


exports.inspectBuf = inspectBuf

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
