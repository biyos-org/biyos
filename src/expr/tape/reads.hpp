#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/layout.hpp"
#include "expr/tape/tape.hpp"

#include <span>

namespace biyos
{

void read_children(const Graph &graph,
                   const Layout &layout,
                   std::span<const Offset> outputs,
                   Block &block);
void read_routes(const Reach &reach, Block &block);

}
