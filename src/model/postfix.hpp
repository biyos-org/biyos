#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"

#include <span>
#include <vector>

namespace biyos
{

struct Postfix
{
    std::span<const Offset> starts;
    std::span<const int> codes;
    std::span<const Offset> arguments;
    std::span<const double> values;
};

Offset n_values(std::span<const int> codes, std::span<const Offset> arguments);
std::vector<Offset> decode(Graph &graph, Index variables, const Postfix &postfix);

}
