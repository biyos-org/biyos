#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"

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

struct Encoding
{
    std::vector<Offset> starts;
    std::vector<int> codes;
    std::vector<Offset> arguments;
    std::vector<double> values;
};

Offset n_values(std::span<const int> codes, std::span<const Offset> arguments);
std::vector<Offset> decode(Graph &graph, Index variables, const Postfix &postfix);
Encoding encode(const Graph &graph, std::span<const Offset> roots);

}
