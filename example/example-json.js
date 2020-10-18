const fs = require("fs")
const md = require("../build/debug/markdown.node.js")

const source = fs.readFileSync(__dirname + "/example2.md")
const json = md.parse(source, { format: "json" })
console.log(json)
