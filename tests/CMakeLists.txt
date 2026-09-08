# Sere toolchain tests.

if(WIN32)
  add_test(NAME sere.test.updater
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
      -File "${CMAKE_CURRENT_SOURCE_DIR}/update.ps1")
endif()

add_test(NAME sere.test.generic_tuple
  COMMAND "${CMAKE_COMMAND}"
    "-DSERE=${CMAKE_BINARY_DIR}/bin/sere${CMAKE_EXECUTABLE_SUFFIX}"
    "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/generic_tuple.sere"
    "-DOUTPUT=${CMAKE_BINARY_DIR}/generic_tuple${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_generic_tuple.cmake")
set_tests_properties(sere.test.generic_tuple PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib"
  TIMEOUT 120)

add_executable(sere_smoke_llvm smoke_llvm.cpp)
target_link_libraries(sere_smoke_llvm PRIVATE sere_codegen)
sere_apply_warnings(sere_smoke_llvm)
add_test(NAME sere.smoke.llvm COMMAND sere_smoke_llvm)

add_executable(sere_test_lex lex_basic.cpp)
target_link_libraries(sere_test_lex PRIVATE sere_lex)
sere_apply_warnings(sere_test_lex)
add_test(NAME sere.test.lex COMMAND sere_test_lex)

add_executable(sere_test_parse parse_fn.cpp)
target_link_libraries(sere_test_parse PRIVATE sere_parse)
sere_apply_warnings(sere_test_parse)
add_test(NAME sere.test.parse COMMAND sere_test_parse)

add_executable(sere_test_parse_lang parse_lang.cpp)
target_link_libraries(sere_test_parse_lang PRIVATE sere_parse)
sere_apply_warnings(sere_test_parse_lang)
add_test(NAME sere.test.parse_lang COMMAND sere_test_parse_lang)

add_executable(sere_test_sema sema_types.cpp)
target_link_libraries(sere_test_sema PRIVATE sere_sema)
sere_apply_warnings(sere_test_sema)
add_test(NAME sere.test.sema COMMAND sere_test_sema)

add_executable(sere_test_generic_constraints generic_constraints.cpp)
target_link_libraries(sere_test_generic_constraints PRIVATE sere_driver)
sere_apply_warnings(sere_test_generic_constraints)
add_test(NAME sere.test.generic_constraints COMMAND sere_test_generic_constraints)

add_test(NAME sere.example.generic_constraints
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/generic_constraints.sere"
          -o "${CMAKE_BINARY_DIR}/generic_constraints.ll")
