# Sere language runtime.
# Linked into programs produced by sere. Keep the core C so object ABI stays simple.

include("${CMAKE_SOURCE_DIR}/cmake/SereEmbedIcon.cmake")
set(SERE_ICON_ICO "${CMAKE_SOURCE_DIR}/icon.ico")
set(SERE_ICON_INC "${CMAKE_BINARY_DIR}/generated/sere_icon_ico.inc")
sere_embed_icon("${SERE_ICON_ICO}" "${SERE_ICON_INC}")

add_library(sere_runtime
  sere_rt.c
  sere_mod.c
  sere_sys.c
  sere_stdlib.c
  sere_re.c
  sere_gc.c
  sere_async.c
  sere_win.c
  sere_gl.c
  sere_http.c
  sere_proc.c
  sere_icon.c
)
target_include_directories(sere_runtime PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/include"
)
target_include_directories(sere_runtime PRIVATE
  "${CMAKE_BINARY_DIR}/generated"
)
set_source_files_properties(sere_icon.c PROPERTIES OBJECT_DEPENDS "${SERE_ICON_INC}")
sere_apply_warnings(sere_runtime)

set_target_properties(sere_runtime PROPERTIES
  OUTPUT_NAME sere_rt
  C_VISIBILITY_PRESET hidden
)

if(MSVC)
  set_property(TARGET sere_runtime PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
endif()

if(WIN32)
  target_link_libraries(sere_runtime PUBLIC
    user32
    gdi32
    opengl32
    shell32
    advapi32
    winhttp
    ws2_32
  )
else()
  target_link_libraries(sere_runtime PUBLIC m ${CMAKE_DL_LIBS} pthread)
endif()

find_package(Qt6 QUIET COMPONENTS Widgets)
if(NOT Qt6_FOUND)
  file(GLOB _sere_qt_hints
    "C:/Qt/6.*/msvc2022_64"
    "C:/Qt/6.*/msvc2019_64"
    "$ENV{LOCALAPPDATA}/sere/toolchains/qt6"
  )
  if(_sere_qt_hints)
    list(APPEND CMAKE_PREFIX_PATH ${_sere_qt_hints})
    find_package(Qt6 QUIET COMPONENTS Widgets)
  endif()
endif()
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)
if(Qt6_FOUND)
  message(STATUS "Sere Qt6 widgets: enabled (${Qt6_DIR})")
  add_library(sere_qt6 SHARED sere_qt6.cpp)
  target_compile_definitions(sere_qt6 PRIVATE SERE_HAS_QT6)
  target_include_directories(sere_qt6 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/generated"
  )
  set_source_files_properties(sere_qt6.cpp PROPERTIES OBJECT_DEPENDS "${SERE_ICON_INC}")
  target_link_libraries(sere_qt6 PRIVATE Qt6::Widgets)
  get_filename_component(_sere_qt_bin "${Qt6_DIR}/../../../bin" ABSOLUTE)
  get_filename_component(_sere_qt_plugins "${Qt6_DIR}/../../../plugins" ABSOLUTE)
  if(EXISTS "${_sere_qt_bin}")
    string(REPLACE "\\" "/" _sere_qt_bin "${_sere_qt_bin}")
    target_compile_definitions(sere_qt6 PRIVATE "SERE_QT6_BIN=\"${_sere_qt_bin}\"")
  endif()
  if(EXISTS "${_sere_qt_plugins}")
    string(REPLACE "\\" "/" _sere_qt_plugins "${_sere_qt_plugins}")
    target_compile_definitions(sere_qt6 PRIVATE "SERE_QT6_PLUGINS=\"${_sere_qt_plugins}\"")
  endif()
  set_target_properties(sere_qt6 PROPERTIES
    OUTPUT_NAME sere_qt6
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
  if(WIN32)
    add_custom_command(TARGET sere_qt6 POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE:Qt6::Gui>"
        "$<TARGET_FILE:Qt6::Widgets>"
        "$<TARGET_FILE_DIR:sere_qt6>"
    )
    if(EXISTS "${_sere_qt_plugins}/platforms/qwindows.dll")
      add_custom_command(TARGET sere_qt6 POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
          "$<TARGET_FILE_DIR:sere_qt6>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${_sere_qt_plugins}/platforms/qwindows.dll"
          "$<TARGET_FILE_DIR:sere_qt6>/platforms/qwindows.dll"
      )
    endif()
  endif()
else()
  message(STATUS "Sere Qt6 widgets: stub (set CMAKE_PREFIX_PATH to a Qt6 install to enable)")
  add_library(sere_qt6 STATIC sere_qt6.cpp)
  set_target_properties(sere_qt6 PROPERTIES
    OUTPUT_NAME sere_qt6
    C_VISIBILITY_PRESET hidden
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
endif()
if(MSVC)
  set_source_files_properties(sere_qt6.cpp PROPERTIES COMPILE_OPTIONS "/W3;/WX-")
endif()
