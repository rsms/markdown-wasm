#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

node spec/spec.js &

for f in issue*.js; do
  node "$f" "$@" &
done

wait
