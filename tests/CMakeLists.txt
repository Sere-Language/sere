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
set(SERE_STDLIB_TEST_GROUPS hash os sys sockets wsgi)
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
