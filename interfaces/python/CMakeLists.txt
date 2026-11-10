cmake_minimum_required(VERSION 3.25)

project(biyos_python LANGUAGES NONE)

set(BUILD_SHARED_LIBS ON)
set(BIYOS_BUILD_TESTS OFF)
add_subdirectory(../.. biyos)

install(FILES $<TARGET_FILE:biyos>
    DESTINATION biyos
    RENAME $<TARGET_FILE_PREFIX:biyos>biyos$<TARGET_FILE_SUFFIX:biyos>)
