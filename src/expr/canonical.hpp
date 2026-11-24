#pragma once

#include "core/index.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/uses.hpp"
#include "expr/terms.hpp"

#include <vector>

namespace biyos
{

constexpr Offset kNoRemainder = -1;

struct Parts
{
    double constant;
    std::vector<LinearTerm> linear;
    std::vector<QuadraticTerm> quadratic;
    Offset remainder;
};

Parts split(Graph &graph, const Uses &uses, Offset node);

}
