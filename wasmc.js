const package = require("./package.json")
const outdir = debug ? builddir : "dist"

cflags = cflags.concat([
  "-std=c11",
  "-Wall",
  "-Wuninitialized",
  "-Wmissing-field-initializers",
  "-Wconditional-uninitialized",
  "-Wno-nullability-completeness",
  "-Wno-unused-function",
  "-fcolor-diagnostics",
])

const m = {
  jsentry: "src/md.js",

  sources: [
    "src/wlib.c",
    "src/wbuf.c",
    "src/md.c",
    "src/md4c.c",
    "src/fmt_html.c",
    debug ? "src/fmt_json.c" : "",
  ].filter(s => !!s),

  cflags: [
    "-DMD4C_USE_UTF8",
  ].concat(debug ? [
    // debug flags
    "-DDEBUG=1",
    "-DASSERTIONS=1", // emcc
    "-DSAFE_HEAP=1", // emcc
    "-DSTACK_OVERFLOW_CHECK=1", // emcc
    "-DDEMANGLE_SUPPORT=1", // emcc
    "-DMD_WITH_JSON=1", // enable WIP json formatter
  ] : [
    // release flags
  ]),
  constants: {
    VERSION: package.version,
  },
}

// —————————————————————————————————————————————————
// products

// embedded wasm, ES module, nodejs-specific compression
// Suitable for using or bundling as a library targeting nodejs only
module({ ...m,
  name:    "markdown-node",
  out:     outdir + "/markdown.node.js",
  target:  "node",
  embed:   true,
})

if (!debug) {
  // sideloaded wasm, universal js library, exports API at global["markdown"] as a fallback
  // Suitable as a runtime library in browsers
  module({ ...m,
    name: "markdown",
    out:  outdir + "/markdown.js",
  })

  // embedded wasm, ES module
  // Suitable for bundling as a library intended for browsers
  module({ ...m,
    name:    "markdown-es",
    out:     outdir + "/markdown.es.js",
    outwasm: outdir + "/markdown.wasm",
    format:  "es",
  })
}
