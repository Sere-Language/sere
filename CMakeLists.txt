# Sere compiler project.
# Configures the C/C++ toolchain, LLVM integration, runtime, and tests.

cmake_minimum_required(VERSION 3.28)

project(sere
  VERSION 0.1.4
  DESCRIPTION "Sere compiled language toolchain (Python-superset frontend, LLVM backend)"
  LANGUAGES C CXX
)

set(SERE_VERSION_STRING "pre-0.1.5")

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

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
