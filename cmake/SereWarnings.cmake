# SereWarnings.cmake
# Compiler warning policy for the Sere toolchain. Warnings are informational:
# they are never errors, so a stricter compiler on one platform cannot block a
# build. Only the portable, low-noise diagnostics are requested.
# clang-cl treats -Wall as /Wall (every warning). Use MSVC-style /W4 instead.

function(sere_apply_warnings target_name)
  if(MSVC)
    target_compile_options("${target_name}" PRIVATE
      /W4
      /permissive-
      /utf-8
      /wd4324
    )
  else()
    target_compile_options("${target_name}" PRIVATE
      -Wall
      -Wextra
      -Wno-unused-parameter
    )
  endif()
endfunction()
