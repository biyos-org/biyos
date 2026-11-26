cmake_minimum_required(VERSION 3.25)

project(biyos_examples LANGUAGES C)

option(BIYOS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(biyos 0.1.0 EXACT CONFIG REQUIRED)

if(MSVC)
    set(warnings /W4 /permissive-)
    set(errors /WX)
else()
    set(warnings
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion)
    set(errors -Werror)
endif()
if(BIYOS_WARNINGS_AS_ERRORS)
    list(APPEND warnings ${errors})
endif()

enable_testing()

add_subdirectory(test/c)
