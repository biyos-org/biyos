#pragma once

#include "core/numbers.hpp"
#include "expr/sparsity/radix.hpp"
#include "expr/sparsity/reach.hpp"

#include <span>
#include <vector>

namespace biyos
{

void settle(std::vector<Index> &items, std::vector<Index> &into);
void settle(std::vector<Range> &items, std::vector<Range> &into);
void join(std::span<const Range> items, std::vector<Range> &into);

class Joiner
{
  public:
    void start(std::span<const Range> own);
    void push_back(Range run);
    void finish(Radix &radix, std::vector<Range> &into);

  private:
    bool follows(Range run) const noexcept;

    std::vector<Range> runs_;
    bool sorted_ = true;
};

}
