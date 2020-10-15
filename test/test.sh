#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

for f in issue*.js; do
  node "$f" "$@"
done
