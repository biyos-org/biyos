#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"

#include <span>
#include <vector>

namespace biyos
{

class Parents
{
  public:
    explicit Parents(const Graph &graph);

    Offset size() const noexcept;
    std::span<const Offset> of(Offset node) const noexcept;

  private:
    std::vector<Offset> starts_;
    std::vector<Offset> parents_;
};

}
