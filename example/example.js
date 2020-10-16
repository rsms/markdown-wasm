const fs = require("fs")
const md = require("../dist/markdown.node.js")

const source = fs.readFileSync(__dirname + "/example.md")
const outbuf = md.parse(source, { bytes: true })
const outfile = __dirname + "/example.html"
console.log("write", outfile)
fs.writeFileSync(outfile, outbuf)

console.log(fs.readFileSync(outfile, "utf8"))

// mini benchmark
if (process.argv.includes("-bench")) {
  console.log("benchmark start (sampling ~2s of data)")
  const timeStart = Date.now()
  const N = 1000, T = 2000
  let ntotal = 0
  while (Date.now() - timeStart < 2000) {
    for (let i = 0; i < N; i++) {
      global["dont-optimize-away"] = md.parse(source, { bytes: true })
    }
    ntotal += N
  }
  const timeSpent = Date.now() - timeStart
  console.log(`benchmark end -- avg parse time: ${((timeSpent / ntotal) * 1000).toFixed(1)}us`)
}
