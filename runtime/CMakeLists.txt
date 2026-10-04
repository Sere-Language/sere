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
  sere_runtime.c
  sere_stdlib.c
  sere_re.c
  sere_gc.c
  sere_async.c
  sere_win.c
  sere_gl.c
  sere_http.c
  sere_wsgi.c
  sere_socket.c
  sere_proc.c
  sere_icon.c
)

# Metadata `sys` reports about the build and the compilation target. The target
# triple comes from the toolchain when one is configured, so a cross-compiled
# runtime describes its own target instead of the machine that built it.
target_compile_definitions(sere_runtime PRIVATE
  SERE_RUNTIME_BUILD="$<UPPER_CASE:$<CONFIG>>"
  SERE_RUNTIME_OPT="${SERE_RUNTIME_OPT_LEVEL}"
  SERE_TARGET_TRIPLE="${CMAKE_C_COMPILER_TARGET}"
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
