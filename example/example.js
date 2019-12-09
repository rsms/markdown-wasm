const RELEASE = process.argv.includes("-release")

const fs = require("fs")
const md = RELEASE ? require("../build/release/md.js") : require("../build/debug/md.js")

const source = fs.readFileSync(__dirname + "/example.md")
const outbuf = md.parse(source, { asMemoryView: true })
const outfile = __dirname + "/example.html"
console.log("write", outfile)
fs.writeFileSync(outfile, outbuf)

console.log(fs.readFileSync(outfile, "utf8"))

// mini benchmark
if (RELEASE) {
  console.log("benchmark start")
  const timeStart = Date.now()
  const iterations = 10000
  for (let i = 0; i < iterations; i++) {
    global["dont-optimize-away"] = md.parse(source, { asMemoryView: true })
  }
  const timeSpent = Date.now() - timeStart
  console.log(`benchmark end -- avg parse time: ${(timeSpent / iterations).toFixed(2)}ms`)
}
