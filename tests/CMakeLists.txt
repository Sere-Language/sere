# Compile and execute language feature regressions through both backends.
file(GLOB SERE_FEATURE_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/features/*.sere")
foreach(source IN LISTS SERE_FEATURE_TESTS)
  get_filename_component(feature "${source}" NAME_WE)
  foreach(backend IN ITEMS llvm serem)
    add_test(NAME sere.feature.${feature}.${backend}
      COMMAND "${CMAKE_COMMAND}"
        "-DCOMPILER=${CMAKE_BINARY_DIR}/bin/sere${CMAKE_EXECUTABLE_SUFFIX}"
        "-DSOURCE=${source}"
        "-DSTDLIB=${CMAKE_SOURCE_DIR}/stdlib"
        "-DBACKEND=${backend}"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${feature}-${backend}${CMAKE_EXECUTABLE_SUFFIX}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_feature.cmake")
    set_tests_properties(sere.feature.${feature}.${backend} PROPERTIES TIMEOUT 90)
  endforeach()
endforeach()

# Focused standard-library regressions. Windows tests avoid interactive desktop APIs.
add_test(NAME sere.bindings.diagnostics
  COMMAND "${CMAKE_COMMAND}"
    "-DCOMPILER=${CMAKE_BINARY_DIR}/bin/sere${CMAKE_EXECUTABLE_SUFFIX}"
    "-DSTDLIB=${CMAKE_SOURCE_DIR}/stdlib"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/binding-diagnostics"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_binding_diagnostics.cmake")

set(SERE_STDLIB_TEST_GROUPS crypto hash os sys sockets wsgi generics pathlib multiprocessing)
if(WIN32)
  list(APPEND SERE_STDLIB_TEST_GROUPS windows)
endif()
foreach(group IN LISTS SERE_STDLIB_TEST_GROUPS)
  file(GLOB sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${group}/*.sere")
  foreach(source IN LISTS sources)
    get_filename_component(test_name "${source}" NAME_WE)
    foreach(backend IN ITEMS llvm serem)
      add_test(NAME sere.${group}.${test_name}.${backend}
        COMMAND "${CMAKE_COMMAND}"
          "-DCOMPILER=${CMAKE_BINARY_DIR}/bin/sere${CMAKE_EXECUTABLE_SUFFIX}"
          "-DSOURCE=${source}"
          "-DSTDLIB=${CMAKE_SOURCE_DIR}/stdlib"
          "-DBACKEND=${backend}"
          "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${group}-${test_name}-${backend}${CMAKE_EXECUTABLE_SUFFIX}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/run_feature.cmake")
      set_tests_properties(sere.${group}.${test_name}.${backend} PROPERTIES
        TIMEOUT 90 LABELS "${group}")
    endforeach()
  endforeach()
endforeach()

# Docstring parsing and Markdown rendering (no LLVM, no code generation).
add_executable(sere_test_docformat DocFormatTest.cpp)
target_link_libraries(sere_test_docformat PRIVATE sere_docformat)
add_test(NAME sere.test.docformat COMMAND sere_test_docformat)

add_executable(sere_test_wavio WavioTest.cpp)
target_link_libraries(sere_test_wavio PRIVATE sere_runtime)
add_test(NAME sere.test.wavio COMMAND sere_test_wavio)
set_tests_properties(sere.test.wavio PROPERTIES TIMEOUT 30 LABELS "wavio")
