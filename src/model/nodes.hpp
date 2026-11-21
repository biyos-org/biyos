#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"

#include <span>

namespace biyos
{

Operator to_operator(int op);
Offset n_values(int op, Offset n);
Offset append_node(Graph &graph,
                   int op,
                   std::span<const Offset> children,
                   std::span<const double> values);

}
