#!/bin/bash
# Builds the engine and runs its Node tests. Needs Emscripten and Node.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
OUT=${1:-$(cd "$HERE/../.." && pwd)/build-preview}

"$HERE/build.sh" "$OUT"
PREVIEW_BUILD=$OUT node --test "$HERE"/test/*.test.mjs
