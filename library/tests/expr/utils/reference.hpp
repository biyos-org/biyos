#pragma once

#include "core/env/env.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/options/lookup.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"

#include <span>

namespace biyos::test
{

inline Tolerances default_tolerances()
{
    Env env;
    return Tolerances(Lookup(env.options()));
}

Real comparison(Operator op, Real a, Real b);

Real connective(Operator op, std::span<const Real> conditions);

void evaluate(const Graph &graph, std::span<const Real> point, std::span<Real> values);

}
