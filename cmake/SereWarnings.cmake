# SereWarnings.cmake
# Compiler warning policy for the Sere toolchain. Warnings are errors.
# clang-cl treats -Wall as /Wall (every warning). Use MSVC-style /W4 instead.

function(sere_apply_warnings target_name)
  if(MSVC)
    target_compile_options("${target_name}" PRIVATE
      /W4
      /WX
      /permissive-
      /utf-8
      /wd4324
    )
  else()
    target_compile_options("${target_name}" PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wshadow
      -Werror
      -Wno-unused-parameter
    )
  endif()
endfunction()
