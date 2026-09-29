# Shared by the WebAssembly builds of tools/preview. Source it after setting
# HERE to the directory of the script that sources it.

ROOT=$(cd "$HERE/../.." && pwd)

require_emcc() {
    command -v emcc >/dev/null || {
        echo "Emscripten (emcc) not found: brew install emscripten" >&2
        exit 1
    }
}

# gnu23 like CMake's default: strict c23 hides M_PI. fft.c wants
# -fcx-limited-range, applied to all of it here
EMCC_FLAGS=(-std=gnu23 -O2 -Wall -Wextra -fcx-limited-range)

# The portable modules the engine needs, every source of each: a new file in
# one of them is picked up without touching this list
MODULES=(fft spectrum features effects color settings visualizer)
MODULE_INC=()
MODULE_SRC=()
for module in "${MODULES[@]}"; do
    MODULE_INC+=(-iquote "$ROOT/lib/$module")
    for file in "$ROOT/lib/$module"/*.c; do
        MODULE_SRC+=("$file")
    done
done
