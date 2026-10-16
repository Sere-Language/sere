# Kept separate so Wavio remains one independently maintained runtime subsystem.
target_sources(sere_runtime PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio.c
  ${CMAKE_CURRENT_SOURCE_DIR}/buffer.c
  ${CMAKE_CURRENT_SOURCE_DIR}/codec.c
  ${CMAKE_CURRENT_SOURCE_DIR}/engine.c
  ${CMAKE_CURRENT_SOURCE_DIR}/dsp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bindings.c
)
