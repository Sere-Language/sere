execute_process(
  COMMAND "${SERE}" "${SOURCE}" "--backend=${BACKEND}" -o "${OUTPUT}"
  RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_err
  TIMEOUT 90)
if(NOT compile_result STREQUAL "0")
  message(FATAL_ERROR "${BACKEND}/${CASE} compilation failed (${compile_result}): ${compile_out}\n${compile_err}")
endif()
set(run_dir "${OUTPUT}.work")
file(MAKE_DIRECTORY "${run_dir}")
file(WRITE "${run_dir}/input.txt" "hello world\n")
execute_process(
  COMMAND "${OUTPUT}" alpha "two words"
  WORKING_DIRECTORY "${run_dir}"
  INPUT_FILE "${run_dir}/input.txt"
  RESULT_VARIABLE run_result OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err
  TIMEOUT 20)
string(REPLACE "\r\n" "\n" run_out "${run_out}")
if(NOT run_result STREQUAL "0")
  message(FATAL_ERROR "${BACKEND}/${CASE} exited ${run_result}:\n${run_out}\n${run_err}")
endif()
if(CASE STREQUAL "objects")
  # argv[0] depends on the build directory; validate the list and both arguments.
  string(REGEX REPLACE "\n\\[[^\n]*, 'alpha', 'two words'\\]\n" "\nARGV\n" run_out "${run_out}")
  set(expected "[1, 2, 3]\nARGV\nHello, my name is Alice, I am 25 years old.\nHello, my name is Bob, I am 45 years old, and I am the mayor of Springfield.\nlen: 25\nThis is a person!\nBob\nThis is a person!\nAlice\nBob\n[Person(name=Alice, age=25), Person(name=Bob, age=45)]\n[Person(name=Alice, age=25), Person(name=Bob, age=45)]\n")
elseif(CASE STREQUAL "fs")
  set(expected "Input: Found 3 vowels\n['e', 'o', 'o']\nVowels: ['e', 'o', 'o']\n[]\n[86, 111, 119, 101, 108, 115]\nTrue\n[86, 111, 119, 101, 108, 115]\n[4, 9, 6]\n[5, 3, 1]\nTrue\nTrue\n")
  file(READ "${run_dir}/output.txt" written)
  if(NOT written STREQUAL "Vowels: ['e', 'o', 'o']")
    message(FATAL_ERROR "${BACKEND} wrote unexpected text: ${written}")
  endif()
  file(READ "${run_dir}/bytes.bin" written)
  if(NOT written STREQUAL "Vowels")
    message(FATAL_ERROR "${BACKEND} wrote unexpected bytes: ${written}")
  endif()
elseif(CASE STREQUAL "casts")
  set(expected "-7\n-7\n42\n9\n3\n")
else()
  set(expected "entry ok\n")
endif()
if(NOT run_result STREQUAL "0" OR NOT run_out STREQUAL expected)
  message(FATAL_ERROR "${BACKEND}/${CASE} failed (${run_result}):\n${run_out}\n${run_err}\nExpected:\n${expected}")
endif()
