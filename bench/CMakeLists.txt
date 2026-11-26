cmake_minimum_required(VERSION 3.25)

project(biyos_bench LANGUAGES CXX)

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

add_executable(roundtrip roundtrip.cpp)
target_link_libraries(roundtrip PRIVATE biyos::biyos)
set_target_properties(roundtrip PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(roundtrip PRIVATE ${warnings})
