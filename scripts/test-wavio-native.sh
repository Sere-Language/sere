#!/usr/bin/env bash
# Standalone native regression/ASan+UBSan harness; does not need the Sere compiler.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$root/build/wavio-native-sanitized"
mkdir -p "$build"
cc="${CC:-clang}"
cxx="${CXX:-clang++}"
flags=(-g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined)
for unit in miniaudio buffer codec engine dsp; do
  "$cc" -std=c17 -D_GNU_SOURCE "${flags[@]}" -I"$root/runtime" \
    -c "$root/runtime/wavio/$unit.c" -o "$build/$unit.o"
done
"$cxx" -std=c++20 "${flags[@]}" -I"$root/runtime" "$root/tests/WavioTest.cpp" \
  "$build/miniaudio.o" "$build/buffer.o" "$build/codec.o" "$build/engine.o" \
  "$build/dsp.o" -lm -ldl -lpthread -o "$build/wavio-test"
cd "$build"
ASAN_OPTIONS=detect_leaks=1 ./wavio-test "$@"
