add_library(biyos_internal STATIC
    core/check.cpp
    core/env.cpp
    core/error.cpp
    core/events/deliveries.cpp
    core/events/event.cpp
    core/events/events.cpp
    core/events/format.cpp
    core/events/hooks.cpp
    core/events/records.cpp
    core/memory/usage.cpp
    core/numeric/tolerances.cpp
    core/options/catalog.cpp
    core/options/lookup.cpp
    core/options/options.cpp
    core/options/settings.cpp
    core/options/table.cpp
    core/options/text.cpp
    core/options/value.cpp
    core/status.cpp
    core/text/strings.cpp
    core/text/utf8.cpp
    core/thread.cpp
    core/timing/clock.cpp
    core/timing/deadline.cpp
    core/timing/timer.cpp
    core/version.cpp
    expr/curvature/arithmetic.cpp
    expr/curvature/bends.cpp
    expr/curvature/curvature.cpp
    expr/curvature/exponents.cpp
    expr/curvature/shape.cpp
    expr/curvature/waves.cpp
    expr/derivatives/derivative.cpp
    expr/derivatives/partials.cpp
    expr/derivatives/reverse.cpp
    expr/derivatives/value.cpp
    expr/forms/canonical.cpp
    expr/forms/terms.cpp
    expr/graph/builder.cpp
    expr/graph/compact.cpp
    expr/graph/graph.cpp
    expr/graph/node.cpp
    expr/graph/parents.cpp
    expr/graph/uses.cpp
    expr/intervals/elementary.cpp
    expr/intervals/enclosure.cpp
    expr/intervals/interval.cpp
    expr/intervals/periodic.cpp
    expr/intervals/powers.cpp
    expr/intervals/rounding.cpp
    expr/sparsity/memo.cpp
    expr/sparsity/radix.cpp
    expr/sparsity/reach.cpp
    expr/tape/layout.cpp
    expr/tape/locator.cpp
    expr/tape/plan.cpp
    expr/tape/reads.cpp
    expr/tape/tape.cpp
    io/bounds.cpp
    io/column_shape.cpp
    io/columns.cpp
    io/directory.cpp
    io/draft.cpp
    io/files.cpp
    io/labels.cpp
    io/leftovers.cpp
    io/lines.cpp
    io/mps.cpp
    io/print.cpp
    io/records.cpp
    io/report.cpp
    io/row_shape.cpp
    io/rows.cpp
    io/sections.cpp
    io/sketch.cpp
    io/support.cpp
    io/values.cpp
    io/vectors.cpp
    model/attach.cpp
    model/callbacks.cpp
    model/checks.cpp
    model/compare.cpp
    model/compressed.cpp
    model/expressions.cpp
    model/fields.cpp
    model/functions.cpp
    model/match.cpp
    model/merge.cpp
    model/model.cpp
    model/names.cpp
    model/naming.cpp
    model/nodes.cpp
    model/overrides.cpp
    model/pairs.cpp
    model/patterns.cpp
    model/postfix.cpp
    model/quadratic.cpp
    model/records/complements.cpp
    model/records/cones.cpp
    model/records/constraints.cpp
    model/records/indicators.cpp
    model/records/objectives.cpp
    model/records/sos.cpp
    model/records/variables.cpp
    model/renumber.cpp
    model/terms.cpp
    model/tolerances.cpp
    model/transpose.cpp
    model/users.cpp)

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
    capi/files.cpp
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
