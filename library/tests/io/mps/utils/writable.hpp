#pragma once

#include "../../../model/utils/sample.hpp"

#include "model/model.hpp"

namespace biyos::test
{

inline Parts writable()
{
    Parts parts = sample();
    parts.quadratic = {.functions = {}, .starts = {0}, .terms = {}};
    parts.expressions = {};
    parts.objectives = {default_objective(2, 0)};
    parts.indicators = {};
    parts.special_ordered_sets = {.kinds = {}, .starts = {0}, .members = {}};
    parts.cones = {.kinds = {},
                   .starts = {0},
                   .functions = {},
                   .parameter_starts = {0},
                   .parameters = {}};
    parts.complements = {};
    parts.indicator_names = {};
    parts.special_ordered_set_names = {};
    parts.cone_names = {};
    parts.complement_names = {};
    return parts;
}

}
