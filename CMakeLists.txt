cmake_minimum_required(VERSION 3.25)

project(biyos VERSION 0.1.0 LANGUAGES C CXX)

option(BUILD_SHARED_LIBS "Build biyos as a shared library" ON)
option(BIYOS_BUILD_TESTS "Build the test suite" ON)
option(BIYOS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

include(cmake/BiyosWarnings.cmake)

if(NOT MSVC)
    add_compile_options(-ffp-contract=off)
endif()

add_subdirectory(src)

if(BIYOS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
