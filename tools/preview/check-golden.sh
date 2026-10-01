#!/bin/bash
# The firmware's golden test, compiled to WebAssembly and run under Node: the
# portable modules compile with Emscripten and give the same pixels as the
# host build. Needs Emscripten; not part of ctest.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "$HERE/common.sh"
require_emcc

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

emcc "${EMCC_FLAGS[@]}" "${MODULE_INC[@]}" -iquote "$ROOT/tests" \
    "${MODULE_SRC[@]}" "$ROOT/tests/test_golden.c" -o "$TMP/golden.js"

node "$TMP/golden.js"
