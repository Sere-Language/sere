foreach(opt IN ITEMS O0 O2)
  execute_process(COMMAND "${SERE}" "${SOURCE}" --opt ${opt} -o "${OUTPUT}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 90)
  if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Iterator compilation failed (${opt}): ${out}\n${err}")
  endif()
  execute_process(COMMAND "${OUTPUT}" RESULT_VARIABLE result
    OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 20)
  if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Iterator execution failed (${opt}): ${result} ${out}\n${err}")
  endif()
endforeach()
