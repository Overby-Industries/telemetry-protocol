cmake_minimum_required(VERSION 3.16)
project(otf1 LANGUAGES CXX VERSION 0.1.0)

# libotf1 -- the C++ reference implementation of OTF-1. The format itself is
# defined by schema/otf-1.schema.json; see cpp/include/otf1/otf1.hpp.
#
# Consume it from another CMake project with:
#
#     add_subdirectory(path/to/telemetry-protocol)
#     target_link_libraries(your_target PRIVATE otf1::otf1)
#
# That builds only the library. The tests and the otf1_check tool are built
# when this repository is the top-level project.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(OTF1_IS_TOP_LEVEL ON)
else()
    set(OTF1_IS_TOP_LEVEL OFF)
endif()

option(OTF1_BUILD_TESTS "Build the libotf1 test suite" ${OTF1_IS_TOP_LEVEL})
option(OTF1_BUILD_TOOLS "Build the otf1_check command-line tool" ${OTF1_IS_TOP_LEVEL})

add_library(otf1 STATIC
    cpp/src/json.cpp
    cpp/src/time.cpp
    cpp/src/otf1.cpp
)

add_library(otf1::otf1 ALIAS otf1)

target_compile_features(otf1 PUBLIC cxx_std_17)
set_target_properties(otf1 PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(otf1 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(otf1 PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(otf1 PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

if(OTF1_BUILD_TOOLS)
    add_executable(otf1_check cpp/tools/otf1_check.cpp)
    target_link_libraries(otf1_check PRIVATE otf1)
    # The tool's JSON-only mode drives the parser directly, which is private
    # to the library.
    target_include_directories(otf1_check PRIVATE cpp/src)
    if(MSVC)
        target_compile_options(otf1_check PRIVATE /utf-8)
    endif()
endif()

if(OTF1_BUILD_TESTS)
    enable_testing()

    add_executable(otf1_tests
        cpp/tests/main.cpp
        cpp/tests/json_tests.cpp
        cpp/tests/time_tests.cpp
        cpp/tests/otf1_tests.cpp
    )
    target_link_libraries(otf1_tests PRIVATE otf1)
    target_include_directories(otf1_tests PRIVATE cpp/src)
    # The codec tests read examples/ and tests/invalid/ from this repository:
    # the same files the JSON Schema is validated against.
    target_compile_definitions(otf1_tests PRIVATE
        OTF1_REPO_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(otf1_tests PRIVATE /utf-8)
    endif()

    add_test(NAME otf1_tests COMMAND otf1_tests)
endif()
