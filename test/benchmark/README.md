# Markdown-wasm benchmarks

This directory contains a benchmark suite.
You'll need nodejs and npm installed.

1. `npm install`
2. `npm run bench`

Running the benchmarks takes a while since in order to be accurate each parse-and-render
operation is performed synchronously on a single CPU thread.

Results are written to the `results` directory; `bench.csv` along with SVG graphs.
