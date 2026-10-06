#!/usr/bin/env bash
# Build the Linux x86_64 distro and stage it into releases/<name>/.
#
# This runs inside a Linux environment (native host, WSL2, or a container),
# because the distro needs a Linux compiler: the linux-clang-relwithdebinfo
# preset is Linux-only and scripts/bootstrap-llvm.sh fetches a Linux toolchain.
#
#   SERE_RELEASE_NAME=0.2.1 REPO=/mnt/c/repo WORK=/opt/sere-release/0.2.1 \
#     ./releases/linux-build.sh
#   ./releases/linux-build.sh --name 0.2.1 --repo /mnt/c/repo --test
#
# Options:
#   --name VERSION   release version (default: VERSION from CMakeLists.txt)
#   --repo DIR       repository as seen from here (default: this script's root)
#   --work DIR       workspace for sources and build (default: $HOME/sere-release/$NAME)
#   --skip-build     reuse the existing build in the workspace
#   --test           run ctest before staging
#   --with-payload   also copy the extracted linux-x64 tree back into --repo
#   --without-editor stage without an editor extension
#
# The same options are accepted as environment variables (SERE_RELEASE_NAME,
# REPO, WORK, SKIP_BUILD=1, TEST=1, WITH_PAYLOAD=1, WITHOUT_EDITOR=1).
set -euo pipefail

SELF="$(cd "$(dirname "$0")" && pwd)"
# Runtime-based checker for the bundled LLVM toolchain (shared with
# releases/stage-linux.sh).
# shellcheck source=releases/lib-llvm-check.sh
. "${SELF}/lib-llvm-check.sh"
ARG_NAME=""
ARG_REPO=""
ARG_WORK=""
SKIP_BUILD="${SKIP_BUILD:-0}"
RUN_TESTS="${TEST:-0}"
WITH_PAYLOAD="${WITH_PAYLOAD:-0}"
WITHOUT_EDITOR="${WITHOUT_EDITOR:-0}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --name) ARG_NAME="$2"; shift 2 ;;
    --repo) ARG_REPO="$2"; shift 2 ;;
    --work) ARG_WORK="$2"; shift 2 ;;
    --skip-build) SKIP_BUILD=1; shift ;;
    --test) RUN_TESTS=1; shift ;;
    --with-payload) WITH_PAYLOAD=1; shift ;;
    --without-editor) WITHOUT_EDITOR=1; shift ;;
    -h|--help) sed -n '2,26p' "$0"; exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
done

REPO="${ARG_REPO:-${REPO:-$(cd "${SELF}/.." && pwd)}}"
# SERE_RELEASE_NAME, not NAME: shells and WSL commonly export NAME (the Windows
# host name), which would otherwise be mistaken for a release version.
NAME="${ARG_NAME:-${SERE_RELEASE_NAME:-}}"
LLVM_VERSION="22.1.8"

if [[ "$(uname -s)" != Linux ]]; then
  echo "linux-build.sh must run inside Linux (found $(uname -s))." >&2
  exit 1
fi

if [[ -z "${NAME}" ]]; then
  NAME="$(sed -nE 's/^[[:space:]]*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*$/\1/p' "${REPO}/CMakeLists.txt" | head -n 1)"
