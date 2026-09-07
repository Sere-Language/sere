execute_process(
  COMMAND "${SERE}" "${SOURCE}" -o "${OUTPUT}"
  RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_err
  TIMEOUT 90)
if(NOT compile_result STREQUAL "0")
  message(FATAL_ERROR "Generic tuple failed to compile: ${compile_out}\n${compile_err}")
endif()
execute_process(
  COMMAND "${OUTPUT}"
  RESULT_VARIABLE run_result OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err
  TIMEOUT 20)
string(REPLACE "\r\n" "\n" run_out "${run_out}")
set(expected "(42, hello)\n(world, 99)\n[42, 'hello']\n['world', 99]\n{'left': 42, 'right': 'hello'}\n(7, updated)\n7\n'updated'\n42\nNone\nbad index\nbad value\n")
if(NOT run_result STREQUAL "0" OR NOT run_out STREQUAL expected)
  message(FATAL_ERROR "Generic tuple failed (${run_result}):\n${run_out}\n${run_err}\nExpected:\n${expected}")
endif()
