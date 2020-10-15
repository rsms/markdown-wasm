#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

node issue5.js "$@"
