#pragma once

#include "core/env/env.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/options/lookup.hpp"
#include "expr/graph/graph.hpp"

#include <span>

namespace biyos::test
{

inline Tolerances default_tolerances()
{
    Env env;
    return Tolerances(Lookup(env.options()));
}

void evaluate(const Graph &graph, std::span<const Real> point, std::span<Real> values);

}
