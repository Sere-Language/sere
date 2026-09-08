include("${CMAKE_CURRENT_LIST_DIR}/run_async_await.cmake")

function(reject name source diagnostic)
  file(WRITE "${OUTPUT}.${name}.sere" "${source}")
  execute_process(COMMAND "${SERE}" --emit-llvm "${OUTPUT}.${name}.sere"
    -o "${OUTPUT}.${name}.ll" RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
  if(result STREQUAL "0" OR NOT error MATCHES "${diagnostic}")
    message(FATAL_ERROR "${name}: expected ${diagnostic}, got ${result}: ${output}\n${error}")
  endif()
endfunction()
reject(incompatible "def set_value(out_value: Ptr[i64]) -> void:\n    *out_value = 42\ndef main() -> i32:\n    value: i32 = 0\n    set_value(&value)\n    return 0\n" "parameter.*type mismatch")
reject(own_borrow "def main() -> i32:\n    value: i32 = 0\n    owner: Unique[i32] = &value\n    return 0\n" "cannot initialize")
reject(local_escape "def broken() -> Ptr[str]:\n    name: str = \"local\"\n    return &name\ndef main() -> i32:\n    return 0\n" "cannot return the address of local")
reject(param_escape "def broken(value: str) -> Ptr[str]:\n    return &value\ndef main() -> i32:\n    return 0\n" "cannot return the address of local")
reject(free_borrow "def main() -> i32:\n    value: i32 = 0\n    free(&value)\n    return 0\n" "cannot free an address borrowed")
reject(free_owner "def main() -> i32:\n    owner: Unique[i32] = unique[i32](0)\n    free(owner)\n    return 0\n" "owning pointers are released automatically")
reject(void_load "def main() -> i32:\n    value: Ptr[void] = None\n    load(value)\n    return 0\n" "non-void pointee")
reject(readonly "const fixed: i32 = 1\ndef main() -> i32:\n    value: Ptr[i32] = &fixed\n    return 0\n" "cannot take the address")