fi
[[ "${NAME}" =~ ^(pre-)?[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Invalid release name: ${NAME}" >&2; exit 2; }
NAME="${NAME#pre-}"
WORK="${ARG_WORK:-${WORK:-${HOME}/sere-release/${NAME}}}"
SRC="${WORK}/src"
LLVM_DIR="${SERE_LLVM_DIR:-${HOME}/.local/share/sere/toolchains/llvm-${LLVM_VERSION}}"
PRESET="linux-clang-relwithdebinfo"

export DEBIAN_FRONTEND=noninteractive

echo "== release ${NAME} =="
echo "   repository: ${REPO}"
echo "   workspace:  ${WORK}"

# ---------------------------------------------------------------------------
# The container path starts from a bare Ubuntu image. Whether the LLVM
# toolchain can actually run is a separate, runtime-based check
# (sere_check_llvm_runtime), so no shared-library names appear here.
# ---------------------------------------------------------------------------
missing=""
for tool in cmake ninja curl xz zip; do
  command -v "${tool}" >/dev/null 2>&1 || missing="${missing} ${tool}"
done
if [[ -n "${missing}" ]]; then
  if ! command -v apt-get >/dev/null 2>&1; then
    echo "Missing build dependencies:${missing}. Install them and retry." >&2
    exit 1
  fi
  if ! apt-get update -qq; then
    echo "warning: apt-get update failed; continuing" >&2
  fi
  echo "== installing build dependencies:${missing} =="
  apt-get install -y -qq cmake ninja-build curl xz-utils zip file >/dev/null
fi

# ---------------------------------------------------------------------------
# Sources are copied onto the Linux filesystem: building straight from a
# Windows mount would lose the executable bits the distro depends on.
# ---------------------------------------------------------------------------
echo "== syncing sources into ${SRC} =="
mkdir -p "${SRC}"
for item in CMakeLists.txt CMakePresets.json cmake lib include tools runtime stdlib tests \
  scripts packaging editors examples docs; do
  if [[ -e "${REPO}/${item}" ]]; then
    rm -rf "${SRC:?}/${item}"
    cp -a "${REPO}/${item}" "${SRC}/"
  fi
done
if [[ -f "${REPO}/LICENSE" ]]; then
  cp -f "${REPO}/LICENSE" "${SRC}/LICENSE"
fi
# A prebuilt extension is reused when the editor sources are absent.
if [[ "${WITHOUT_EDITOR}" != "1" ]]; then
  mkdir -p "${SRC}/dist"
  if [[ -d "${REPO}/dist" ]]; then
    cp -a "${REPO}/dist/." "${SRC}/dist/"
  fi
  # The versioned folder may hold the extension built by a Windows release run.
  # The version-specific match is copied last so stage.sh, which takes the newest
  # dist/sere-*.vsix, cannot pick a stale one.
  shopt -s nullglob
  for vsix in "${REPO}/releases/sere-"*.vsix "${REPO}/releases/sere-${NAME}.vsix"; do
    cp -f "${vsix}" "${SRC}/dist/$(basename "${vsix}")"
  done
  shopt -u nullglob
fi
# Only the release scripts are copied: the versioned folders hold large build
# output that is irrelevant here, and stage.sh must run from the copy so the
# payload is assembled on Linux.
mkdir -p "${SRC}/releases"
shopt -s nullglob
for script in "${REPO}/releases"/*.sh; do
  cp -f "${script}" "${SRC}/releases/"
done
shopt -u nullglob
STAGE="${SRC}/releases/stage.sh"
if [[ ! -f "${STAGE}" ]]; then
  echo "releases/stage.sh is missing from ${REPO}." >&2
  exit 1
fi

echo "== LLVM ${LLVM_VERSION} =="
bash "${REPO}/scripts/bootstrap-llvm.sh"
if [[ ! -x "${LLVM_DIR}/bin/clang" ]]; then
  echo "LLVM not found at ${LLVM_DIR} after bootstrap." >&2
  exit 1
fi
export SERE_LLVM_DIR="${LLVM_DIR}"
export PATH="${SERE_LLVM_DIR}/bin:${PATH}"

# Validate the toolchain by running it; never assume a particular SONAME.
if ! sere_check_llvm_runtime "${SERE_LLVM_DIR}" "${LLVM_VERSION}"; then
  echo "Fix the LLVM toolchain above, or point SERE_LLVM_DIR at a working LLVM ${LLVM_VERSION}." >&2
  exit 1
fi

# The project is C++20, so clang++ needs the host C++ standard library.
if ! sere_cxx_stdlib_ok "${SERE_LLVM_DIR}/bin/clang++"; then
  if [[ "${SERE_SKIP_DEPS:-0}" != "1" ]] && command -v apt-get >/dev/null 2>&1; then
    echo "== installing a C++ standard library (required by clang++) =="
    export DEBIAN_FRONTEND=noninteractive
    if ! apt-get install -y -qq g++; then
      echo "warning: could not install g++" >&2
    fi
  fi
  if ! sere_cxx_stdlib_ok "${SERE_LLVM_DIR}/bin/clang++"; then
    [[ -z "${SERE_CXX_PROBE_OUTPUT}" ]] || printf '%s\n' "${SERE_CXX_PROBE_OUTPUT}" >&2
    echo "clang++ cannot compile C++20. Install a C++ standard library" >&2
    echo "(for example 'apt-get install -y g++') and re-run." >&2
    exit 1
  fi
fi

# LLVMExports.cmake needs the development packages of the libraries the
# archive was built against; install them when the throwaway probe fails.
if ! sere_llvm_cmake_ok "${SERE_LLVM_DIR}" "${SERE_LLVM_DIR}/bin/clang++"; then
  if [[ "${SERE_SKIP_DEPS:-0}" != "1" ]] && command -v apt-get >/dev/null 2>&1; then
    echo "== installing LLVM SDK development packages =="
    export DEBIAN_FRONTEND=noninteractive
    if ! apt-get update -qq; then
      echo "warning: apt-get update failed; continuing" >&2
    fi
    # One package at a time: a name this distribution does not have (libtinfo-dev
    # has no candidate on Ubuntu 26.04) must not stop the others from installing.
    for pkg in libzstd-dev zlib1g-dev libxml2-dev; do
      if ! apt-get install -y -qq "${pkg}"; then
        echo "warning: could not install ${pkg}" >&2
      fi
    done
  fi
  if ! sere_llvm_cmake_ok "${SERE_LLVM_DIR}" "${SERE_LLVM_DIR}/bin/clang++"; then
    printf '%s\n' "${SERE_LLVM_CMAKE_PROBE_OUTPUT}" | tail -n 25 >&2
    echo "The LLVM CMake package cannot be configured. Install the development" >&2
    echo "packages its imported targets need (zstd, zlib, libxml2) and re-run." >&2
    exit 1
  fi
fi

# The archive's ld.lld links against libxml2.so.2, which some distributions no
# longer ship (Ubuntu 26.04 ships libxml2.so.16). Substitute a linker that runs
# on this host instead of failing the release over one bundled tool.
configure_args=()
linker="$(sere_select_linker "${SERE_LLVM_DIR}")" || linker=""
if [[ -z "${linker}" ]]; then
  echo "No usable linker found (tried the bundled ld.lld, ld.lld, lld, ld, mold)." >&2
  exit 1
fi
if [[ "${linker}" != "${SERE_LLVM_DIR}/bin/ld.lld" ]]; then
  echo "   linker:     ${linker} (bundled ld.lld cannot run here)"
  configure_args+=(
    "-DCMAKE_LINKER=${linker}"
    "-DCMAKE_EXE_LINKER_FLAGS_INIT=" "-DCMAKE_EXE_LINKER_FLAGS="
    "-DCMAKE_SHARED_LINKER_FLAGS_INIT=" "-DCMAKE_SHARED_LINKER_FLAGS="
    "-DCMAKE_MODULE_LINKER_FLAGS_INIT=" "-DCMAKE_MODULE_LINKER_FLAGS="
  )
fi

# ---------------------------------------------------------------------------
# Configure, build, and stage. stage.sh must run from the workspace copy so the
# payload is assembled on Linux with correct permissions.
# ---------------------------------------------------------------------------
cd "${SRC}"
if [[ "${SKIP_BUILD}" != "1" ]]; then
  echo "== configure (${PRESET}) =="
  cmake --preset "${PRESET}" ${configure_args[@]+"${configure_args[@]}"}
  echo "== build =="
  cmake --build --preset "${PRESET}"
  if [[ "${RUN_TESTS}" == "1" ]]; then
    echo "== test =="
    ctest --preset "${PRESET}" --output-on-failure
  fi
fi

echo "== staging the Linux distro =="
chmod +x "${STAGE}"
"${STAGE}" "${NAME}"

# ---------------------------------------------------------------------------
# Publish the archives back into the repository's release folder. The extracted
# payload stays in the workspace unless WITH_PAYLOAD=1.
# ---------------------------------------------------------------------------
STAGED="${SRC}/releases/${NAME}"
if [[ ! -d "${STAGED}" ]]; then
  STAGED="$(dirname "${STAGE}")/${NAME}"
fi
PUBLISH="${REPO}/releases/${NAME}"
echo "== publishing artifacts into ${PUBLISH} =="
mkdir -p "${PUBLISH}"
shopt -s nullglob
for artifact in "${STAGED}"/Sere-"${NAME}"-linux-x64-* "${STAGED}"/SHA256SUMS-linux.txt; do
  cp -f "${artifact}" "${PUBLISH}/"
  echo "   $(basename "${artifact}")"
done
shopt -u nullglob
if [[ "${WITH_PAYLOAD}" == "1" && -d "${STAGED}/linux-x64" ]]; then
  rm -rf "${PUBLISH:?}/linux-x64"
  cp -a "${STAGED}/linux-x64" "${PUBLISH}/"
  echo "   linux-x64/"
fi

echo "Linux release ${NAME} is ready in ${PUBLISH}"
