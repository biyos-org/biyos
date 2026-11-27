add_library(biyos_internal STATIC
    core/env/check.cpp
    core/env/env.cpp
    core/env/error.cpp
    core/env/pool.cpp
    core/env/status.cpp
    core/env/thread.cpp
    core/env/version.cpp
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
    core/text/bignum.cpp
    core/text/chars.cpp
    core/text/strings.cpp
    core/text/utf8.cpp
    core/timing/clock.cpp
    core/timing/deadline.cpp
    core/timing/timer.cpp
    expr/analysis/curvature/arithmetic.cpp
    expr/analysis/curvature/bends.cpp
    expr/analysis/curvature/curvature.cpp
    expr/analysis/curvature/exponents.cpp
    expr/analysis/curvature/logic.cpp
    expr/analysis/curvature/segments.cpp
    expr/analysis/curvature/shape.cpp
    expr/analysis/curvature/waves.cpp
    expr/analysis/intervals/elementary.cpp
    expr/analysis/intervals/enclosure.cpp
    expr/analysis/intervals/interval.cpp
    expr/analysis/intervals/logic.cpp
    expr/analysis/intervals/periodic.cpp
    expr/analysis/intervals/piecewise.cpp
    expr/analysis/intervals/powers.cpp
    expr/analysis/intervals/rounding.cpp
    expr/evaluation/derivatives/derivative.cpp
    expr/evaluation/derivatives/partials.cpp
    expr/evaluation/derivatives/reverse.cpp
    expr/evaluation/derivatives/value.cpp
    expr/evaluation/hessian/elements.cpp
    expr/evaluation/hessian/hessian.cpp
    expr/evaluation/hessian/pattern.cpp
    expr/evaluation/hessian/sweep.cpp
    expr/evaluation/hessian/tangents.cpp
    expr/evaluation/threads/levels.cpp
    expr/evaluation/threads/sweeps.cpp
    expr/evaluation/threads/team.cpp
    expr/forms/canonical.cpp
    expr/forms/terms.cpp
    expr/graph/builder.cpp
    expr/graph/compact.cpp
    expr/graph/graph.cpp
    expr/graph/node.cpp
    expr/graph/parents.cpp
    expr/graph/points.cpp
    expr/graph/sharing.cpp
    expr/graph/uses.cpp
    expr/sparsity/memo.cpp
    expr/sparsity/radix.cpp
    expr/sparsity/reach.cpp
    expr/tape/layout.cpp
    expr/tape/locator.cpp
    expr/tape/plan.cpp
    expr/tape/reads.cpp
    expr/tape/tape.cpp
    io/draft/column_shape.cpp
    io/draft/draft.cpp
    io/draft/labels.cpp
    io/draft/row_shape.cpp
    io/draft/sketch.cpp
    io/draft/structure.cpp
    io/draft/support.cpp
    io/files.cpp
    io/mps/mps.cpp
    io/mps/print.cpp
    io/mps/readers/bounds.cpp
    io/mps/readers/columns.cpp
    io/mps/readers/indicators.cpp
    io/mps/readers/rows.cpp
    io/mps/readers/sets.cpp
    io/mps/readers/values.cpp
    io/mps/readers/vectors.cpp
    io/mps/sections.cpp
    io/nl/bodies.cpp
    io/nl/expressions.cpp
    io/nl/header.cpp
    io/nl/nl.cpp
    io/nl/operators.cpp
    io/nl/pieces.cpp
    io/nl/tokens.cpp
    io/native/native.cpp
    io/native/parts.cpp
    io/native/print.cpp
    io/native/readers/attributes.cpp
    io/native/readers/functions.cpp
    io/native/readers/records.cpp
    io/native/utils/body.cpp
    io/native/utils/escape.cpp
    io/native/utils/record.cpp
    io/native/writers/attributes.cpp
    io/native/writers/functions.cpp
    io/native/writers/records.cpp
    io/utils/codes.cpp
    io/utils/directory.cpp
    io/utils/leftovers.cpp
    io/utils/lines.cpp
    io/utils/records.cpp
    io/utils/report.cpp
    model/compare/compare.cpp
    model/compare/fields.cpp
    model/compare/match.cpp
    model/compare/records.cpp
    model/compare/tables.cpp
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
    model/model.cpp
    model/parts/attributes/names.cpp
    model/parts/attributes/naming.cpp
    model/parts/attributes/records.cpp
    model/parts/attributes/tolerances.cpp
    model/parts/functions/attach.cpp
    model/parts/functions/functions.cpp
    model/parts/functions/merge.cpp
    model/parts/functions/pairs.cpp
    model/parts/functions/quadratic.cpp
    model/parts/functions/terms.cpp
    model/parts/functions/transpose.cpp
    model/parts/records/complements.cpp
    model/parts/records/cones.cpp
    model/parts/records/constraints.cpp
    model/parts/records/indicators.cpp
    model/parts/records/objectives.cpp
    model/parts/records/special_ordered_sets.cpp
    model/parts/records/variables.cpp
    model/session/session.cpp
    model/utils/checks.cpp
    model/utils/compressed.cpp
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
    capi/model/compare.cpp
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
