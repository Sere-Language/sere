# sere compiler driver.

file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/icon.ico" SERE_ICON_ICO)
configure_file(sere.rc.in "${CMAKE_CURRENT_BINARY_DIR}/sere.rc" @ONLY)

add_executable(sere
  main.cpp
)
if(WIN32)
  target_sources(sere PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/sere.rc")
endif()
target_link_libraries(sere PRIVATE sere_driver)
# The staged front-end libraries come from these targets, so build them too.
add_dependencies(sere sere_front)
sere_apply_warnings(sere)

if(WIN32)
  set(SERE_ICON_RES "${CMAKE_BINARY_DIR}/bin/sere_icon.res")
  if(NOT CMAKE_RC_COMPILER)
    find_program(CMAKE_RC_COMPILER NAMES llvm-rc llvm-rc.exe rc rc.exe
      HINTS "${LLVM_TOOLS_BINARY_DIR}")
  endif()
  get_filename_component(_sere_rc_name "${CMAKE_RC_COMPILER}" NAME)
  if(_sere_rc_name MATCHES "llvm-rc")
    set(_sere_rc_fo "/FO")
    set(_sere_rc_nologo "")
  else()
    set(_sere_rc_fo "/fo")
    set(_sere_rc_nologo "/nologo")
  endif()
  add_custom_command(
    OUTPUT "${SERE_ICON_RES}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/bin"
    COMMAND "${CMAKE_RC_COMPILER}" ${_sere_rc_nologo} ${_sere_rc_fo} "${SERE_ICON_RES}"
            "${CMAKE_CURRENT_BINARY_DIR}/sere.rc"
    DEPENDS
      "${CMAKE_CURRENT_BINARY_DIR}/sere.rc"
      "${CMAKE_SOURCE_DIR}/icon.ico"
    COMMENT "Compile default Sere program icon"
    VERBATIM
  )
  add_custom_target(sere_icon_res ALL DEPENDS "${SERE_ICON_RES}")
  add_dependencies(sere sere_icon_res)
endif()

set_target_properties(sere PROPERTIES
  OUTPUT_NAME sere
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/staging"
)
if(UNIX AND NOT APPLE)
  set_target_properties(sere PROPERTIES
    BUILD_RPATH "$ORIGIN;$ORIGIN/../lib;$ENV{SERE_LLVM_DIR}/lib"
    INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib;$ORIGIN/../toolchains/llvm-22.1.8/lib"
    BUILD_WITH_INSTALL_RPATH OFF
  )
endif()

set(SERE_PROJECT_BIN "${CMAKE_SOURCE_DIR}/bin")
# Staging runs on every build rather than as a POST_BUILD of `sere`: the staged
# front-end libraries change without the executable relinking, and a program that
# imports `ast` links whatever was staged last.
add_custom_target(sere_stage ALL
  COMMAND "${CMAKE_COMMAND}"
    -DSERE_EXE="$<TARGET_FILE:sere>"
    -DSERE_RUNTIME="$<TARGET_FILE:sere_runtime>"
    -DSERE_FRONT="$<TARGET_FILE:sere_front>"
    -DSERE_PARSE="$<TARGET_FILE:sere_parse>"
    -DSERE_AST="$<TARGET_FILE:sere_ast>"
    -DSERE_TYPES="$<TARGET_FILE:sere_types>"
    -DSERE_LEX="$<TARGET_FILE:sere_lex>"
    -DSERE_SOURCE="$<TARGET_FILE:sere_source>"
    -DSERE_DIAG="$<TARGET_FILE:sere_diag>"
    -DSERE_TARGET_OS="${CMAKE_SYSTEM_NAME}"
    -DSERE_STDLIB="${CMAKE_SOURCE_DIR}/stdlib"
    -DSERE_API_HEADER="${CMAKE_SOURCE_DIR}/include/sere/api/sere_mod.h"
    -DSERE_GC_HEADER="${CMAKE_SOURCE_DIR}/include/sere/api/sere_gc.h"
    -DSERE_BUILD_BIN="${CMAKE_BINARY_DIR}/bin"
    -DSERE_PROJECT_BIN="${SERE_PROJECT_BIN}"
    -DSERE_SUFFIX="$<TARGET_FILE_SUFFIX:sere>"
    -DSERE_ICON_ICO="${CMAKE_SOURCE_DIR}/icon.ico"
    -DSERE_ICON_RES="${CMAKE_BINARY_DIR}/bin/sere_icon.res"
    -P "${CMAKE_SOURCE_DIR}/scripts/copy_compiler_outputs.cmake"
  DEPENDS sere sere_runtime
  COMMENT "Staging sere, its runtime, stdlib, C API headers, and front libraries"
)
