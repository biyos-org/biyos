include(FetchContent)

FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    SYSTEM)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

function(biyos_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE GTest::gtest_main)
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_compile_definitions(${name} PRIVATE BIYOS_EXPECTED_VERSION="${PROJECT_VERSION}")
    set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
    biyos_set_warnings(${name})
    gtest_discover_tests(${name} PROPERTIES TIMEOUT 120)
endfunction()

biyos_add_test(test_core core/test_check.cpp core/test_clock.cpp core/test_environment.cpp core/test_error.cpp core/test_events.cpp core/test_format.cpp core/test_name.cpp core/test_options.cpp core/test_status.cpp core/test_timer.cpp core/test_tolerances.cpp core/test_utf8.cpp core/test_version.cpp)
target_link_libraries(test_core PRIVATE biyos_internal)

biyos_add_test(test_allocation core/allocations.cpp core/test_allocation.cpp expr/test_allocation.cpp)
target_include_directories(test_allocation PRIVATE core)
target_link_libraries(test_allocation PRIVATE biyos_internal)

add_library(local_name OBJECT EXCLUDE_FROM_ALL core/local_name.cpp)
target_link_libraries(local_name PRIVATE biyos_internal)
add_test(NAME NameTest.LocalBuffer
    COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} --target local_name --config $<CONFIG>)
set_tests_properties(NameTest.LocalBuffer PROPERTIES PASS_REGULAR_EXPRESSION "constant expression")

biyos_add_test(test_expr expr/reference.cpp expr/sampling.cpp expr/test_builder.cpp expr/test_canonical.cpp expr/test_compact.cpp expr/test_elementary.cpp expr/test_enclosure.cpp expr/test_graph.cpp expr/test_interval.cpp expr/test_parents.cpp expr/test_periodic.cpp expr/test_powers.cpp expr/test_tape.cpp expr/test_terms.cpp expr/test_value.cpp)
target_link_libraries(test_expr PRIVATE biyos_internal)

biyos_add_test(test_model model/test_constraints.cpp model/test_functions.cpp model/test_variables.cpp)
target_link_libraries(test_model PRIVATE biyos_internal)

biyos_add_test(test_capi capi/test_constraints.cpp capi/test_environment.cpp capi/test_events.cpp capi/test_firewall.cpp capi/test_format.cpp capi/test_functions.cpp capi/test_model.cpp capi/test_options.cpp capi/test_version.cpp)
target_link_libraries(test_capi PRIVATE biyos::biyos biyos_internal)

add_executable(test_capi_c capi/test_header.c)
target_link_libraries(test_capi_c PRIVATE biyos::biyos)
set_target_properties(test_capi_c PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
biyos_set_warnings(test_capi_c)
add_test(NAME CApiTest.CompilesAsC99 COMMAND test_capi_c)
