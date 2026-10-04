# Sere compiler project.
# Configures the C/C++ toolchain, LLVM integration, runtime, and tests.

cmake_minimum_required(VERSION 3.28)
project(sere
  VERSION 0.2.1
  DESCRIPTION "Sere compiled language toolchain (Python-superset frontend, LLVM backend)"
  LANGUAGES C CXX
)

set(SERE_VERSION_PRE_RELEASE "pre")
set(SERE_VERSION_BUILD "" CACHE STRING "Optional build tag reported by sys.version_info.build")
set(SERE_VERSION_STRING "${SERE_VERSION_PRE_RELEASE}-${PROJECT_VERSION}")

# Optimization level the runtime library is built with, reported by
# `sys.optimization`. Release presets set CMAKE_C_FLAGS_RELEASE etc., so the
# first configured flag of the active build type is the closest single answer.
set(SERE_RUNTIME_OPT_LEVEL "unknown")
foreach(_sere_opt_config Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${_sere_opt_config}" _sere_opt_upper)
  if(CMAKE_BUILD_TYPE STREQUAL _sere_opt_config OR
     (NOT CMAKE_BUILD_TYPE AND CMAKE_CONFIGURATION_TYPES))
    string(REGEX MATCH "[/-]O[0-9sz]+" _sere_opt_match "${CMAKE_C_FLAGS_${_sere_opt_upper}}")
    if(_sere_opt_match)
      # Report the portable spelling (`O2`) rather than the driver's flag form.
      string(REGEX REPLACE "^[/-]" "" SERE_RUNTIME_OPT_LEVEL "${_sere_opt_match}")
    endif()
  endif()
endforeach()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SereWarnings.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SereLLVM.cmake")

# MSVC 14.5x's <xutility> enables vectorized algorithms that lower to
# `__std_replace_copy_*` helpers clang-cl cannot emit, so linking fails with an
# undefined symbol. The STL honours this override, so pin it off for clang-cl.
if(MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_definitions(_USE_STD_VECTOR_ALGORITHMS=0)
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/sere/Version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/sere/Version.h"
  @ONLY
)

string(REPLACE "\\" "/" SERE_LLVM_TOOLS_DIR "${LLVM_TOOLS_BINARY_DIR}")
string(REPLACE "\\" "/" SERE_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
set(SERE_PINNED_LLVM_VERSION "22.1.8")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/sere/ToolchainPaths.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/sere/ToolchainPaths.h"
  @ONLY
)

add_library(sere_headers INTERFACE)
target_include_directories(sere_headers INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

add_subdirectory(runtime)
add_subdirectory(lib)
add_subdirectory(tools/sere)

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  add_subdirectory(tests)
endif()

if(WIN32)
  add_custom_target(sere_release
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
      "${CMAKE_SOURCE_DIR}/releases/stage.ps1" -SkipBuild
      -BuildDir "${CMAKE_BINARY_DIR}" -LlvmRoot "${LLVM_TOOLS_BINARY_DIR}/.."
    DEPENDS sere
    USES_TERMINAL
    COMMENT "Package portable Sere and per-user installer into releases/${PROJECT_VERSION}"
    VERBATIM
  )
endif()
