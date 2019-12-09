const package = require("./package.json")
const outdir = debug ? builddir : "dist"

const m = {
  name:    "markdown",
  out:     outdir + "/markdown.js",
  jsentry: "src/md.js",
  sources: [
    "src/wlib.c",
    "src/wbuf.c",
    "src/md.c",
    "src/md4c.c",
    "src/fmt_html.c",
    // "src/fmt_json.c",
  ],
  cflags: debug ? ["-DDEBUG=1"] : [],
  constants: {
    VERSION: package.version,
  },
}


if (!debug) {
  // generic js module
  module({...m})

  // node cjs module
  module({ ...m,
    name:    "markdown-node",
    out:     outdir + "/markdown.node.js",
    target:  "node",
    embed:   true,
  })
}

// node es module
module({ ...m,
  name:    "markdown-es",
  out:     outdir + "/markdown.es.js",
  format:  "es",
  target:  "node",
  embed:   true,
})
