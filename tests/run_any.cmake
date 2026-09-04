execute_process(
  COMMAND "${SERE}" "${SOURCE}" -o "${OUTPUT}"
  RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_err
  TIMEOUT 90)
if(NOT compile_result STREQUAL "0")
  message(FATAL_ERROR "Any regression failed to compile: ${compile_out}\n${compile_err}")
endif()
execute_process(
  COMMAND "${OUTPUT}"
  RESULT_VARIABLE run_result OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err
  TIMEOUT 20)
if(EXPECT_MISMATCH)
  if(run_result STREQUAL "0" OR NOT "${run_out}${run_err}" MATCHES "Any type mismatch: expected i32")
    message(FATAL_ERROR "Expected checked Any failure; got ${run_result}: ${run_out}\n${run_err}")
  endif()
elseif(NOT run_result STREQUAL "0")
  message(FATAL_ERROR "Any regression failed at runtime (${run_result}): ${run_out}\n${run_err}")
endif()
