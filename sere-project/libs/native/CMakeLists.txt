cmake_minimum_required(VERSION 3.20)
project(sere_native LANGUAGES C CXX)
add_library(sere_native STATIC native.cpp)
target_include_directories(sere_native PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../venv/include")
