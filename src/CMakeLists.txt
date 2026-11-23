add_library(biyos_internal STATIC
    core/check.cpp
    core/clock.cpp
    core/deadline.cpp
    core/env.cpp
    core/error.cpp
    core/events.cpp
    core/format.cpp
    core/lookup.cpp
    core/option_lookup.cpp
    core/option_table.cpp
    core/option_text.cpp
    core/option_value.cpp
    core/options.cpp
    core/settings.cpp
    core/status.cpp
    core/strings.cpp
    core/timer.cpp
    core/tolerances.cpp
    core/usage.cpp
    core/utf8.cpp
    core/version.cpp
    expr/arithmetic.cpp
    expr/bends.cpp
    expr/builder.cpp
    expr/canonical.cpp
    expr/compact.cpp
    expr/curvature.cpp
    expr/derivative.cpp
    expr/elementary.cpp
    expr/enclosure.cpp
    expr/exponents.cpp
    expr/graph.cpp
    expr/interval.cpp
    expr/layout.cpp
    expr/locator.cpp
    expr/memo.cpp
    expr/node.cpp
    expr/parents.cpp
    expr/partials.cpp
    expr/periodic.cpp
    expr/plan.cpp
    expr/powers.cpp
    expr/radix.cpp
    expr/reach.cpp
    expr/reads.cpp
    expr/reverse.cpp
    expr/rounding.cpp
    expr/shape.cpp
    expr/tape.cpp
    expr/terms.cpp
    expr/uses.cpp
    expr/value.cpp
    expr/waves.cpp
    model/attach.cpp
    model/callbacks.cpp
    model/checks.cpp
    model/compare.cpp
    model/complements.cpp
    model/compressed.cpp
    model/cones.cpp
    model/constraints.cpp
    model/expressions.cpp
    model/fields.cpp
    model/functions.cpp
    model/indicators.cpp
    model/match.cpp
    model/merge.cpp
    model/model.cpp
    model/names.cpp
    model/naming.cpp
    model/nodes.cpp
    model/objectives.cpp
    model/overrides.cpp
    model/pairs.cpp
    model/patterns.cpp
    model/postfix.cpp
    model/quadratic.cpp
    model/renumber.cpp
    model/sos.cpp
    model/terms.cpp
    model/transpose.cpp
    core/thread.cpp
    core/records.cpp)

target_include_directories(biyos_internal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include)
target_compile_features(biyos_internal PUBLIC cxx_std_20)
target_compile_definitions(biyos_internal PRIVATE
    BIYOS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BIYOS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BIYOS_VERSION_PATCH=${PROJECT_VERSION_PATCH})
set_target_properties(biyos_internal PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
biyos_set_warnings(biyos_internal)

add_library(biyos
    capi/arrays.cpp
    capi/callbacks.cpp
    capi/columns.cpp
    capi/complements.cpp
    capi/cones.cpp
    capi/constraints.cpp
    capi/deletion.cpp
    capi/env.cpp
    capi/events.cpp
    capi/expressions.cpp
    capi/format.cpp
    capi/functions.cpp
    capi/indicators.cpp
    capi/linear.cpp
    capi/model.cpp
    capi/names.cpp
    capi/objectives.cpp
    capi/options.cpp
    capi/pairs.cpp
    capi/quadratic.cpp
    capi/rows.cpp
    capi/selection.cpp
    capi/sos.cpp
    capi/version.cpp)
add_library(biyos::biyos ALIAS biyos)

target_include_directories(biyos PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(biyos PRIVATE biyos_internal)
target_compile_definitions(biyos PRIVATE BIYOS_BUILDING)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(biyos PUBLIC BIYOS_STATIC)
endif()
set_target_properties(biyos PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})
biyos_set_warnings(biyos)

find_package(Threads REQUIRED)
target_link_libraries(biyos_internal PUBLIC Threads::Threads)
