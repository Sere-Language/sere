#!/usr/bin/env bash
# Configure/build from any working directory. Extra arguments go to CMake configure.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "${1:-}" == --help ]]; then
  echo "Usage: bash scripts/build.sh [--test] [CMake configure arguments...]"
  echo "Set CMAKE_BUILD_PARALLEL_LEVEL to limit memory usage."
  exit 0
fi
run_tests=false
if [[ "${1:-}" == --test ]]; then
  run_tests=true
  shift
fi
source "$repo/scripts/env.sh"
for tool in cmake ninja; do
  command -v "$tool" >/dev/null || { echo "Missing $tool; see README Linux build dependencies." >&2; exit 1; }
done
cd "$repo"
preset=linux-clang-relwithdebinfo
cmake --preset "$preset" "$@"
cmake --build --preset "$preset"
if "$run_tests"; then
  ctest --preset "$preset" --output-on-failure
fi
printf '\nCompiler: %s/build/%s/bin/sere\n' "$repo" "$preset"
