execute_process(
  COMMAND "${SERE}" "${SOURCE}" -o "${OUTPUT}"
  RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_err
  TIMEOUT 90)
if(NOT compile_result STREQUAL "0")
  message(FATAL_ERROR "Nested scopes failed to compile: ${compile_out}\n${compile_err}")
endif()
execute_process(
  COMMAND "${OUTPUT}"
  RESULT_VARIABLE run_result OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err
  TIMEOUT 20)
string(REPLACE "\r\n" "\n" run_out "${run_out}")
set(expected "child\n14\n10\n34\n8\n")
if(NOT run_result STREQUAL "0" OR NOT run_out STREQUAL expected)
  message(FATAL_ERROR "Nested scopes failed (${run_result}):\n${run_out}\n${run_err}\nExpected:\n${expected}")
endif()

set(fixture_dir "${OUTPUT}-fixtures")
file(MAKE_DIRECTORY "${fixture_dir}")
file(WRITE "${fixture_dir}/private.sere" "class Outer:\n    @private value: i32\n    class Child:\n        pass\ndef main() -> i32:\n    outer = Outer()\n    return outer.value\n")
file(WRITE "${fixture_dir}/local.sere" "class Outer:\n    def run(self) -> i32:\n        def child() -> i32:\n            child_only = 1\n            return child_only\n        return child_only\n")
file(WRITE "${fixture_dir}/class.sere" "class Outer:\n    class Child:\n        pass\ndef main() -> i32:\n    value = Child()\n    return 0\n")
foreach(case IN ITEMS private local class)
  execute_process(COMMAND "${SERE}" --emit-llvm "${fixture_dir}/${case}.sere"
    -o "${fixture_dir}/${case}.ll"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 30)
  if(result STREQUAL "0")
    message(FATAL_ERROR "Nested scope isolation failed: ${case}")
  endif()
  if(case STREQUAL "private")
    set(expected_error "private")
  elseif(case STREQUAL "local")
    set(expected_error "unknown name 'child_only'")
  else()
    set(expected_error "Child")
  endif()
  if(NOT err MATCHES "${expected_error}")
    message(FATAL_ERROR "Unexpected ${case} diagnostic: ${out} ${err}")
  endif()
endforeach()
