#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/parents.hpp"
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

Parts split(Graph &graph, const Parents &parents, Offset node);

}
