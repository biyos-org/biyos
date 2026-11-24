#pragma once

#include "core/index.hpp"
#include "expr/graph/graph.hpp"

#include <cstdint>
#include <vector>

namespace biyos
{

class Uses
{
  public:
    Uses() = default;
    explicit Uses(const Graph &graph);

    Offset size() const noexcept;
    bool shared(Offset node) const noexcept;
    void extend(const Graph &graph);

  private:
    std::vector<std::uint8_t> counts_;
};

}
