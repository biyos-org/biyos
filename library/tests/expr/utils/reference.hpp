#pragma once

#include "core/env/env.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/options/lookup.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"

#include <span>
#include <vector>

namespace biyos::test
{

inline Tolerances default_tolerances()
{
    Env env;
    return Tolerances(Lookup(env.options()));
}

template <typename Value>
std::vector<Real> children_of(const Graph &graph, Offset node, Value value)
{
    std::vector<Real> values;
    for (const Offset child : graph.children(node))
    {
        values.push_back(value(child));
    }
    return values;
}

Real comparison(Operator op, Real a, Real b);

Real connective(Operator op, std::span<const Real> conditions);

Real select(Real condition, Real then, Real otherwise);

void evaluate(const Graph &graph, std::span<const Real> point, std::span<Real> values);

}
