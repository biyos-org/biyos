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
    core/env/test_check.cpp
    core/env/test_env.cpp
    core/env/test_error.cpp
    core/env/test_pool.cpp
    core/env/test_status.cpp
    core/env/test_thread.cpp
    core/env/test_version.cpp
    core/events/test_deliveries.cpp
    core/events/test_events.cpp
    core/events/test_format.cpp
    core/events/test_hooks.cpp
    core/events/test_records.cpp
    core/memory/test_room.cpp
    core/memory/test_slice.cpp
    core/memory/test_usage.cpp
    core/numeric/test_identical.cpp
    core/numeric/test_numbers.cpp
    core/options/test_flags.cpp
    core/options/test_options.cpp
    core/options/test_settings.cpp
    core/options/test_tolerances.cpp
    core/text/test_bignum.cpp
    core/text/test_chars.cpp
    core/text/test_name.cpp
    core/text/test_strings.cpp
    core/text/test_text.cpp
    core/text/test_utf8.cpp
    core/timing/test_clock.cpp
    core/timing/test_timer.cpp)
target_include_directories(test_core PRIVATE core/utils expr/utils)
target_link_libraries(test_core PRIVATE biyos_internal)

biyos_add_test(test_allocation
    core/memory/test_allocation.cpp
    core/utils/allocations.cpp
    expr/test_allocation.cpp
    model/evaluation/test_allocation.cpp
    model/test_allocation.cpp)
target_include_directories(test_allocation PRIVATE core/utils)
target_link_libraries(test_allocation PRIVATE biyos_internal)

add_library(local_name OBJECT EXCLUDE_FROM_ALL core/text/local_name.cpp)
target_link_libraries(local_name PRIVATE biyos_internal)
add_test(NAME NameTest.LocalBuffer
    COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR}
        --target local_name --config $<CONFIG>)
set_tests_properties(NameTest.LocalBuffer PROPERTIES
    PASS_REGULAR_EXPRESSION "constant expression")

biyos_add_test(test_expr
    expr/analysis/curvature/draws.cpp
    expr/analysis/curvature/test_angle.cpp
    expr/analysis/curvature/test_bends.cpp
    expr/analysis/curvature/test_curvature.cpp
    expr/analysis/curvature/test_segments.cpp
    expr/analysis/curvature/test_waves.cpp
    expr/analysis/intervals/test_atan2.cpp
    expr/analysis/intervals/test_enclosure.cpp
    expr/analysis/intervals/test_interval.cpp
    expr/analysis/intervals/test_logic.cpp
    expr/analysis/intervals/test_piecewise.cpp
    expr/evaluation/derivatives/test_angle.cpp
    expr/evaluation/derivatives/test_branches.cpp
    expr/evaluation/derivatives/test_derivative.cpp
    expr/evaluation/derivatives/test_joiner.cpp
    expr/evaluation/derivatives/test_lanes.cpp
    expr/evaluation/derivatives/test_memo.cpp
    expr/evaluation/derivatives/test_partials.cpp
    expr/evaluation/derivatives/test_reach.cpp
    expr/evaluation/hessian/test_colored.cpp
    expr/evaluation/hessian/test_coloring.cpp
    expr/evaluation/hessian/test_elements.cpp
    expr/evaluation/hessian/test_hessian.cpp
    expr/evaluation/hessian/test_pattern.cpp
    expr/evaluation/hessian/test_pushing.cpp
    expr/evaluation/hessian/test_tangents.cpp
    expr/evaluation/threads/test_levels.cpp
    expr/evaluation/threads/test_threads.cpp
    expr/evaluation/values/test_arithmetic.cpp
    expr/evaluation/values/test_conditions.cpp
    expr/evaluation/values/test_elementary.cpp
    expr/evaluation/values/test_exponents.cpp
    expr/evaluation/values/test_periodic.cpp
    expr/evaluation/values/test_piecewise.cpp
    expr/evaluation/values/test_powers.cpp
    expr/evaluation/values/test_value.cpp
    expr/forms/test_canonical.cpp
    expr/forms/test_terms.cpp
    expr/graph/test_builder.cpp
    expr/graph/test_compact.cpp
    expr/graph/test_graph.cpp
    expr/graph/test_parents.cpp
    expr/graph/test_points.cpp
    expr/graph/test_sharing.cpp
    expr/graph/test_tape.cpp
    expr/graph/test_uses.cpp
    expr/utils/chords.cpp
    expr/utils/reference.cpp
    expr/utils/sampling.cpp)
target_include_directories(test_expr PRIVATE expr/utils utils)
target_link_libraries(test_expr PRIVATE biyos_internal)

