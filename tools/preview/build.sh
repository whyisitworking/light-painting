#!/bin/bash
# Compiles the firmware's portable modules and the preview engine to
# WebAssembly, one file: OUTDIR/engine.js (default build-preview/), which
# defines createEngine with the wasm embedded. Needs Emscripten.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "$HERE/common.sh"
require_emcc

OUT=${1:-$ROOT/build-preview}
mkdir -p "$OUT"

emcc "${EMCC_FLAGS[@]}" "${MODULE_INC[@]}" \
    -iquote "$HERE" -iquote "$ROOT/app/ui" \
    "${MODULE_SRC[@]}" "$ROOT/app/ui/ui_names.c" "$HERE/preview_api.c" \
    -sMODULARIZE=1 -sEXPORT_NAME=createEngine -sSINGLE_FILE=1 \
    -sENVIRONMENT=web,node -sFILESYSTEM=0 \
    -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAP16,HEAPF32,UTF8ToString \
    -o "$OUT/engine.js"

echo "engine: $OUT/engine.js"
