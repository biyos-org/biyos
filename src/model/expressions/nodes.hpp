#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"

#include <span>

namespace biyos
{

Operator to_operator(int op);
Offset n_values(int op, Offset n);
Offset append_node(Graph &graph,
                   int op,
                   std::span<const Offset> children,
                   std::span<const Real> values);

}