biyos_add_test(test_model
    model/edits/test_compare.cpp
    model/edits/test_copy.cpp
    model/edits/test_delete.cpp
    model/edits/test_names.cpp
    model/evaluation/test_evaluation.cpp
    model/evaluation/test_mixed.cpp
    model/evaluation/test_numbering.cpp
    model/evaluation/test_threads.cpp
    model/parts/functions/test_callbacks.cpp
    model/parts/functions/test_encoding.cpp
    model/parts/functions/test_expressions.cpp
    model/parts/functions/test_functions.cpp
    model/parts/functions/test_piecewise.cpp
    model/parts/functions/test_quadratic.cpp
    model/parts/functions/test_transpose.cpp
    model/parts/records/test_complements.cpp
    model/parts/records/test_cones.cpp
    model/parts/records/test_constraints.cpp
    model/parts/records/test_indicators.cpp
    model/parts/records/test_objectives.cpp
    model/parts/records/test_special_ordered_sets.cpp
    model/parts/records/test_variables.cpp
    model/parts/test_tolerances.cpp
    model/session/test_routes.cpp
    model/session/test_session.cpp
    model/session/test_stream.cpp
    model/session/test_timer.cpp
    model/session/test_warnings.cpp
    model/test_gather.cpp)
target_include_directories(test_model PRIVATE core/utils model/utils utils)
target_link_libraries(test_model PRIVATE biyos_internal)

biyos_add_test(test_io
    io/mps/test_faults.cpp
    io/mps/test_fixed.cpp
    io/mps/test_leftovers.cpp
    io/mps/test_mps.cpp
    io/mps/test_refusals.cpp
    io/mps/test_structure.cpp
    io/mps/test_write.cpp
    io/native/test_faults.cpp
    io/native/test_native.cpp
    io/native/test_random.cpp
    io/native/test_writes.cpp
    io/native/test_words.cpp
    io/nl/test_binary.cpp
    io/nl/test_faults.cpp
    io/nl/test_limits.cpp
    io/nl/test_nl.cpp
    io/nl/test_refusals.cpp
    io/nl/test_write.cpp
    io/test_lines.cpp)
target_include_directories(test_io PRIVATE io/mps/utils io/native/utils io/nl/utils model/utils utils)
target_link_libraries(test_io PRIVATE biyos_internal)

biyos_add_test(test_capi
    capi/env/events/test_events.cpp
    capi/env/events/test_format.cpp
    capi/env/events/test_spans.cpp
    capi/env/test_env.cpp
    capi/env/test_firewall.cpp
    capi/env/test_interrupt.cpp
    capi/env/test_options.cpp
    capi/env/test_threads.cpp
    capi/env/test_version.cpp
    capi/evaluation/mixed.cpp
    capi/evaluation/test_cache.cpp
    capi/evaluation/test_callbacks.cpp
    capi/evaluation/test_evaluation.cpp
    capi/evaluation/test_patterns.cpp
    capi/linear/test_adders.cpp
    capi/linear/test_constants.cpp
    capi/linear/test_functions.cpp
    capi/linear/test_linear.cpp
    capi/linear/test_terms.cpp
    capi/model/test_compare.cpp
    capi/model/test_copy.cpp
    capi/model/test_counts.cpp
    capi/model/test_delete.cpp
    capi/model/test_files.cpp
    capi/model/test_model.cpp
    capi/model/test_names.cpp
    capi/model/test_rebuild.cpp
    capi/nonlinear/test_callbacks.cpp
    capi/nonlinear/test_expressions.cpp
    capi/nonlinear/test_lists.cpp
    capi/nonlinear/test_pairs.cpp
    capi/nonlinear/test_piecewise.cpp
    capi/nonlinear/test_products.cpp
    capi/nonlinear/test_quadratic.cpp
    capi/records/test_complements.cpp
    capi/records/test_cones.cpp
    capi/records/test_constraints.cpp
    capi/records/test_indicators.cpp
    capi/records/test_objectives.cpp
    capi/records/test_ranges.cpp
    capi/records/test_special_ordered_sets.cpp
    capi/utils/sample.cpp)
target_include_directories(test_capi PRIVATE capi/utils io/nl/utils utils)
target_link_libraries(test_capi PRIVATE biyos::biyos biyos_internal)

add_executable(test_capi_c capi/test_header.c)
target_link_libraries(test_capi_c PRIVATE biyos::biyos)
set_target_properties(test_capi_c PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF)
biyos_set_warnings(test_capi_c)
add_test(NAME CApiTest.CompilesAsC99 COMMAND test_capi_c)
