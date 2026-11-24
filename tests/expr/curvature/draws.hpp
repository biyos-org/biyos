#pragma once

#include "expr/graph.hpp"
#include "sampling.hpp"

#include <cstddef>

namespace biyos::test
{

Graph draw_graph(Engine &engine, std::size_t size);
Box draw_box(Engine &engine);

}
