execute_process(
  COMMAND "${SERE}" "${SOURCE}" -o "${OUTPUT}"
  RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_err
  TIMEOUT 90)
if(NOT compile_result STREQUAL "0")
  message(FATAL_ERROR "Imported routes failed to compile: ${compile_out}\n${compile_err}")
endif()
execute_process(
  COMMAND "${OUTPUT}"
  RESULT_VARIABLE run_result OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err
  TIMEOUT 20)
string(REPLACE "\r\n" "\n" run_out "${run_out}")
set(expected "Hello, world!\nposted\ndefault\n3\n")
if(NOT run_result STREQUAL "0" OR NOT run_out STREQUAL expected)
  message(FATAL_ERROR "Imported routes failed (${run_result}):\n${run_out}\n${run_err}\nExpected:\n${expected}")
endif()

# Invalid imported keywords must use the same diagnostics as local method calls.
get_filename_component(source_dir "${SOURCE}" DIRECTORY)
set(fixture_dir "${OUTPUT}-fixtures")
file(MAKE_DIRECTORY "${fixture_dir}")
configure_file("${source_dir}/router.sere" "${fixture_dir}/router.sere" COPYONLY)
file(READ "${SOURCE}" original)
foreach(case IN ITEMS unknown wrong_type)
  if(case STREQUAL "unknown")
    set(replacement "unknown=router.Method.GET")
    set(diagnostic "unexpected keyword argument 'unknown'")
  else()
    set(replacement [[methods="GET"]])
    set(diagnostic "parameter 'methods' type mismatch")
  endif()
  string(REPLACE "methods=router.Method.GET" "${replacement}" invalid "${original}")
  file(WRITE "${fixture_dir}/${case}.sere" "${invalid}")
  execute_process(COMMAND "${SERE}" --emit-llvm "${fixture_dir}/${case}.sere"
    -o "${fixture_dir}/${case}.ll"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 30)
  if(result STREQUAL "0" OR NOT err MATCHES "${diagnostic}")
    message(FATAL_ERROR "Imported keyword ${case} diagnostic missing: ${out} ${err}")
  endif()
endforeach()
