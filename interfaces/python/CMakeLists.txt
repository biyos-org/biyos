cmake_minimum_required(VERSION 3.25)

project(biyos_python LANGUAGES NONE)

find_package(biyos ${SKBUILD_PROJECT_VERSION} EXACT CONFIG REQUIRED)

get_target_property(kind biyos::biyos TYPE)
if(NOT kind STREQUAL "SHARED_LIBRARY")
    message(FATAL_ERROR "biyos::biyos is a ${kind}: the package needs the shared library")
endif()

set(library $<TARGET_FILE_PREFIX:biyos::biyos>biyos$<TARGET_FILE_SUFFIX:biyos::biyos>)
file(GENERATE OUTPUT library.txt CONTENT ${library})

get_target_property(header biyos::biyos HEADER_SET)
set(constants ${CMAKE_CURRENT_BINARY_DIR}/_header.py)
set(script ${CMAKE_CURRENT_SOURCE_DIR}/header.cmake)
add_custom_command(OUTPUT ${constants}
    COMMAND ${CMAKE_COMMAND} -D header=${header} -D output=${constants} -P ${script}
    DEPENDS ${header} ${script})
add_custom_target(header ALL DEPENDS ${constants})

install(FILES $<TARGET_FILE:biyos::biyos>
    DESTINATION biyos
    RENAME ${library})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/library.txt DESTINATION biyos)
install(FILES ${constants} DESTINATION biyos)
