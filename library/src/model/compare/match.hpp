#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"

#include <string>
#include <utility>
#include <vector>

namespace biyos
{

class Matching
{
  public:
    Matching(const Graph &a, const Graph &b);

    std::string difference(Offset a, Offset b);

  private:
    std::string visit(Offset a, Offset b);
    std::string node_difference(Offset a, Offset b) const;

    const Graph &a_;
    const Graph &b_;
    std::vector<Offset> to_b_;
    std::vector<Offset> to_a_;
    std::vector<std::pair<Offset, Offset>> pending_;
};

}
