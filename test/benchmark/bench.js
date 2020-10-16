#!/usr/bin/env node
const Benchmark = require('benchmark').Benchmark
const fs = require('fs')
const Path = require('path')

const commonmark = require('commonmark')
const Showdown = require('showdown')
const marked = require('marked')
const markdownit = require('markdown-it')('commonmark')
const markdown_wasm = require('../../dist/markdown.node.js')

// setup markdownit
// disable expensive IDNa links encoding:
const markdownit_encode = markdownit.utils.lib.mdurl.encode;
markdownit.normalizeLink = function(url) { return markdownit_encode(url); };
markdownit.normalizeLinkText = function(str) { return str; };

// setup showdown
var showdown = new Showdown.Converter()

// setup commonmark
var parser = new commonmark.Parser()
var renderer = new commonmark.HtmlRenderer()

// parse CLI input
let filename = process.argv[2]
if (!filename) {
  console.error(`usage: bench.js <markdown-file>`)
  console.error(`usage: bench.js <dir-of-markdown-files>`)
  process.exit(1)
}

// print CSV header
console.log(csv(["library","file","ops/sec","filesize"]))

// run tests on all files in a directory or a single file
let st = fs.statSync(filename)
if (st.isDirectory()) {
  process.chdir(filename)
  for (let fn of fs.readdirSync(".")) {
    benchmarkFile(fn)
  }
} else {
  benchmarkFile(filename)
}

// Benchmark.options.maxTime = 10

function csv(values) {
  return values.map(s => String(s).replace(/,/g,"\\,")).join(",")
}

function benchmarkFile(benchfile) {

  var contents = fs.readFileSync(benchfile, 'utf8');
  var contentsBuffer = fs.readFileSync(benchfile);

  let csvLinePrefix = `${benchfile.replace(/,/g,"\\,")},${contentsBuffer.length},`

  new Benchmark.Suite({
    onCycle(ev) {
      let b = ev.target
      // console.log("cycle", b)
      console.log(csv([b.name, benchfile, b.hz, contentsBuffer.length]))
    },
    // onComplete(ev) {
    //   let b = ev.target
    //   console.log("onComplete", {ev}, b.stats, b.times)
    // }
  }).add('commonmark', function() {
    renderer.render(parser.parse(contents));
  })
  .add('showdown', function() {
    showdown.makeHtml(contents);
  })
  .add('marked', function() {
    marked(contents);
  })
  .add('markdown-it', function() {
    markdownit.render(contents);
  })
  .add('markdown-wasm/string', function() {
    markdown_wasm.parse(contents);
  })
  .add('markdown-wasm/bytes', function() {
    markdown_wasm.parse(contentsBuffer, {asMemoryView:true});
  })
  .run();
}
