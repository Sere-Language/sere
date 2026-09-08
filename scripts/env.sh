#!/usr/bin/env bash
# Source this file to configure the current Bash session.
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "Run: source scripts/env.sh" >&2
  exit 1
fi
_sere_env() {
  if [[ "$(uname -s)" != Linux ]]; then
    echo "env.sh requires Linux; on Windows use scripts/env.ps1." >&2
    return 1
  fi
  local repo llvm tool
  repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)" || return
  llvm="${SERE_LLVM_DIR:-${SERE_TOOLCHAIN_ROOT:-${XDG_DATA_HOME:-$HOME/.local/share}/sere/toolchains}/llvm-22.1.8}"
  for tool in clang clang++ ld.lld llvm-ar llvm-ranlib; do
    if [[ ! -x "$llvm/bin/$tool" ]]; then
      echo "Missing $llvm/bin/$tool. Run bash scripts/bootstrap-llvm.sh or set SERE_LLVM_DIR." >&2
      return 1
    fi
  done
  if [[ ! -f "$llvm/lib/cmake/llvm/LLVMConfig.cmake" ]]; then
    echo "SERE_LLVM_DIR must contain LLVM development headers and libraries." >&2
    return 1
  fi
  export SERE_LLVM_DIR="$llvm"
  export SERE_STDLIB="$repo/stdlib"
  case ":$PATH:" in
    *":$llvm/bin:"*) ;;
    *) export PATH="$llvm/bin:$PATH" ;;
  esac
}
if _sere_env; then
  unset -f _sere_env
else
  unset -f _sere_env
  return 1
fi
