# sere compiler driver.

add_executable(sere
  main.cpp
)
target_link_libraries(sere PRIVATE sere_driver)
add_dependencies(sere sere_runtime sere_qt6)
sere_apply_warnings(sere)

set_target_properties(sere PROPERTIES
  OUTPUT_NAME sere
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/staging"
)

set(SERE_PROJECT_BIN "${CMAKE_SOURCE_DIR}/bin")
add_custom_command(TARGET sere POST_BUILD
  COMMAND "${CMAKE_COMMAND}"
    -DSERE_EXE="$<TARGET_FILE:sere>"
    -DSERE_RUNTIME="$<TARGET_FILE:sere_runtime>"
    -DSERE_QT6="$<TARGET_LINKER_FILE:sere_qt6>"
    -DSERE_QT6_BIN="$<TARGET_FILE:sere_qt6>"
    -DSERE_STDLIB="${CMAKE_SOURCE_DIR}/stdlib"
    -DSERE_API_HEADER="${CMAKE_SOURCE_DIR}/include/sere/api/sere_mod.h"
    -DSERE_GC_HEADER="${CMAKE_SOURCE_DIR}/include/sere/api/sere_gc.h"
    -DSERE_BUILD_BIN="${CMAKE_BINARY_DIR}/bin"
    -DSERE_PROJECT_BIN="${SERE_PROJECT_BIN}"
    -DSERE_SUFFIX="$<TARGET_FILE_SUFFIX:sere>"
    -P "${CMAKE_SOURCE_DIR}/scripts/copy_compiler_outputs.cmake"
  COMMENT "Copy runtime, stdlib, C API, and sere into build/bin and project bin/"
)
