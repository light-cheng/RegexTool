cmake_minimum_required(VERSION 3.16)
project(regex_tool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/utf-8 /W4)
  # Single self-contained exe: statically link the C runtime.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(engine STATIC src/engine/engine.cpp)
target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(regex_tool WIN32 src/app/main.cpp src/app/app.manifest)
target_link_libraries(regex_tool PRIVATE engine comctl32 dwmapi uxtheme)

add_executable(engine_tests tests/engine_tests.cpp)
target_link_libraries(engine_tests PRIVATE engine)

enable_testing()
add_test(NAME engine_tests COMMAND engine_tests)
