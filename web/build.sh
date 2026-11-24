#!/bin/sh
# Builds the free web edition into <out>: index.html, app.js, worklet.js, ui.js + ui.wasm (the panel), dsp.wasm
# (the engine). Needs Emscripten (em++) and NanoVG's sources (iPlug2/Dependencies/IGraphics/NanoVG/src).
#   web/build.sh <nanovg src dir> <out dir>          (run from the repository's top folder)
set -e
NVG=$1; OUT=$2
[ -n "$NVG" ] && [ -n "$OUT" ] || { echo "usage: web/build.sh <nanovg src dir> <out dir>"; exit 1; }
mkdir -p "$OUT"
# the sound: the engine as a standalone WebAssembly module for the audio worklet (no JavaScript glue)
em++ -O3 -std=c++17 -fno-exceptions -fno-rtti -DMS20_RAMFILE=1 -sSTANDALONE_WASM=1 --no-entry \
  -sINITIAL_MEMORY=16777216 -sALLOW_MEMORY_GROWTH=0 -o "$OUT/dsp.wasm" web/dsp.cpp engine/engine_globals.cpp
# the panel: ui/StudioUI.h on the Linux app's IGraphics stand-in, NanoVG on WebGL
emcc -O2 -c "$NVG/nanovg.c" -I "$NVG" -o "$OUT/nanovg.o"
em++ -O2 -std=c++17 -fno-exceptions -fno-rtti -DTE_NO_POPUP=1 -I web -I linux/shim -I . -I "$NVG" web/ui.cpp "$OUT/nanovg.o" \
  -sMODULARIZE=1 -sEXPORT_NAME=TwentyEngineUI -sENVIRONMENT=web -sALLOW_MEMORY_GROWTH=1 -sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=1 \
  -sFILESYSTEM=1 "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','stringToUTF8']" \
  "-sEXPORTED_FUNCTIONS=['_te_ui_init','_te_ui_frame','_te_ui_mouse','_te_ui_engine','_te_ui_leds','_te_ui_status','_te_ui_midi','_te_ui_ccmap','_te_ui_kw','_te_ui_kh','_malloc','_free']" \
  -o "$OUT/ui.js"
rm -f "$OUT/nanovg.o"
cp web/index.html web/app.js web/worklet.js "$OUT/"
ls -la "$OUT"
