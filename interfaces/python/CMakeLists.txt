cmake_minimum_required(VERSION 3.25)

project(biyos_python LANGUAGES NONE)

set(BUILD_SHARED_LIBS ON)
set(BIYOS_BUILD_TESTS OFF)
add_subdirectory(../.. biyos)

set(library $<TARGET_FILE_PREFIX:biyos>biyos$<TARGET_FILE_SUFFIX:biyos>)
file(GENERATE OUTPUT library.txt CONTENT ${library})

install(FILES $<TARGET_FILE:biyos>
    DESTINATION biyos
    RENAME ${library})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/library.txt DESTINATION biyos)
