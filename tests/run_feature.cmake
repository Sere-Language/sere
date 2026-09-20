# Compiler intermediates use the source basename; isolate each backend's temp files.
file(MAKE_DIRECTORY "${OUTPUT}.tmp")
set(ENV{TMP} "${OUTPUT}.tmp")
set(ENV{TEMP} "${OUTPUT}.tmp")
set(ENV{TMPDIR} "${OUTPUT}.tmp")
set(ENV{SERE_STDLIB} "${STDLIB}")
execute_process(COMMAND "${COMPILER}" "--backend=${BACKEND}" "${SOURCE}" -o "${OUTPUT}"
  RESULT_VARIABLE compiled OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_err TIMEOUT 60)
if(NOT compiled STREQUAL "0")
  message(FATAL_ERROR "Compilation failed (${compiled}):\n${compile_out}\n${compile_err}")
endif()
execute_process(COMMAND "${OUTPUT}"
  RESULT_VARIABLE executed OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err TIMEOUT 20)
if(NOT executed STREQUAL "0")
  message(FATAL_ERROR "Execution failed (${executed}):\n${run_out}\n${run_err}")
endif()
message(STATUS "${run_out}")
