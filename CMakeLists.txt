cmake_minimum_required(VERSION 3.20)
project(AnimCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(animcore STATIC
    core/src/ai.cpp
    core/src/brush.cpp
    core/src/celbox_c.cpp
    core/src/document.cpp
    core/src/drawing.cpp
    core/src/easing.cpp
    core/src/edit.cpp
    core/src/evaluate.cpp
    core/src/layer.cpp
    core/src/json.cpp
    core/src/ops.cpp
    core/src/serialize.cpp
    core/src/session.cpp
    core/src/tessellate.cpp
)
target_include_directories(animcore PUBLIC core/include)
if(MSVC)
    target_compile_options(animcore PRIVATE /W4)
else()
    target_compile_options(animcore PRIVATE -Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
endif()

option(ANIMCORE_TESTS "Build tests" ON)
if(ANIMCORE_TESTS)
    enable_testing()
    add_executable(animcore_tests tests/test_main.cpp)
    target_link_libraries(animcore_tests PRIVATE animcore)
    add_test(NAME animcore_tests COMMAND animcore_tests)
endif()
