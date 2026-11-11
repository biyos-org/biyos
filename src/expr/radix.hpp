#pragma once

#include "core/index.hpp"
#include "expr/reach.hpp"

#include <vector>

namespace biyos
{

class Radix
{
  public:
    void sort(std::vector<Range> &items);

  private:
    void pass(const std::vector<Range> &items, Offset low, int shift);

    std::vector<Range> spare_;
};

}
