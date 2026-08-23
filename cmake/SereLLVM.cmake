# SereLLVM.cmake
# Locates the pinned LLVM development package and applies matching compile flags.

set(SERE_LLVM_MIN_VERSION "22.0.0")

if(DEFINED ENV{SERE_LLVM_DIR} AND EXISTS "$ENV{SERE_LLVM_DIR}/lib/cmake/llvm")
  set(LLVM_DIR "$ENV{SERE_LLVM_DIR}/lib/cmake/llvm" CACHE PATH "LLVM CMake package directory")
endif()

find_package(LLVM REQUIRED CONFIG)

if(LLVM_PACKAGE_VERSION VERSION_LESS "${SERE_LLVM_MIN_VERSION}")
  message(FATAL_ERROR "Sere requires LLVM ${SERE_LLVM_MIN_VERSION} or newer, found ${LLVM_PACKAGE_VERSION}")
endif()

message(STATUS "Sere LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Sere LLVM_DIR ${LLVM_DIR}")
message(STATUS "Sere LLVM tools ${LLVM_TOOLS_BINARY_DIR}")

# Official Windows LLVM binaries record an absolute DIA SDK path from the
# packager's Visual Studio install. Remap it onto this machine's DIA SDK.
if(WIN32 AND TARGET LLVMDebugInfoPDB)
  set(_sere_dia_lib "")
  if(DEFINED ENV{VSINSTALLDIR})
    set(_sere_dia_candidate "$ENV{VSINSTALLDIR}DIA SDK/lib/amd64/diaguids.lib")
    if(EXISTS "${_sere_dia_candidate}")
      set(_sere_dia_lib "${_sere_dia_candidate}")
    endif()
  endif()
  if(_sere_dia_lib STREQUAL "")
    file(GLOB _sere_dia_glob
      "C:/Program Files (x86)/Microsoft Visual Studio/*/BuildTools/DIA SDK/lib/amd64/diaguids.lib"
      "C:/Program Files/Microsoft Visual Studio/*/Community/DIA SDK/lib/amd64/diaguids.lib"
      "C:/Program Files/Microsoft Visual Studio/*/Professional/DIA SDK/lib/amd64/diaguids.lib"
      "C:/Program Files/Microsoft Visual Studio/*/Enterprise/DIA SDK/lib/amd64/diaguids.lib"
    )
    if(_sere_dia_glob)
      list(GET _sere_dia_glob 0 _sere_dia_lib)
    endif()
  endif()
  if(_sere_dia_lib STREQUAL "")
    message(FATAL_ERROR "DIA SDK diaguids.lib not found. Install the C++ workload in Visual Studio Build Tools.")
  endif()
  get_target_property(_sere_pdb_libs LLVMDebugInfoPDB INTERFACE_LINK_LIBRARIES)
  set(_sere_pdb_remapped "")
  foreach(_sere_lib IN LISTS _sere_pdb_libs)
    if(_sere_lib MATCHES "diaguids\\.lib$")
      list(APPEND _sere_pdb_remapped "${_sere_dia_lib}")
    else()
      list(APPEND _sere_pdb_remapped "${_sere_lib}")
    endif()
  endforeach()
  set_property(TARGET LLVMDebugInfoPDB PROPERTY INTERFACE_LINK_LIBRARIES "${_sere_pdb_remapped}")
  message(STATUS "Sere remapped DIA SDK to ${_sere_dia_lib}")
endif()

separate_arguments(SERE_LLVM_DEFINITIONS NATIVE_COMMAND "${LLVM_DEFINITIONS}")

# Follow the CRT that this LLVM package was built with (/MT on the official
# Windows clang+llvm archive). Do not override CMAKE_MSVC_RUNTIME_LIBRARY after
# find_package(LLVM); LLVMConfig.cmake already sets it.

function(sere_configure_llvm_target target_name)
  target_include_directories("${target_name}" SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
  target_compile_definitions("${target_name}" PUBLIC ${SERE_LLVM_DEFINITIONS})

  if(NOT LLVM_ENABLE_RTTI)
    if(MSVC)
      target_compile_options("${target_name}" PRIVATE /GR-)
    else()
      target_compile_options("${target_name}" PRIVATE -fno-rtti)
    endif()
  endif()

  if(NOT LLVM_ENABLE_EH)
    if(MSVC)
      target_compile_options("${target_name}" PRIVATE /EHs-c-)
    else()
      target_compile_options("${target_name}" PRIVATE -fno-exceptions)
    endif()
  endif()

  llvm_map_components_to_libnames(sere_llvm_libs
    Core
    Support
    Analysis
    TransformUtils
    InstCombine
    ScalarOpts
    IPO
    Vectorize
    Passes
    BitWriter
    IRReader
    MC
    MCParser
    Target
    TargetParser
    native
    nativecodegen
  )
  target_link_libraries("${target_name}" PUBLIC ${sere_llvm_libs})
endfunction()
