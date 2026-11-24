cmake_minimum_required(VERSION 3.25)

project(biyos_python LANGUAGES NONE)

file(SHA256 ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE shipped_licence)
file(SHA256 ${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE root_licence)
if(NOT shipped_licence STREQUAL root_licence)
    message(FATAL_ERROR "LICENSE is not the repository licence: check out with symbolic links")
endif()

set(BUILD_SHARED_LIBS ON)
set(BIYOS_BUILD_TESTS OFF)
add_subdirectory(../.. biyos)

set(library $<TARGET_FILE_PREFIX:biyos>biyos$<TARGET_FILE_SUFFIX:biyos>)
file(GENERATE OUTPUT library.txt CONTENT ${library})

set(header ${biyos_SOURCE_DIR}/include/biyos/biyos.h)
set(constants ${CMAKE_CURRENT_BINARY_DIR}/_header.py)
set(script ${CMAKE_CURRENT_SOURCE_DIR}/header.cmake)
add_custom_command(OUTPUT ${constants}
    COMMAND ${CMAKE_COMMAND} -D header=${header} -D output=${constants} -P ${script}
    DEPENDS ${header} ${script})
add_custom_target(header ALL DEPENDS ${constants})

install(FILES $<TARGET_FILE:biyos>
    DESTINATION biyos
    RENAME ${library})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/library.txt DESTINATION biyos)
install(FILES ${constants} DESTINATION biyos)