set_tests_properties(sere.example.generic_constraints PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

foreach(_any_case IN ITEMS values bad_cast)
  if(_any_case STREQUAL "values")
    set(_any_source "${CMAKE_SOURCE_DIR}/examples/any.sere")
    set(_any_failure OFF)
  else()
    set(_any_source "${CMAKE_CURRENT_SOURCE_DIR}/any_bad_cast.sere")
    set(_any_failure ON)
  endif()
  add_test(NAME sere.test.any_${_any_case}
    COMMAND "${CMAKE_COMMAND}"
      "-DSERE=${CMAKE_BINARY_DIR}/bin/sere${CMAKE_EXECUTABLE_SUFFIX}"
      "-DSOURCE=${_any_source}"
      "-DOUTPUT=${CMAKE_BINARY_DIR}/any_${_any_case}${CMAKE_EXECUTABLE_SUFFIX}"
      "-DEXPECT_MISMATCH=${_any_failure}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/run_any.cmake")
  set_tests_properties(sere.test.any_${_any_case} PROPERTIES
    ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib"
    TIMEOUT 120)
endforeach()

add_executable(sere_test_sema_control sema_control.cpp)
target_link_libraries(sere_test_sema_control PRIVATE sere_sema)
sere_apply_warnings(sere_test_sema_control)
add_test(NAME sere.test.sema_control COMMAND sere_test_sema_control)

add_executable(sere_test_sema_class sema_class.cpp)
target_link_libraries(sere_test_sema_class PRIVATE sere_sema)
sere_apply_warnings(sere_test_sema_class)
add_test(NAME sere.test.sema_class COMMAND sere_test_sema_class)

add_executable(sere_test_diag diag_pretty.cpp)
target_link_libraries(sere_test_diag PRIVATE sere_sema)
sere_apply_warnings(sere_test_diag)
add_test(NAME sere.test.diag COMMAND sere_test_diag)

add_executable(sere_test_macro_tt macro_tt.cpp)
target_link_libraries(sere_test_macro_tt PRIVATE sere_macro)
sere_apply_warnings(sere_test_macro_tt)
add_test(NAME sere.test.macro_tt COMMAND sere_test_macro_tt)

add_executable(sere_test_macro_expand macro_expand.cpp)
target_link_libraries(sere_test_macro_expand PRIVATE sere_macro)
sere_apply_warnings(sere_test_macro_expand)
add_test(NAME sere.test.macro_expand COMMAND sere_test_macro_expand)

add_executable(sere_test_macro_errors macro_errors.cpp)
target_link_libraries(sere_test_macro_errors PRIVATE sere_macro)
sere_apply_warnings(sere_test_macro_errors)
add_test(NAME sere.test.macro_errors COMMAND sere_test_macro_errors)

add_test(NAME sere.example.hello
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/hello.sere"
          -o "${CMAKE_BINARY_DIR}/hello.ll")
set_tests_properties(sere.example.hello PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.hello.ast
  COMMAND "$<TARGET_FILE:sere>" --dump-ast
          "${CMAKE_SOURCE_DIR}/examples/hello.sere")
set_tests_properties(sere.example.hello.ast PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.hello.symbols
  COMMAND "$<TARGET_FILE:sere>" --dump-symbols
          "${CMAKE_SOURCE_DIR}/examples/hello.sere")
set_tests_properties(sere.example.hello.symbols PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.hello.raw-ir
  COMMAND "${CMAKE_BINARY_DIR}/bin/sere${CMAKE_EXECUTABLE_SUFFIX}" --dump-llvm-ir-raw
          "${CMAKE_SOURCE_DIR}/examples/hello.sere")
set_tests_properties(sere.example.hello.raw-ir PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.hello.combined
  COMMAND "$<TARGET_FILE:sere>" --dump-llvm-ir-raw --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/hello.sere"
          -o "${CMAKE_BINARY_DIR}/hello-combined.ll")
set_tests_properties(sere.example.hello.combined PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.aliases
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/aliases.sere"
          -o "${CMAKE_BINARY_DIR}/aliases.ll")
set_tests_properties(sere.example.aliases PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.memory
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/memory.sere"
          -o "${CMAKE_BINARY_DIR}/memory.ll")
set_tests_properties(sere.example.memory PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.control
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/control.sere"
          -o "${CMAKE_BINARY_DIR}/control.ll")
set_tests_properties(sere.example.control PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.point
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/point.sere"
          -o "${CMAKE_BINARY_DIR}/point.ll")
set_tests_properties(sere.example.point PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.types
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/types.sere"
          -o "${CMAKE_BINARY_DIR}/types.ll")
set_tests_properties(sere.example.types PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.callable
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/callable.sere"
          -o "${CMAKE_BINARY_DIR}/callable.ll")
set_tests_properties(sere.example.callable PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.collections
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/collections.sere"
          -o "${CMAKE_BINARY_DIR}/collections.ll")
set_tests_properties(sere.example.collections PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.oop
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/oop.sere"
          -o "${CMAKE_BINARY_DIR}/oop.ll")
set_tests_properties(sere.example.oop PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.imports
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/imports.sere"
          -o "${CMAKE_BINARY_DIR}/imports.ll")
set_tests_properties(sere.example.imports PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.lang
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/lang.sere"
          -o "${CMAKE_BINARY_DIR}/lang.ll")
set_tests_properties(sere.example.lang PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.features
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/features.sere"
          -o "${CMAKE_BINARY_DIR}/features.ll")
set_tests_properties(sere.example.features PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.introspect
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/introspect.sere"
          -o "${CMAKE_BINARY_DIR}/introspect.ll")
set_tests_properties(sere.example.introspect PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.numeric
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/numeric.sere"
          -o "${CMAKE_BINARY_DIR}/numeric.ll")
set_tests_properties(sere.example.numeric PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.enum_print
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/enum_print.sere"
          -o "${CMAKE_BINARY_DIR}/enum_print.ll")
set_tests_properties(sere.example.enum_print PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.enums
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/enums.sere"
          -o "${CMAKE_BINARY_DIR}/enums.ll")
set_tests_properties(sere.example.enums PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.stdlib_mods
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/stdlib_mods.sere"
          -o "${CMAKE_BINARY_DIR}/stdlib_mods.ll")
set_tests_properties(sere.example.stdlib_mods PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.stdlib_more
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/stdlib_more.sere"
          -o "${CMAKE_BINARY_DIR}/stdlib_more.ll")
set_tests_properties(sere.example.stdlib_more PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.gc_mem
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/gc_mem.sere"
          -o "${CMAKE_BINARY_DIR}/gc_mem.ll")
set_tests_properties(sere.example.gc_mem PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.structs
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/structs.sere"
          -o "${CMAKE_BINARY_DIR}/structs.ll")
set_tests_properties(sere.example.structs PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.strings
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/strings.sere"
          -o "${CMAKE_BINARY_DIR}/strings.ll")
set_tests_properties(sere.example.strings PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.dunders
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/dunders.sere"
          -o "${CMAKE_BINARY_DIR}/dunders.ll")
set_tests_properties(sere.example.dunders PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.errors
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/errors.sere"
          -o "${CMAKE_BINARY_DIR}/errors.ll")
set_tests_properties(sere.example.errors PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.macros_basic
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/macros_basic.sere"
          -o "${CMAKE_BINARY_DIR}/macros_basic.ll")
set_tests_properties(sere.example.macros_basic PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.macros_html
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/macros_html.sere"
          -o "${CMAKE_BINARY_DIR}/macros_html.ll")
set_tests_properties(sere.example.macros_html PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib"
  TIMEOUT 20)

add_test(NAME sere.example.macros_pipeline
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/macros_pipeline.sere"
          -o "${CMAKE_BINARY_DIR}/macros_pipeline.ll")
set_tests_properties(sere.example.macros_pipeline PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.macros_typed
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/macros_typed.sere"
          -o "${CMAKE_BINARY_DIR}/macros_typed.ll")
set_tests_properties(sere.example.macros_typed PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.platform
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/platform.sere"
          -o "${CMAKE_BINARY_DIR}/platform.ll")
set_tests_properties(sere.example.platform PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.qt6
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/qt6_app.sere"
          -o "${CMAKE_BINARY_DIR}/qt6_app.ll")
set_tests_properties(sere.example.qt6 PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.gl_info
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/gl_info.sere"
          -o "${CMAKE_BINARY_DIR}/gl_info.ll")
set_tests_properties(sere.example.gl_info PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.example.gl_triangle
  COMMAND "$<TARGET_FILE:sere>" --emit-llvm
          "${CMAKE_SOURCE_DIR}/examples/gl_triangle.sere"
          -o "${CMAKE_BINARY_DIR}/gl_triangle.ll")
set_tests_properties(sere.example.gl_triangle PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

foreach(_sere_example IN ITEMS
    pythonish walrus lambda tuples defer with_ctx del_list flags_enum requests wsgi
    import_exc properties any)
  add_test(NAME sere.example.${_sere_example}
    COMMAND "$<TARGET_FILE:sere>" --emit-llvm
            "${CMAKE_SOURCE_DIR}/examples/${_sere_example}.sere"
            -o "${CMAKE_BINARY_DIR}/${_sere_example}.ll")
  set_tests_properties(sere.example.${_sere_example} PROPERTIES
    ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")
endforeach()

foreach(_async_case IN ITEMS await suspend)
  add_test(NAME sere.test.async_${_async_case}
    COMMAND "${CMAKE_COMMAND}"
      "-DSERE=${CMAKE_BINARY_DIR}/bin/sere${CMAKE_EXECUTABLE_SUFFIX}"
      "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/async_${_async_case}.sere"
      "-DOUTPUT=${CMAKE_BINARY_DIR}/async_${_async_case}${CMAKE_EXECUTABLE_SUFFIX}"
      "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/async_${_async_case}.expected"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/run_async_await.cmake")
  set_tests_properties(sere.test.async_${_async_case} PROPERTIES
    ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib"
    TIMEOUT 120)
endforeach()

add_executable(sere_test_project_cli project_cli.cpp)
target_link_libraries(sere_test_project_cli PRIVATE sere_driver)
sere_apply_warnings(sere_test_project_cli)
add_test(NAME sere.test.project_cli COMMAND sere_test_project_cli)

add_executable(sere_test_import_private import_private.cpp)
target_link_libraries(sere_test_import_private PRIVATE sere_driver)
sere_apply_warnings(sere_test_import_private)
add_test(NAME sere.test.import_private COMMAND sere_test_import_private)

add_executable(sere_test_sema_shadow_types sema_shadow_types.cpp)
target_link_libraries(sere_test_sema_shadow_types PRIVATE sere_driver)
sere_apply_warnings(sere_test_sema_shadow_types)
add_test(NAME sere.test.sema_shadow_types COMMAND sere_test_sema_shadow_types)

add_executable(sere_test_library_pack library_pack.cpp)
target_link_libraries(sere_test_library_pack PRIVATE sere_driver)
sere_apply_warnings(sere_test_library_pack)
add_test(NAME sere.test.library_pack COMMAND sere_test_library_pack)

add_executable(sere_test_type_ignore type_ignore.cpp)
target_link_libraries(sere_test_type_ignore PRIVATE sere_sema)
sere_apply_warnings(sere_test_type_ignore)
add_test(NAME sere.test.type_ignore COMMAND sere_test_type_ignore)

add_executable(sere_test_macro_lsp macro_lsp.cpp)
target_link_libraries(sere_test_macro_lsp PRIVATE sere_driver)
target_compile_definitions(sere_test_macro_lsp PRIVATE SERE_STDLIB_DIR="${CMAKE_SOURCE_DIR}/stdlib")
sere_apply_warnings(sere_test_macro_lsp)
add_test(NAME sere.test.macro_lsp COMMAND sere_test_macro_lsp)
set_tests_properties(sere.test.macro_lsp PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_executable(sere_test_lsp_semantic lsp_semantic.cpp)
target_link_libraries(sere_test_lsp_semantic PRIVATE sere_driver)
target_compile_definitions(sere_test_lsp_semantic PRIVATE SERE_STDLIB_DIR="${CMAKE_SOURCE_DIR}/stdlib")
sere_apply_warnings(sere_test_lsp_semantic)
add_test(NAME sere.test.lsp_semantic COMMAND sere_test_lsp_semantic)
set_tests_properties(sere.test.lsp_semantic PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_executable(sere_test_import_completion import_completion.cpp)
target_link_libraries(sere_test_import_completion PRIVATE sere_driver)
sere_apply_warnings(sere_test_import_completion)
add_test(NAME sere.test.import_completion COMMAND sere_test_import_completion)

add_executable(sere_test_member_completion member_completion.cpp)
target_link_libraries(sere_test_member_completion PRIVATE sere_driver)
target_compile_definitions(sere_test_member_completion PRIVATE SERE_STDLIB_DIR="${CMAKE_SOURCE_DIR}/stdlib")
sere_apply_warnings(sere_test_member_completion)
add_test(NAME sere.test.member_completion COMMAND sere_test_member_completion)
set_tests_properties(sere.test.member_completion PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib")

add_test(NAME sere.test.exceptions
  COMMAND "${CMAKE_COMMAND}"
    "-DSERE=${CMAKE_BINARY_DIR}/bin/sere${CMAKE_EXECUTABLE_SUFFIX}"
    "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/exceptions.sere"
    "-DOUTPUT=${CMAKE_BINARY_DIR}/exceptions${CMAKE_EXECUTABLE_SUFFIX}"
    "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/exceptions.expected"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_exceptions.cmake")
set_tests_properties(sere.test.exceptions PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib"
  TIMEOUT 120)

foreach(_case IN ITEMS branch_scope hash_file)
  add_test(NAME sere.test.${_case}
    COMMAND "${CMAKE_COMMAND}"
      "-DSERE=${CMAKE_BINARY_DIR}/bin/sere${CMAKE_EXECUTABLE_SUFFIX}"
      "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/${_case}.sere"
      "-DOUTPUT=${CMAKE_BINARY_DIR}/${_case}${CMAKE_EXECUTABLE_SUFFIX}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/run_async_await.cmake")
  set_tests_properties(sere.test.${_case} PROPERTIES
    ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib"
    TIMEOUT 120)
endforeach()
add_executable(sere_test_file_hash file_hash.c)
target_include_directories(sere_test_file_hash PRIVATE "${CMAKE_SOURCE_DIR}/runtime")
target_link_libraries(sere_test_file_hash PRIVATE sere_runtime)
if(MSVC)
  target_compile_definitions(sere_test_file_hash PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
sere_apply_warnings(sere_test_file_hash)
add_test(NAME sere.test.file_hash_runtime COMMAND sere_test_file_hash)

add_test(NAME sere.test.pointer_functions
  COMMAND "${CMAKE_COMMAND}"
    "-DSERE=${CMAKE_BINARY_DIR}/bin/sere${CMAKE_EXECUTABLE_SUFFIX}"
    "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/pointer_functions.sere"
    "-DOUTPUT=${CMAKE_BINARY_DIR}/pointer_functions${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_pointer_functions.cmake")
set_tests_properties(sere.test.pointer_functions PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib"
  TIMEOUT 120)

add_test(NAME sere.test.generic_pair_copy
  COMMAND "${CMAKE_COMMAND}"
    "-DSERE=${CMAKE_BINARY_DIR}/bin/sere${CMAKE_EXECUTABLE_SUFFIX}"
    "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/generic_pair_copy.sere"
    "-DOUTPUT=${CMAKE_BINARY_DIR}/generic_pair_copy${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_generic_pair_copy.cmake")
set_tests_properties(sere.test.generic_pair_copy PROPERTIES
  ENVIRONMENT "SERE_STDLIB=${CMAKE_SOURCE_DIR}/stdlib"
  TIMEOUT 120)
