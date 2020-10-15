const fs = require("fs")
const markdown = require('../../dist/markdown.node.js')
const { exit } = require("../testutil")

// https://spec.commonmark.org
const source = fs.readFileSync(__dirname + "/spec.md")

const timeLabel = `markdown.parse("spec.md")`
console.time(timeLabel)
let html = markdown.parse(source)
console.timeEnd(timeLabel)

html = `
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Markdown spec</title>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <body>
  ${html}
  </body>
</html>
`.trim()

const outfile = __dirname + "/spec.html"
fs.writeFileSync(outfile, html, "utf8")
exit()
