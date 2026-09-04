#!/usr/bin/env bash
# Downloads LLVM 22.1.8 for Linux x86_64 (Ubuntu 22.04 baseline).
set -euo pipefail

LLVM_VERSION="${LLVM_VERSION:-22.1.8}"
LLVM_TAG="llvmorg-${LLVM_VERSION}"
# Official 22.1.8 Linux x64 toolchain+devel archive:
# https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.8
ARCHIVE_NAME="LLVM-${LLVM_VERSION}-Linux-X64.tar.xz"
URL="https://github.com/llvm/llvm-project/releases/download/${LLVM_TAG}/${ARCHIVE_NAME}"

DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}"
TOOLCHAIN_ROOT="${SERE_TOOLCHAIN_ROOT:-${DATA_HOME}/sere/toolchains}"
ARCHIVE_PATH="${TOOLCHAIN_ROOT}/${ARCHIVE_NAME}"
INSTALL_DIR="${TOOLCHAIN_ROOT}/llvm-${LLVM_VERSION}"

mkdir -p "${TOOLCHAIN_ROOT}"

llvm_ok() {
  local root="$1"
  [[ -x "${root}/bin/clang" && -f "${root}/lib/cmake/llvm/LLVMConfig.cmake" && -x "${root}/bin/ld.lld" ]]
}

if llvm_ok "${INSTALL_DIR}"; then
  echo "LLVM ${LLVM_VERSION} already installed at ${INSTALL_DIR}"
else
  if [[ ! -f "${ARCHIVE_PATH}" ]] || [[ "$(stat -c%s "${ARCHIVE_PATH}" 2>/dev/null || echo 0)" -lt 100000000 ]]; then
    echo "Downloading ${URL}"
    curl -L --fail --retry 3 --retry-delay 5 -o "${ARCHIVE_PATH}" "${URL}"
  fi
  staging="${TOOLCHAIN_ROOT}/tmp-extract-${LLVM_VERSION}"
  rm -rf "${staging}"
  mkdir -p "${staging}"
  echo "Extracting ${ARCHIVE_PATH}"
  tar -xf "${ARCHIVE_PATH}" -C "${staging}"
  inner="$(find "${staging}" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
  if [[ -z "${inner}" ]]; then
    echo "extracted archive had no directory" >&2
    exit 1
  fi
  rm -rf "${INSTALL_DIR}"
  mv "${inner}" "${INSTALL_DIR}"
  rm -rf "${staging}"
  if ! llvm_ok "${INSTALL_DIR}"; then
    echo "LLVM extract succeeded but clang/LLVMConfig.cmake/ld.lld were not found in ${INSTALL_DIR}" >&2
    exit 1
  fi
fi

export SERE_LLVM_DIR="${INSTALL_DIR}"
echo
echo "LLVM ${LLVM_VERSION} is ready:"
echo "  SERE_LLVM_DIR=${SERE_LLVM_DIR}"
echo "  clang=$("${SERE_LLVM_DIR}/bin/clang" --version | head -n 1)"
echo "  ld.lld=$("${SERE_LLVM_DIR}/bin/ld.lld" --version | head -n 1)"
