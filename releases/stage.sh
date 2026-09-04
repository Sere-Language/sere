#!/usr/bin/env bash
# Stage a self-contained Linux x64 distro (compiler + runtime + LLVM + stdlib).
# Run on Linux after a Linux build. Do not cross-compile.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -nE 's/^[[:space:]]*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*$/\1/p' "${ROOT}/CMakeLists.txt" | head -n 1)"
NAME="${1:-pre-${VERSION}}"
[[ "$NAME" =~ ^pre-[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Invalid release name' >&2; exit 2; }
RELEASE="${ROOT}/releases/${NAME}"
DEST="${RELEASE}/linux-x64"
mkdir -p "$RELEASE"
ZIP="${RELEASE}/Sere-${NAME}-linux-x64-portable.zip"
TGZ="${RELEASE}/Sere-${NAME}-linux-x64-portable.tar.gz"
LLVM_VERSION="22.1.8"

find_sere() {
  local candidates=(
    "${ROOT}/bin/sere"
    "${ROOT}/build/linux-clang-relwithdebinfo/bin/sere"
    "${ROOT}/build/linux-clang-relwithdebinfo/bin/staging/sere"
  )
  local newest=""
  local newest_mtime=0
  local path mtime
  for path in "${candidates[@]}"; do
    if [[ -x "${path}" ]]; then
      mtime="$(stat -c %Y "${path}")"
      if [[ "${mtime}" -ge "${newest_mtime}" ]]; then
        newest="${path}"
        newest_mtime="${mtime}"
      fi
    fi
  done
  if [[ -z "${newest}" ]]; then
    echo "sere not found. Build with cmake --preset linux-clang-relwithdebinfo" >&2
    exit 1
  fi
  echo "${newest}"
}

llvm_ok() {
  local root="$1"
  [[ -n "${root}" && -x "${root}/bin/clang" && -x "${root}/bin/ld.lld" ]]
}

find_llvm() {
  local candidates=(
    "${SERE_LLVM_DIR:-}"
    "${XDG_DATA_HOME:-${HOME}/.local/share}/sere/toolchains/llvm-${LLVM_VERSION}"
    "${HOME}/.local/share/sere/toolchains/llvm-${LLVM_VERSION}"
    "${HOME}/.local/sere/toolchains/llvm-${LLVM_VERSION}"
  )
  local root
  for root in "${candidates[@]}"; do
    if llvm_ok "${root}"; then
      echo "${root}"
      return 0
    fi
  done
  echo "LLVM ${LLVM_VERSION} not found. Run ./scripts/bootstrap-llvm.sh and set SERE_LLVM_DIR." >&2
  exit 1
}

# Ship clang + lld + their shared libs + clang resource dir. Do not copy the
# full LLVM SDK (static libs, unused tools) — that made a ~12G tree / ~3G tarball
# that cannot go through typical artifact uploads.
copy_llvm_compiler_bundle() {
  local src="$1"
  local dest="$2"
  mkdir -p "${dest}/bin" "${dest}/lib"
  local tool
  for tool in clang clang++ clang-22 clang-22.1 clang-22.1.8 \
              ld.lld lld lld-link llvm-ar llvm-ranlib llvm-nm \
              llvm-objcopy llvm-strip llvm-config; do
    if [[ -e "${src}/bin/${tool}" ]]; then
      cp -a "${src}/bin/${tool}" "${dest}/bin/${tool}"
    fi
  done
  if [[ -d "${src}/lib/clang" ]]; then
    cp -a "${src}/lib/clang" "${dest}/lib/clang"
  fi
  local bin
  for bin in "${dest}/bin/clang" "${dest}/bin/ld.lld" "${dest}/bin/lld"; do
    if [[ ! -x "${bin}" ]]; then
      continue
    fi
    local line lib
    while IFS= read -r line; do
      lib="$(sed -n 's/.*=> \(.*\) (0x.*/\1/p' <<<"${line}")"
      if [[ -z "${lib}" || "${lib}" == linux-vdso.so* ]]; then
        continue
      fi
      if [[ "${lib}" == /lib/* || "${lib}" == /usr/lib/* || "${lib}" == /lib64/* ]]; then
        continue
      fi
      if [[ -f "${lib}" ]]; then
        mkdir -p "${dest}/lib"
        cp -a "${lib}" "${dest}/lib/"
      fi
    done < <(ldd "${bin}" 2>/dev/null || true)
  done
  if [[ -d "${src}/lib" ]]; then
    local so
    shopt -s nullglob
    for so in "${src}/lib"/libLLVM*.so* "${src}/lib"/libclang*.so* \
              "${src}/lib"/libLTO.so* "${src}/lib"/LLVMgold.so; do
      cp -a "${so}" "${dest}/lib/"
    done
    shopt -u nullglob
  fi
}

EXE="$(find_sere)"
COMPILER_DIR="$(cd "$(dirname "${EXE}")" && pwd)"
LLVM_SRC="$(find_llvm)"
echo "staging ${NAME} into ${DEST}"
echo "bundling LLVM from ${LLVM_SRC}"

KEEP="$(mktemp -d)"
cp -f "${ROOT}/packaging/install.sh" "${KEEP}/install.sh"
cp -f "${ROOT}/packaging/uninstall.sh" "${KEEP}/uninstall.sh"
cp -f "${ROOT}/packaging/README-linux.md" "${KEEP}/README.md"

rm -rf "${DEST}"
mkdir -p "${DEST}/bin" "${DEST}/include/sere/api" "${DEST}/examples" "${DEST}/docs" \
  "${DEST}/packaging" "${DEST}/editors" "${DEST}/toolchains"

cp -f "${EXE}" "${DEST}/bin/sere"
chmod +x "${DEST}/bin/sere"

RUNTIME=""
for cand in "${COMPILER_DIR}/libsere_rt.a" "${COMPILER_DIR}/sere_rt.a" \
            "${ROOT}/bin/libsere_rt.a" "${ROOT}/build/linux-clang-relwithdebinfo/bin/libsere_rt.a" \
            "${ROOT}/build/linux-clang-relwithdebinfo/runtime/libsere_rt.a"; do
  if [[ -f "${cand}" ]]; then
    RUNTIME="${cand}"
    break
  fi
done
if [[ -z "${RUNTIME}" ]]; then
  echo "libsere_rt.a not found" >&2
  exit 1
fi
cp -f "${RUNTIME}" "${DEST}/bin/libsere_rt.a"
cp -f "${RUNTIME}" "${DEST}/bin/sere_rt.a"

cp -f "${ROOT}/scripts/sere-path.sh" "${DEST}/bin/sere-path.sh"
chmod +x "${DEST}/bin/sere-path.sh"
cp -f "${ROOT}/scripts/bootstrap-llvm.sh" "${DEST}/packaging/bootstrap-llvm.sh"
chmod +x "${DEST}/packaging/bootstrap-llvm.sh"
cp -f "${KEEP}/install.sh" "${DEST}/install.sh"
cp -f "${KEEP}/uninstall.sh" "${DEST}/uninstall.sh"
cp -f "${KEEP}/README.md" "${DEST}/README.md"
chmod +x "${DEST}/install.sh" "${DEST}/uninstall.sh"
rm -rf "${KEEP}"

echo "bundling compiler LLVM (clang, lld, resource dir, LLVM .so — not the full SDK)"
copy_llvm_compiler_bundle "${LLVM_SRC}" "${DEST}/toolchains/llvm-${LLVM_VERSION}"
if ! llvm_ok "${DEST}/toolchains/llvm-${LLVM_VERSION}"; then
  echo "bundled LLVM is missing clang or ld.lld" >&2
  exit 1
fi

cp -a "${ROOT}/stdlib/." "${DEST}/stdlib/"
rm -f "${DEST}/stdlib/windows.sere"
if [[ -d "${ROOT}/include/sere/api" ]]; then
  cp -a "${ROOT}/include/sere/api/." "${DEST}/include/sere/api/"
fi
cp -f "${ROOT}/LICENSE" "${DEST}/LICENSE"
if [[ -d "${ROOT}/docs" ]]; then
  cp -a "${ROOT}/docs/." "${DEST}/docs/"
fi

while IFS= read -r -d '' example; do
  base="$(basename "${example}")"
  if grep -Eq '^[[:space:]]*import windows[[:space:]]*$' "${example}"; then
    continue
  fi
  cp -f "${example}" "${DEST}/examples/${base}"
done < <(find "${ROOT}/examples" -maxdepth 1 -name '*.sere' -print0)

VSIX="$(ls -1t "${ROOT}/dist"/sere-*.vsix 2>/dev/null | head -n 1 || true)"
if [[ -z "${VSIX}" ]]; then
  VSIX="$(ls -1t "${ROOT}/releases/${NAME}/windows-x64/editors"/sere*.vsix 2>/dev/null | head -n 1 || true)"
fi
if [[ -n "${VSIX}" ]]; then
  cp -f "${VSIX}" "${DEST}/editors/sere.vsix"
fi

if find "${DEST}" \( -iname '*.exe' -o -iname '*.dll' -o -iname '*.lib' \) | grep -q .; then
  echo "refusing to ship PE objects in the Linux distro:" >&2
  find "${DEST}" \( -iname '*.exe' -o -iname '*.dll' -o -iname '*.lib' \) >&2
  exit 1
fi
if [[ -f "${DEST}/stdlib/windows.sere" ]]; then
  echo "windows.sere must not be in the Linux distro" >&2
  exit 1
fi

COMMIT="$(git -C "${ROOT}" rev-parse --short HEAD 2>/dev/null || echo unknown)"
STAMP="$(date -u +"%Y-%m-%d %H:%M:%S UTC")"
{
  echo "Sere ${NAME}"
  echo "platform: linux-x64"
  echo "self-contained: yes (compiler + LLVM ${LLVM_VERSION} + stdlib + runtime)"
  echo "staged: ${STAMP}"
  echo "git: ${COMMIT}"
  echo "llvm: ${LLVM_VERSION} (bundled under toolchains/llvm-${LLVM_VERSION})"
  echo "glibc baseline: 2.35 (Ubuntu 22.04)"
  echo "compiler: ${EXE}"
  echo "llvm source: ${LLVM_SRC}"
} > "${DEST}/MANIFEST.txt"

rm -f "${ZIP}" "${TGZ}"
( cd "${DEST}" && tar --numeric-owner --owner=0 --group=0 -czf "${TGZ}" . )
if command -v zip >/dev/null 2>&1; then
  ( cd "${DEST}" && zip -r -q "${ZIP}" . )
  echo "zip     ${ZIP}"
fi
echo "tar.gz  ${TGZ}"
echo "install with:  ${DEST}/install.sh"
echo "in-place:      . ${DEST}/bin/sere-path.sh"

# A single offline command-line installer carrying the same portable payload.
INSTALLER="${RELEASE}/Sere-${NAME}-linux-x64-setup.run"
cat > "$INSTALLER" <<'HEADER'
#!/usr/bin/env bash
set -euo pipefail
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
line="$(awk '/^__SERE_PAYLOAD__$/ {print NR + 1; exit}' "$0")"
tail -n +"$line" "$0" | tar -xz -C "$work"
bash "$work/install.sh" "$@"
exit 0
__SERE_PAYLOAD__
HEADER
cat "$TGZ" >> "$INSTALLER"
chmod +x "$INSTALLER"
( cd "$RELEASE" && sha256sum "$(basename "$TGZ")" "$(basename "$INSTALLER")" > SHA256SUMS-linux.txt )
echo "installer $INSTALLER"
