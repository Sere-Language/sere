execute_process(
  COMMAND "${SERE}" "${SOURCE}" -o "${OUTPUT}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 90)
if(NOT result STREQUAL "0")
  message(FATAL_ERROR "Exception regression failed to compile: ${output}\n${error}")
endif()
execute_process(COMMAND "${OUTPUT}" RESULT_VARIABLE result
  OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
file(READ "${EXPECTED}" expected)
string(REPLACE "\r\n" "\n" output "${output}")
string(REPLACE "\r\n" "\n" expected "${expected}")
if(NOT result STREQUAL "0" OR NOT output STREQUAL expected OR NOT error STREQUAL "")
  message(FATAL_ERROR "Exception regression failed (${result}):\n${output}\n${error}\nExpected:\n${expected}")
endif()

function(reject name source diagnostic)
  file(WRITE "${OUTPUT}.${name}.sere" "${source}")
  execute_process(COMMAND "${SERE}" --emit-llvm "${OUTPUT}.${name}.sere"
    -o "${OUTPUT}.${name}.ll" RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
  if(result STREQUAL "0" OR NOT error MATCHES "${diagnostic}")
    message(FATAL_ERROR "${name}: expected ${diagnostic}, got ${result}: ${output}\n${error}")
  endif()
endfunction()
reject(raise_integer "def main() -> i32:\n    raise 42\n    return 0\n" "raise requires an Exception")
reject(catch_integer "def main() -> i32:\n    try:\n        pass\n    except i32:\n        pass\n    return 0\n" "except requires an Exception")
reject(catch_order "def main() -> i32:\n    try:\n        pass\n    except:\n        pass\n    except Exception:\n        pass\n    return 0\n" "bare except must be the last")
reject(else_without_except "def main() -> i32:\n    try:\n        pass\n    else:\n        pass\n    finally:\n        pass\n    return 0\n" "try else requires except")
reject(class_arguments "class Error(Exception):\n    def __init__(self, code: i32) -> void:\n        self.message = str(code)\ndef main() -> i32:\n    raise Error\n    return 0\n" "requires constructor arguments")

file(WRITE "${OUTPUT}.uncaught.sere"
  "def fail() -> i32:\n    raise ValueError(\"uncaught detail\")\n    print(\"BAD\")\n    return 0\ndef main() -> i32:\n    return fail()\n")
execute_process(COMMAND "${SERE}" "${OUTPUT}.uncaught.sere" -o "${OUTPUT}.uncaught.exe"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 90)
if(NOT result STREQUAL "0")
  message(FATAL_ERROR "Uncaught regression failed to compile: ${output}\n${error}")
endif()
execute_process(COMMAND "${OUTPUT}.uncaught.exe" RESULT_VARIABLE result
  OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
string(REPLACE "\r\n" "\n" error "${error}")
if(NOT result STREQUAL "1" OR NOT output STREQUAL "" OR
   NOT error STREQUAL "ValueError: uncaught detail\n")
  message(FATAL_ERROR "Uncaught regression failed (${result}): ${output}\n${error}")
endif()
