#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/layout.hpp"
#include "expr/reach.hpp"
#include "expr/tape.hpp"

#include <span>

namespace biyos
{

void read_children(const Graph &graph,
                   const Layout &layout,
                   std::span<const Offset> outputs,
                   Block &block);
void read_routes(const Reach &reach, Block &block);

}
