#!/usr/bin/env bash
# Build the Linux x86_64 distro from this checkout and stage it into
# releases/<version>/ (0.2.1 with the current project version).
#
# Run this on a native Linux x86_64 host or inside a container. It builds in
# place, so the checkout must be writable. This is the Linux-native counterpart
# of releases/stage-linux.ps1; on Windows use that script instead, since it
# drives WSL2 or Docker for you. releases/linux-build.sh remains the driver the
# PowerShell script runs on a synced copy of the sources.
#
#   ./releases/stage-linux.sh
#   ./releases/stage-linux.sh --test
#   ./releases/stage-linux.sh --name 0.2.1 --skip-build
#
# Options:
#   --name VERSION    release version (default: VERSION from CMakeLists.txt)
#   --preset PRESET   configure/build preset (default: linux-clang-relwithdebinfo)
#   --jobs N          parallel build jobs (default: the build system's choice)
#   --skip-build      reuse the existing build in build/<preset>
#   --skip-deps       do not install missing build dependencies
#   --test            run ctest before staging
#   -h, --help        show this help
#
# The same values are accepted as environment variables (SERE_RELEASE_NAME,
# SERE_PRESET, SERE_JOBS, SERE_SKIP_BUILD=1, SERE_SKIP_DEPS=1, SERE_TEST=1).
#
# The payload is assembled by releases/stage.sh, which bundles the compiler,
# runtime, stdlib, and a trimmed LLVM toolchain, and writes the portable
# tarball, optional ZIP, offline .run installer, and SHA256SUMS-linux.txt.
set -euo pipefail

usage() {
  sed -n '2,/^set -euo pipefail$/p' "$0" | sed '$d' | sed -e 's/^# \{0,1\}//'
}

SELF="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "${SELF}/.." && pwd)"

ARG_NAME=""
ARG_PRESET=""
SKIP_BUILD="${SERE_SKIP_BUILD:-0}"
SKIP_DEPS="${SERE_SKIP_DEPS:-0}"
RUN_TESTS="${SERE_TEST:-0}"
JOBS="${SERE_JOBS:-}"
LLVM_VERSION="22.1.8"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --name) ARG_NAME="${2:-}"; shift 2 ;;
    --preset) ARG_PRESET="${2:-}"; shift 2 ;;
    --jobs) JOBS="${2:-}"; shift 2 ;;
    --skip-build) SKIP_BUILD=1; shift ;;
    --skip-deps) SKIP_DEPS=1; shift ;;
    --test) RUN_TESTS=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
done

if [[ "$(uname -s)" != Linux ]]; then
  echo "stage-linux.sh must run inside Linux (found $(uname -s))." >&2
  echo "On Windows or macOS use releases/stage-linux.ps1, which drives WSL2 or Docker." >&2
  exit 1
fi
if [[ "$(uname -m)" != x86_64 ]]; then
  echo "stage-linux.sh targets Linux x86_64 (found $(uname -m))." >&2
  exit 1
fi

# The checker that verifies the bundled LLVM toolchain can actually run; it is
# shared with releases/linux-build.sh.
# shellcheck source=releases/lib-llvm-check.sh
. "${SELF}/lib-llvm-check.sh"

# SERE_RELEASE_NAME, not NAME: shells and WSL commonly export NAME (the Windows
# host name), which would otherwise be mistaken for a release version.
NAME="${ARG_NAME:-${SERE_RELEASE_NAME:-}}"
if [[ -z "${NAME}" ]]; then
  NAME="$(sed -nE 's/^[[:space:]]*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*$/\1/p' "${REPO}/CMakeLists.txt" | head -n 1)"
