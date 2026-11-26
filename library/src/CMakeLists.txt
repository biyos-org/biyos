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
    core/events/stream.cpp
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
    core/text/bignum.cpp
    core/text/chars.cpp
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
    expr/curvature/segments.cpp
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
    expr/graph/points.cpp
    expr/graph/uses.cpp
    expr/hessian/elements.cpp
    expr/hessian/hessian.cpp
    expr/hessian/sweep.cpp
    expr/hessian/tangents.cpp
    expr/intervals/elementary.cpp
    expr/intervals/enclosure.cpp
    expr/intervals/interval.cpp
    expr/intervals/periodic.cpp
    expr/intervals/piecewise.cpp
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
    io/directory.cpp
    io/draft/column_shape.cpp
    io/draft/draft.cpp
    io/draft/labels.cpp
    io/draft/row_shape.cpp
    io/draft/sketch.cpp
    io/draft/support.cpp
    io/files.cpp
    io/leftovers.cpp
    io/lines.cpp
    io/mps/mps.cpp
    io/mps/print.cpp
    io/mps/readers/bounds.cpp
    io/mps/readers/columns.cpp
    io/mps/readers/rows.cpp
    io/mps/readers/values.cpp
    io/mps/readers/vectors.cpp
    io/mps/sections.cpp
    io/records.cpp
    io/report.cpp
    model/attributes/names.cpp
    model/attributes/naming.cpp
    model/attributes/records.cpp
    model/attributes/tolerances.cpp
    model/compare/fields.cpp
    model/compare/match.cpp
    model/compare/records.cpp
    model/compare/tables.cpp
    model/compressed.cpp
    model/evaluation/callbacks.cpp
    model/evaluation/evaluation.cpp
    model/evaluation/hessian.cpp
    model/evaluation/layout.cpp
    model/expressions/callbacks.cpp
    model/expressions/expressions.cpp
    model/expressions/nodes.cpp
    model/expressions/patterns.cpp
    model/expressions/postfix.cpp
    model/expressions/users.cpp
    model/functions/attach.cpp
    model/functions/functions.cpp
    model/functions/merge.cpp
    model/functions/pairs.cpp
    model/functions/quadratic.cpp
    model/functions/terms.cpp
    model/functions/transpose.cpp
    model/model.cpp
    model/records/complements.cpp
    model/records/cones.cpp
    model/records/constraints.cpp
    model/records/indicators.cpp
    model/records/objectives.cpp
    model/records/special_ordered_sets.cpp
    model/records/variables.cpp
    model/session/session.cpp
    model/utils/checks.cpp
    model/utils/renumber.cpp)

target_include_directories(biyos_internal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
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
    capi/env/env.cpp
    capi/env/events.cpp
    capi/env/format.cpp
    capi/env/options.cpp
    capi/env/version.cpp
    capi/evaluation/cache.cpp
    capi/evaluation/evaluate.cpp
    capi/evaluation/patterns.cpp
    capi/linear/columns.cpp
    capi/linear/functions.cpp
    capi/linear/linear.cpp
    capi/linear/rows.cpp
    capi/model/deletion.cpp
    capi/model/files.cpp
    capi/model/model.cpp
    capi/model/names.cpp
    capi/model/selection.cpp
    capi/nonlinear/callbacks.cpp
    capi/nonlinear/expressions.cpp
    capi/nonlinear/pairs.cpp
    capi/nonlinear/piecewise.cpp
    capi/nonlinear/quadratic.cpp
    capi/records/complements.cpp
    capi/records/cones.cpp
    capi/records/constraints.cpp
    capi/records/indicators.cpp
    capi/records/objectives.cpp
    capi/records/special_ordered_sets.cpp
    capi/utils/arrays.cpp)
add_library(biyos::biyos ALIAS biyos)

target_sources(biyos PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES ${PROJECT_SOURCE_DIR}/include/biyos/biyos.h)
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
