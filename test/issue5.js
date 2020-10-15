// https://github.com/rsms/markdown-wasm/issues/5
const { checkHTMLResult, exit } = require("./testutil")

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

checkHTMLResult("test1", input1, expected1)
checkHTMLResult("test2", input2, expected2)
checkHTMLResult("test3", input3, expected3)
exit()
