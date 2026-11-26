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

if(PROJECT_IS_TOP_LEVEL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(package_dir ${CMAKE_INSTALL_LIBDIR}/cmake/biyos)
    set(installed biyos)
    if(NOT BUILD_SHARED_LIBS)
        list(APPEND installed biyos_internal)
    endif()

    install(TARGETS ${installed} EXPORT biyosTargets FILE_SET HEADERS)
    install(EXPORT biyosTargets NAMESPACE biyos:: DESTINATION ${package_dir})

    configure_package_config_file(cmake/biyosConfig.cmake.in biyosConfig.cmake
        INSTALL_DESTINATION ${package_dir})
    write_basic_package_version_file(biyosConfigVersion.cmake COMPATIBILITY ExactVersion)
    install(FILES
        ${PROJECT_BINARY_DIR}/biyosConfig.cmake
        ${PROJECT_BINARY_DIR}/biyosConfigVersion.cmake
        DESTINATION ${package_dir})
endif()

if(BIYOS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
