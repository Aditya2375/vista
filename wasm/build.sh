#!/bin/sh
# Rebuild the browser demo in docs/ (requires emsdk on PATH).
set -e
cd "$(dirname "$0")/.."
emcc -O2 -Isrc -DDEMO_NO_MAIN src/keys.c src/run.c src/surface.c src/widget.c examples/demo.c wasm/vista_wasm.c -o docs/vista.js \
    -sEXPORTED_FUNCTIONS=_vista_new,_vista_free,_vista_key,_vista_w,_vista_h,_vista_cell \
    -sEXPORTED_RUNTIME_METHODS=ccall -sMODULARIZE=1 -sEXPORT_NAME=VistaModule -sALLOW_MEMORY_GROWTH=1
