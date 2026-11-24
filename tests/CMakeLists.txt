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
    target_compile_definitions(${name} PRIVATE
        BIYOS_EXPECTED_VERSION="${PROJECT_VERSION}")
    set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
    biyos_set_warnings(${name})
    gtest_discover_tests(${name} PROPERTIES TIMEOUT 120)
endfunction()

biyos_add_test(test_core
    core/env/test_env.cpp
    core/env/test_error.cpp
    core/env/test_status.cpp
    core/env/test_thread.cpp
    core/env/test_version.cpp
    core/events/test_deliveries.cpp
    core/events/test_events.cpp
    core/events/test_format.cpp
    core/events/test_hooks.cpp
    core/events/test_records.cpp
    core/memory/test_room.cpp
    core/memory/test_usage.cpp
    core/options/test_check.cpp
    core/options/test_flags.cpp
    core/options/test_options.cpp
    core/options/test_settings.cpp
    core/options/test_tolerances.cpp
    core/text/test_name.cpp
    core/text/test_strings.cpp
    core/text/test_utf8.cpp
    core/timing/test_clock.cpp
    core/timing/test_timer.cpp)
target_include_directories(test_core PRIVATE core)
target_link_libraries(test_core PRIVATE biyos_internal)

biyos_add_test(test_allocation
    core/allocations.cpp
    core/memory/test_allocation.cpp
    expr/test_allocation.cpp
    model/test_allocation.cpp)
target_include_directories(test_allocation PRIVATE core)
target_link_libraries(test_allocation PRIVATE biyos_internal)

add_library(local_name OBJECT EXCLUDE_FROM_ALL core/local_name.cpp)
target_link_libraries(local_name PRIVATE biyos_internal)
add_test(NAME NameTest.LocalBuffer
    COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR}
        --target local_name --config $<CONFIG>)
set_tests_properties(NameTest.LocalBuffer PROPERTIES
    PASS_REGULAR_EXPRESSION "constant expression")

biyos_add_test(test_expr
    expr/chords.cpp
    expr/reference.cpp
    expr/sampling.cpp
    expr/curvature/draws.cpp
    expr/curvature/test_bends.cpp
    expr/curvature/test_curvature.cpp
    expr/curvature/test_waves.cpp
    expr/derivatives/test_derivative.cpp
    expr/derivatives/test_partials.cpp
    expr/forms/test_canonical.cpp
    expr/forms/test_terms.cpp
    expr/graph/test_builder.cpp
    expr/graph/test_compact.cpp
    expr/graph/test_graph.cpp
    expr/graph/test_parents.cpp
    expr/graph/test_tape.cpp
    expr/graph/test_uses.cpp
    expr/intervals/test_enclosure.cpp
    expr/intervals/test_interval.cpp
    expr/values/test_arithmetic.cpp
    expr/values/test_elementary.cpp
    expr/values/test_exponents.cpp
    expr/values/test_periodic.cpp
    expr/values/test_powers.cpp
    expr/values/test_value.cpp)
target_include_directories(test_expr PRIVATE expr)
target_link_libraries(test_expr PRIVATE biyos_internal)

biyos_add_test(test_model
    model/edits/test_compare.cpp
    model/edits/test_copy.cpp
    model/edits/test_delete.cpp
    model/edits/test_names.cpp
    model/functions/test_callbacks.cpp
    model/functions/test_encoding.cpp
    model/functions/test_expressions.cpp
    model/functions/test_functions.cpp
    model/functions/test_quadratic.cpp
    model/records/test_complements.cpp
    model/records/test_cones.cpp
    model/records/test_constraints.cpp
    model/records/test_indicators.cpp
    model/records/test_objectives.cpp
    model/records/test_sos.cpp
    model/records/test_variables.cpp)
target_include_directories(test_model PRIVATE model)
target_link_libraries(test_model PRIVATE biyos_internal)

biyos_add_test(test_io
    io/test_lines.cpp
    io/mps/test_faults.cpp
    io/mps/test_leftovers.cpp
    io/mps/test_mps.cpp
    io/mps/test_refusals.cpp
    io/mps/test_write.cpp)
target_link_libraries(test_io PRIVATE biyos_internal)

biyos_add_test(test_capi
    capi/sample.cpp
    capi/test_adders.cpp
    capi/test_callbacks.cpp
    capi/test_complements.cpp
    capi/test_cones.cpp
    capi/test_constants.cpp
    capi/test_constraints.cpp
    capi/test_copy.cpp
    capi/test_delete.cpp
    capi/test_env.cpp
    capi/test_events.cpp
    capi/test_files.cpp
    capi/test_expressions.cpp
    capi/test_firewall.cpp
    capi/test_format.cpp
    capi/test_functions.cpp
    capi/test_indicators.cpp
    capi/test_interrupt.cpp
    capi/test_linear.cpp
    capi/test_model.cpp
    capi/test_names.cpp
    capi/test_objectives.cpp
    capi/test_options.cpp
    capi/test_pairs.cpp
    capi/test_products.cpp
    capi/test_quadratic.cpp
    capi/test_ranges.cpp
    capi/test_rebuild.cpp
    capi/test_sos.cpp
    capi/test_spans.cpp
    capi/test_terms.cpp
    capi/test_threads.cpp
    capi/test_version.cpp)
target_link_libraries(test_capi PRIVATE biyos::biyos biyos_internal)

add_executable(test_capi_c capi/test_header.c)
target_link_libraries(test_capi_c PRIVATE biyos::biyos)
set_target_properties(test_capi_c PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF)
biyos_set_warnings(test_capi_c)
add_test(NAME CApiTest.CompilesAsC99 COMMAND test_capi_c)