fi
# Both bare and pre- versions land in the same releases/<version>/ folder.
NAME="${NAME#pre-}"
if [[ ! "${NAME}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Invalid release name: ${NAME}" >&2
  echo "Pass --name x.y.z, or clear SERE_RELEASE_NAME to use the CMakeLists.txt version." >&2
  exit 2
fi
PRESET="${ARG_PRESET:-${SERE_PRESET:-linux-clang-relwithdebinfo}}"
RELEASE="${REPO}/releases/${NAME}"

run_root() {
  if [[ "${EUID}" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    echo "Root is required for: $*" >&2
    echo "Install the missing packages manually, then pass --skip-deps." >&2
    exit 1
  fi
}

echo "== release ${NAME} =="
echo "   repository: ${REPO}"
echo "   preset:     ${PRESET}"

# ---------------------------------------------------------------------------
# Build dependencies. Only touched when something is actually missing, and only
# through apt-get, so a prepared host is never modified. Whether the LLVM
# toolchain can actually run is a separate, runtime-based check
# (sere_check_llvm_runtime), so no shared-library names appear here.
# ---------------------------------------------------------------------------
if [[ "${SKIP_DEPS}" != "1" ]]; then
  missing=""
  for tool in cmake ninja curl xz zip sha256sum; do
    command -v "${tool}" >/dev/null 2>&1 || missing="${missing} ${tool}"
  done

  if [[ -n "${missing}" ]]; then
    if ! command -v apt-get >/dev/null 2>&1; then
      echo "Missing build dependencies:${missing}." >&2
      echo "Install them and retry (or pass --skip-deps to skip this check)." >&2
      exit 1
    fi
    export DEBIAN_FRONTEND=noninteractive
    # A stale mirror must not stop an otherwise satisfiable build.
    if ! run_root apt-get update -qq; then
      echo "warning: apt-get update failed; continuing" >&2
    fi
    echo "== installing build dependencies:${missing} =="
    run_root apt-get install -y -qq cmake ninja-build curl xz-utils zip file
  fi
fi

# ---------------------------------------------------------------------------
# LLVM 22.1.8. bootstrap-llvm.sh exports into its own process, so the value is
# resolved here and passed to cmake through the preset's environment.
# ---------------------------------------------------------------------------
toolchain_root="${SERE_TOOLCHAIN_ROOT:-${XDG_DATA_HOME:-${HOME}/.local/share}/sere/toolchains}"
default_llvm="${toolchain_root}/llvm-${LLVM_VERSION}"
LLVM_DIR="${SERE_LLVM_DIR:-${default_llvm}}"
llvm_ok() { [[ -x "$1/bin/clang" && -x "$1/bin/ld.lld" ]]; }

if ! llvm_ok "${LLVM_DIR}" && ! llvm_ok "${default_llvm}"; then
  echo "== LLVM ${LLVM_VERSION} =="
  bash "${REPO}/scripts/bootstrap-llvm.sh"
fi
# Prefer an explicit but stale SERE_LLVM_DIR only when it is actually usable.
llvm_ok "${LLVM_DIR}" || LLVM_DIR="${default_llvm}"
if ! llvm_ok "${LLVM_DIR}"; then
  echo "LLVM ${LLVM_VERSION} not found at ${LLVM_DIR}." >&2
  echo "Run scripts/bootstrap-llvm.sh, then point SERE_LLVM_DIR at the toolchain." >&2
  exit 1
fi
export SERE_LLVM_DIR="${LLVM_DIR}"
export PATH="${SERE_LLVM_DIR}/bin:${PATH}"
echo "   llvm:       ${SERE_LLVM_DIR}"

# Validate the toolchain by running it; never assume a particular SONAME.
if ! sere_check_llvm_runtime "${SERE_LLVM_DIR}" "${LLVM_VERSION}"; then
  echo "Fix the LLVM toolchain above, or point SERE_LLVM_DIR at a working LLVM ${LLVM_VERSION}." >&2
  exit 1
fi

# The project is C++20, so clang++ needs the host C++ standard library.
if ! sere_cxx_stdlib_ok "${SERE_LLVM_DIR}/bin/clang++"; then
  if [[ "${SKIP_DEPS}" != "1" ]] && command -v apt-get >/dev/null 2>&1; then
    echo "== installing a C++ standard library (required by clang++) =="
    export DEBIAN_FRONTEND=noninteractive
    if ! run_root apt-get install -y -qq g++; then
      echo "warning: could not install g++" >&2
    fi
  fi
  if ! sere_cxx_stdlib_ok "${SERE_LLVM_DIR}/bin/clang++"; then
    [[ -z "${SERE_CXX_PROBE_OUTPUT}" ]] || printf '%s\n' "${SERE_CXX_PROBE_OUTPUT}" >&2
    echo "clang++ cannot compile C++20. Install a C++ standard library" >&2
    echo "(for example 'sudo apt-get install -y g++') and re-run." >&2
    exit 1
  fi
fi

# LLVMExports.cmake needs the development packages of the libraries the
# archive was built against; install them when the throwaway probe fails.
if ! sere_llvm_cmake_ok "${SERE_LLVM_DIR}" "${SERE_LLVM_DIR}/bin/clang++"; then
  if [[ "${SKIP_DEPS}" != "1" ]] && command -v apt-get >/dev/null 2>&1; then
    echo "== installing LLVM SDK development packages =="
    export DEBIAN_FRONTEND=noninteractive
    if ! run_root apt-get update -qq; then
      echo "warning: apt-get update failed; continuing" >&2
    fi
    # One package at a time: a name this distribution does not have (libtinfo-dev
    # has no candidate on Ubuntu 26.04) must not stop the others from installing.
    for pkg in libzstd-dev zlib1g-dev libxml2-dev; do
      if ! run_root apt-get install -y -qq "${pkg}"; then
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
# Configure, build, and optionally test. The preset is Linux-only, so the
# condition in CMakePresets.json also rejects a non-Linux host here.
# ---------------------------------------------------------------------------
cd "${REPO}"
if [[ "${SKIP_BUILD}" != "1" ]]; then
  echo "== configure (${PRESET}) =="
  cmake --preset "${PRESET}" ${configure_args[@]+"${configure_args[@]}"}
  echo "== build (${PRESET}) =="
  build_args=(--preset "${PRESET}")
  if [[ -n "${JOBS}" ]]; then
    build_args+=(--parallel "${JOBS}")
  fi
  cmake --build "${build_args[@]}"
  if [[ "${RUN_TESTS}" == "1" ]]; then
    echo "== test (${PRESET}) =="
    ctest --preset "${PRESET}" --output-on-failure
  fi
else
  echo "== reusing the existing build in build/${PRESET} =="
fi

# ---------------------------------------------------------------------------
# Stage. stage.sh resolves the newest compiler it can find and writes the
# payload into releases/<version>/ next to the Windows artifacts.
# ---------------------------------------------------------------------------
STAGE="${REPO}/releases/stage.sh"
[[ -f "${STAGE}" ]] || { echo "releases/stage.sh is missing from ${REPO}." >&2; exit 1; }

if [[ ! -x "${REPO}/bin/sere" && ! -x "${REPO}/build/${PRESET}/bin/sere" ]]; then
  echo "No Linux compiler found in bin/sere or build/${PRESET}/bin/sere." >&2
  echo "Build it first, or drop --skip-build so this script can build it." >&2
  exit 1
fi

echo "== staging the Linux distro into ${RELEASE} =="
chmod +x "${STAGE}"
bash "${STAGE}" "${NAME}"

echo
echo "Linux release ${NAME} is ready in ${RELEASE}"
for artifact in \
  "${RELEASE}/Sere-${NAME}-linux-x64-portable.tar.gz" \
  "${RELEASE}/Sere-${NAME}-linux-x64-portable.zip" \
  "${RELEASE}/Sere-${NAME}-linux-x64-setup.run" \
  "${RELEASE}/SHA256SUMS-linux.txt"; do
  if [[ -e "${artifact}" ]]; then
    echo "   $(basename "${artifact}")"
  fi
done
