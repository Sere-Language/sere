#!/usr/bin/env bash
# Downloads the pinned LLVM development toolchain for Linux x86_64.
set -euo pipefail

if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
  echo "Automatic LLVM setup requires Linux x86_64." >&2
  exit 1
fi
for tool in curl tar xz; do
  command -v "$tool" >/dev/null || { echo "Missing dependency: $tool" >&2; exit 1; }
done
LLVM_VERSION="22.1.8"
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
  if [[ -e "${INSTALL_DIR}" ]]; then
    echo "Incomplete toolchain at ${INSTALL_DIR}; move it aside before retrying." >&2
    exit 1
  fi

  # An interrupted run leaves a partial tree with tens of thousands of files.
  # Reclaim it before the space check so a retry after a full disk can proceed.
  for stale in "${TOOLCHAIN_ROOT}"/.llvm-extract.*; do
    [[ -e "${stale}" ]] || continue
    echo "Removing interrupted extraction ${stale}"
    rm -rf -- "${stale}" 2>/dev/null || echo "warning: could not remove ${stale}" >&2
  done

  # The extracted toolchain is ~5 GB, plus ~1 GB for the archive. Without this
  # check a full disk shows up as thousands of tar errors and can leave the
  # filesystem read-only, which then breaks unrelated commands in the distro.
  needed_kb=$((10 * 1024 * 1024))
  avail_kb="$(df -Pk "${TOOLCHAIN_ROOT}" 2>/dev/null | awk 'NR==2 {print $4}')" || avail_kb=""
  if [[ -n "${avail_kb}" ]] && (( avail_kb < needed_kb )); then
    echo "Not enough free space to install LLVM ${LLVM_VERSION}." >&2
    echo "  need about 10 GB free in ${TOOLCHAIN_ROOT}" >&2
    echo "  available: $((avail_kb / 1024 / 1024)) GB" >&2
    exit 1
  fi

  staging="$(mktemp -d "${TOOLCHAIN_ROOT}/.llvm-extract.XXXXXXXX")"
  # A read-only or full filesystem makes cleanup very noisy; the failure that
  # matters has already been reported by the time this runs.
  trap 'rm -rf -- "$staging" 2>/dev/null || true' EXIT
  if [[ ! -f "${ARCHIVE_PATH}" ]] || [[ "$(stat -c%s "${ARCHIVE_PATH}" 2>/dev/null || echo 0)" -lt 100000000 ]]; then
    echo "Downloading ${URL}"
    curl -L --fail --retry 3 --retry-delay 5 -o "${staging}/download.tar.xz" "${URL}"
    mv "${staging}/download.tar.xz" "${ARCHIVE_PATH}"
  fi
  echo "Extracting ${ARCHIVE_PATH}"
  # Capture the output: a full or read-only filesystem produces one error per
  # entry, which buries the actual problem in thousands of lines.
  if ! tar_log="$(tar -xf "${ARCHIVE_PATH}" -C "${staging}" 2>&1)"; then
    echo "Extracting ${ARCHIVE_PATH} failed:" >&2
    printf '%s\n' "${tar_log}" | head -n 20 >&2
    if grep -qE 'Read-only file system|No space left on device' <<<"${tar_log}"; then
      echo >&2
      echo "The filesystem ran out of space or was remounted read-only." >&2
      echo "Inside WSL the toolchain lives on the ext4 disk image, which is backed" >&2
      echo "by a Windows drive; free space there, run 'wsl --shutdown', then retry." >&2
    fi
    exit 1
  fi
  inner="$(find "${staging}" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
  if [[ -z "${inner}" ]]; then
    echo "extracted archive had no directory" >&2
    exit 1
  fi
  if ! llvm_ok "${inner}"; then
    echo "LLVM extract succeeded but clang/LLVMConfig.cmake/ld.lld were not found in ${INSTALL_DIR}" >&2
    exit 1
  fi
  mv "${inner}" "${INSTALL_DIR}"
fi

export SERE_LLVM_DIR="${INSTALL_DIR}"
echo
echo "LLVM ${LLVM_VERSION} is ready:"
echo "  SERE_LLVM_DIR=${SERE_LLVM_DIR}"
echo "  clang=$("${SERE_LLVM_DIR}/bin/clang" --version | head -n 1)"
echo "  ld.lld=$("${SERE_LLVM_DIR}/bin/ld.lld" --version | head -n 1)"

echo "Next: source scripts/env.sh (from the repository root)."
